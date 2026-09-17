# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import json
import re
import sys
import tomllib
import unittest
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REQUIREMENT_ID = re.compile(r"REQ-[A-Z][A-Z0-9]*-[0-9]{3}")
sys.path.insert(0, str(ROOT))


def nonblank(value):
    return isinstance(value, str) and bool(value.strip())


def validate_register(data):
    if not isinstance(data, dict) or set(data) != {"schema_version", "requirements"}:
        raise ValueError("Register must contain only schema_version and requirements")
    if type(data["schema_version"]) is not int or data["schema_version"] != 1:
        raise ValueError("Unsupported schema_version; expected integer 1")
    rows = data["requirements"]
    if not isinstance(rows, list) or not rows:
        raise ValueError("Requirement register must be a nonempty list")
    requirements = {}
    fields = {"id", "title", "statement", "status", "acceptance"}
    for row in rows:
        if not isinstance(row, dict) or set(row) != fields:
            raise ValueError("Each requirement must contain exactly: " + ", ".join(sorted(fields)))
        identifier = row["id"]
        if not isinstance(identifier, str) or not REQUIREMENT_ID.fullmatch(identifier):
            raise ValueError(f"Invalid requirement ID: {identifier!r}")
        if identifier in requirements:
            raise ValueError(f"Duplicate requirement ID: {identifier}")
        if not all(nonblank(row[field]) for field in ("title", "statement")):
            raise ValueError(f"{identifier}: title and statement must be nonblank strings")
        if row["status"] not in ("planned", "implemented", "retired"):
            raise ValueError(f"{identifier}: invalid status")
        acceptance = row["acceptance"]
        if not isinstance(acceptance, list) or not acceptance or not all(map(nonblank, acceptance)):
            raise ValueError(f"{identifier}: acceptance must be a nonempty list of nonblank strings")
        requirements[identifier] = row
    return requirements


def load_requirements(path):
    with path.open("rb") as source:
        return validate_register(tomllib.load(source))


def test_cases(suite):
    for item in suite:
        if isinstance(item, unittest.TestSuite):
            yield from test_cases(item)
        else:
            yield item


def validate_links(requirements, suite):
    links = {}
    for test in test_cases(suite):
        identifier = test.id()
        if identifier in links:
            raise ValueError(f"Duplicate test ID: {identifier}")
        method = getattr(test, test._testMethodName)
        references = getattr(method, "requirement_ids", ())
        if not isinstance(references, tuple) or not references:
            raise ValueError(f"{identifier}: missing requirement links")
        for reference in references:
            if not isinstance(reference, str) or reference not in requirements:
                raise ValueError(f"{identifier}: unknown requirement {reference!r}")
            if requirements[reference]["status"] == "retired":
                raise ValueError(f"{identifier}: links to retired requirement {reference}")
        if len(set(references)) != len(references):
            raise ValueError(f"{identifier}: duplicate requirement links")
        links[identifier] = list(references)
    if not links:
        raise ValueError("No tests discovered")
    covered = {reference for references in links.values() for reference in references}
    missing = [identifier for identifier, row in requirements.items()
               if row["status"] == "implemented" and identifier not in covered]
    if missing:
        raise ValueError("Implemented requirements without tests: " + ", ".join(missing))
    return links


class TraceResult(unittest.TestResult):
    def __init__(self, links):
        super().__init__()
        self.records = {
            identifier: {"requirements": references, "outcome": "not_run", "diagnostics": []}
            for identifier, references in links.items()
        }
        self.issues = []

    def record(self, test, outcome, diagnostic=None):
        identifier = getattr(test, "test_case", test).id()
        if identifier not in self.records:
            self.issues.append(f"{identifier}: {outcome}\n{diagnostic or ''}")
            return
        record = self.records[identifier]
        priority = {"not_run": 0, "passed": 1, "skipped": 2, "expected_failure": 2,
                    "unexpected_success": 3, "failed": 3, "error": 4}
        if priority[outcome] > priority[record["outcome"]]:
            record["outcome"] = outcome
        if diagnostic:
            record["diagnostics"].append(diagnostic)

    def addSuccess(self, test):
        super().addSuccess(test)
        self.record(test, "passed")

    def addFailure(self, test, error):
        super().addFailure(test, error)
        self.record(test, "failed", self._exc_info_to_string(error, test))

    def addError(self, test, error):
        super().addError(test, error)
        self.record(test, "error", self._exc_info_to_string(error, test))

    def addSkip(self, test, reason):
        super().addSkip(test, reason)
        self.record(test, "skipped", f"{test.id()}: {reason}")

    def addExpectedFailure(self, test, error):
        super().addExpectedFailure(test, error)
        self.record(test, "expected_failure", self._exc_info_to_string(error, test))

    def addUnexpectedSuccess(self, test):
        super().addUnexpectedSuccess(test)
        self.record(test, "unexpected_success", "Expected failure unexpectedly passed")

    def addSubTest(self, test, subtest, error):
        super().addSubTest(test, subtest, error)
        if error is not None:
            outcome = "failed" if issubclass(error[0], test.failureException) else "error"
            self.record(test, outcome, f"{subtest.id()}\n{self._exc_info_to_string(error, test)}")


def summarize(requirements, records):
    summaries = {}
    for identifier, row in requirements.items():
        linked = {test_id: record for test_id, record in records.items()
                  if identifier in record["requirements"]}
        outcomes = [record["outcome"] for record in linked.values()]
        if row["status"] != "implemented":
            outcome = "pending" if row["status"] == "planned" else "retired"
        elif any(value in ("failed", "error", "unexpected_success") for value in outcomes):
            outcome = "failed"
        elif outcomes and all(value == "passed" for value in outcomes):
            outcome = "passed"
        else:
            outcome = "incomplete"
        summaries[identifier] = {"status": row["status"], "outcome": outcome, "tests": list(linked)}
    return summaries


def execute(requirements, suite):
    links = validate_links(requirements, suite)
    result = TraceResult(links)
    suite.run(result)
    passed = not result.issues and all(record["outcome"] == "passed" for record in result.records.values())
    summaries = summarize(requirements, result.records)
    if result.issues:
        for summary in summaries.values():
            if summary["outcome"] == "passed":
                summary["outcome"] = "incomplete"
    return {
        "passed": passed,
        "errors": result.issues,
        "tests": result.records,
        "requirements": summaries,
    }


def check(root=ROOT):
    report_path = root / "build/test-results.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.unlink(missing_ok=True)
    report = {"passed": False, "errors": [], "tests": {}, "requirements": {}}
    requirements = {}
    try:
        requirements = load_requirements(root / "docs/requirements.toml")
        loader = unittest.TestLoader()
        suite = loader.discover(str(root / "tests"), top_level_dir=str(root))
        if loader.errors:
            raise ValueError("Test discovery failed:\n" + "\n".join(loader.errors))
        report.update(execute(requirements, suite))
    except (Exception, KeyboardInterrupt, SystemExit) as error:
        report["errors"].append(f"{type(error).__name__}: {error}")
        report["requirements"] = summarize(requirements, {})
    report["schema_version"] = 1
    report["generated_at"] = datetime.now(timezone.utc).isoformat()
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for identifier, summary in report["requirements"].items():
        print(f"{identifier}: {summary['outcome']} ({len(summary['tests'])} tests)")
    for error in report["errors"]:
        print(error, file=sys.stderr)
    for identifier, record in report["tests"].items():
        if record["outcome"] != "passed":
            print(f"{identifier}: {record['outcome']}", file=sys.stderr)
            for diagnostic in record["diagnostics"]:
                print(diagnostic, file=sys.stderr)
    print(f"{'PASS' if report['passed'] else 'FAIL'}: {len(report['tests'])} tests; {report_path}")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(check())