# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from tests.support import verifies
from tools.check import ROOT, execute, load_requirements, summarize, validate_links, validate_register


def register(status="implemented"):
    return {
        "schema_version": 1,
        "requirements": [{
            "id": "REQ-TEST-001",
            "title": "Example",
            "statement": "The example shall pass.",
            "status": status,
            "acceptance": ["The test passes."],
        }],
    }


class RegisterTests(unittest.TestCase):
    @verifies("REQ-TOOL-001")
    def test_valid_states_and_project_register(self):
        for status in ("planned", "implemented", "retired"):
            with self.subTest(status=status):
                self.assertEqual(validate_register(register(status))["REQ-TEST-001"]["status"], status)
        self.assertTrue(load_requirements(ROOT / "docs/requirements.toml"))

    @verifies("REQ-TOOL-001")
    def test_invalid_register_shape_and_versions(self):
        candidates = [None, [], {}, {**register(), "extra": True}]
        candidates += [{**register(), "schema_version": version} for version in (True, 1.0, 2, "1")]
        candidates += [{**register(), "requirements": rows} for rows in ([], {}, [None], [{}])]
        for candidate in candidates:
            with self.subTest(candidate=candidate), self.assertRaises(ValueError):
                validate_register(candidate)

    @verifies("REQ-TOOL-001")
    def test_invalid_requirement_fields(self):
        invalid = {
            "id": (None, 1, "REQ-test-001", "REQ-TEST-01", "REQ-TEST-001\n"),
            "title": (None, "", "  "),
            "statement": (None, "", "\t"),
            "status": ("passed", None, []),
            "acceptance": ([], "text", [""], [None], ["good", " "]),
        }
        for field, values in invalid.items():
            for value in values:
                candidate = register()
                candidate["requirements"][0][field] = value
                with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                    validate_register(candidate)
        for field in register()["requirements"][0]:
            candidate = register()
            del candidate["requirements"][0][field]
            with self.subTest(missing=field), self.assertRaises(ValueError):
                validate_register(candidate)
        candidate = register()
        candidate["requirements"][0]["extra"] = True
        with self.assertRaises(ValueError):
            validate_register(candidate)

    @verifies("REQ-TOOL-001")
    def test_duplicate_ids(self):
        candidate = register()
        candidate["requirements"].append(copy.deepcopy(candidate["requirements"][0]))
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            validate_register(candidate)

    @verifies("REQ-TOOL-001")
    def test_invalid_toml(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "requirements.toml"
            path.write_text("schema_version = [", encoding="utf-8")
            with self.assertRaises(ValueError):
                load_requirements(path)


def example_suite(outcome="passed", references=("REQ-TEST-001",)):
    class Example(unittest.TestCase):
        @verifies(*references)
        def test_example(self):
            if outcome == "failed":
                self.fail("Intentional failure")
            elif outcome == "error":
                raise RuntimeError("Intentional error")
            elif outcome == "skipped":
                self.skipTest("Intentional skip")
            elif outcome in ("subtest_failure", "subtest_error", "subtest_skip"):
                with self.subTest(part="first"):
                    if outcome == "subtest_failure":
                        self.fail("Intentional subtest failure")
                    elif outcome == "subtest_error":
                        raise RuntimeError("Intentional subtest error")
                    else:
                        self.skipTest("Intentional subtest skip")
                with self.subTest(part="second"):
                    self.assertTrue(True)
            elif outcome == "expected_failure":
                self.fail("Intentional expected failure")

    if outcome in ("expected_failure", "unexpected_success"):
        unittest.expectedFailure(Example.test_example)
    if outcome == "fixture_error":
        def broken_setup(cls):
            raise RuntimeError("Intentional fixture error")
        Example.setUpClass = classmethod(broken_setup)
    if outcome == "teardown_error":
        def broken_teardown(cls):
            raise RuntimeError("Intentional teardown error")
        Example.tearDownClass = classmethod(broken_teardown)
    return unittest.defaultTestLoader.loadTestsFromTestCase(Example)


class LinkTests(unittest.TestCase):
    @verifies("REQ-TOOL-002")
    def test_missing_unknown_duplicate_and_retired_links(self):
        requirements = validate_register(register())
        for references in ((), ("REQ-NONE-001",), (None,), ([],),
                           ("REQ-TEST-001", "REQ-TEST-001")):
            with self.subTest(references=references), self.assertRaises(ValueError):
                validate_links(requirements, example_suite(references=references))
        with self.assertRaisesRegex(ValueError, "retired"):
            validate_links(validate_register(register("retired")), example_suite())
        suite = example_suite()
        next(iter(suite)).test_example.__func__.requirement_ids = "REQ-TEST-001"
        with self.assertRaisesRegex(ValueError, "missing"):
            validate_links(requirements, suite)

    @verifies("REQ-TOOL-002")
    def test_empty_duplicate_and_uncovered_tests(self):
        requirements = validate_register(register())
        with self.assertRaisesRegex(ValueError, "No tests"):
            validate_links(requirements, unittest.TestSuite())
        suite = example_suite()
        suite.addTest(next(iter(suite)))
        with self.assertRaisesRegex(ValueError, "Duplicate test"):
            validate_links(requirements, suite)
        candidate = register()
        candidate["requirements"].append({**candidate["requirements"][0], "id": "REQ-TEST-002"})
        with self.assertRaisesRegex(ValueError, "without tests"):
            validate_links(validate_register(candidate), example_suite())

    @verifies("REQ-TOOL-002")
    def test_many_to_many_and_pending_without_execution(self):
        candidate = register()
        candidate["requirements"] += [
            {**candidate["requirements"][0], "id": "REQ-TEST-002"},
            {**candidate["requirements"][0], "id": "REQ-TEST-003", "status": "planned"},
            {**candidate["requirements"][0], "id": "REQ-TEST-004", "status": "retired"},
        ]
        requirements = validate_register(candidate)
        suite = example_suite("failed", ("REQ-TEST-001", "REQ-TEST-002"))
        second = example_suite("failed", ("REQ-TEST-001", "REQ-TEST-002"))
        next(iter(second)).id = lambda: "second.test_example"
        suite.addTest(second)
        links = validate_links(requirements, suite)
        self.assertEqual(len(links), 2)
        self.assertTrue(all(len(references) == 2 for references in links.values()))
        self.assertEqual(suite.countTestCases(), 2)


class OutcomeTests(unittest.TestCase):
    @verifies("REQ-TOOL-003")
    def test_suite_executes_once(self):
        calls = []

        class CountingTest(unittest.TestCase):
            @verifies("REQ-TEST-001")
            def test_count(self):
                calls.append("executed")

        suite = unittest.defaultTestLoader.loadTestsFromTestCase(CountingTest)
        report = execute(validate_register(register()), suite)
        self.assertTrue(report["passed"])
        self.assertEqual(calls, ["executed"])

    @verifies("REQ-TOOL-003")
    def test_actual_outcomes_and_subtests(self):
        expected = {
            "passed": ("passed", "passed"),
            "failed": ("failed", "failed"),
            "error": ("error", "failed"),
            "skipped": ("skipped", "incomplete"),
            "expected_failure": ("expected_failure", "incomplete"),
            "unexpected_success": ("unexpected_success", "failed"),
            "subtest_failure": ("failed", "failed"),
            "subtest_error": ("error", "failed"),
            "subtest_skip": ("skipped", "incomplete"),
            "fixture_error": ("not_run", "incomplete"),
            "teardown_error": ("passed", "incomplete"),
        }
        for scenario, (test_outcome, requirement_outcome) in expected.items():
            with self.subTest(scenario=scenario):
                report = execute(validate_register(register()), example_suite(scenario))
                self.assertEqual(report["passed"], scenario == "passed")
                record = next(iter(report["tests"].values()))
                self.assertEqual(record["outcome"], test_outcome)
                self.assertEqual(report["requirements"]["REQ-TEST-001"]["outcome"], requirement_outcome)
                if scenario != "passed":
                    self.assertTrue(record["diagnostics"] or report["errors"])

    @verifies("REQ-TOOL-003")
    def test_planned_retired_and_incomplete_are_not_passed(self):
        for status, expected in (("planned", "pending"), ("retired", "retired"),
                                 ("implemented", "incomplete")):
            with self.subTest(status=status):
                summary = summarize(validate_register(register(status)), {})
                self.assertEqual(summary["REQ-TEST-001"]["outcome"], expected)
        report = execute(validate_register(register("planned")), example_suite())
        self.assertTrue(report["passed"])
        self.assertEqual(report["requirements"]["REQ-TEST-001"]["outcome"], "pending")

    @verifies("REQ-TOOL-001", "REQ-TOOL-002", "REQ-TOOL-003")
    def test_process_exit_reports_and_stale_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "docs").mkdir()
            (root / "tests").mkdir()
            (root / "build").mkdir()
            (root / "tests/__init__.py").touch()
            requirement_path = root / "docs/requirements.toml"
            valid_toml = '''schema_version = 1
[[requirements]]
id = "REQ-TEST-001"
title = "Example"
statement = "The example shall pass."
status = "implemented"
acceptance = ["The example passes."]
'''
            test_source = '''import unittest
class Example(unittest.TestCase):
    def test_example(self):
        {body}
    test_example.requirement_ids = ("REQ-TEST-001",)
'''
            scenarios = {
                "passed": "self.assertTrue(True)",
                "failed": "self.fail('intentional')",
                "skipped": "self.skipTest('intentional')",
                "interrupted": "raise KeyboardInterrupt()",
                "import_error": None,
                "import_exit": None,
                "empty": None,
                "invalid_toml": None,
                "unknown_link": "self.assertTrue(True)",
            }
            for scenario, body in scenarios.items():
                with self.subTest(scenario=scenario):
                    requirement_path.write_text("invalid [" if scenario == "invalid_toml" else valid_toml,
                                                encoding="utf-8")
                    source = test_source.format(body=body) if body else ""
                    if scenario == "import_error":
                        source = "raise RuntimeError('intentional import error')"
                    if scenario == "import_exit":
                        source = "raise SystemExit(0)"
                    if scenario == "unknown_link":
                        source = source.replace("REQ-TEST-001", "REQ-NONE-001")
                    (root / "tests/test_example.py").write_text(source, encoding="utf-8")
                    report_path = root / "build/test-results.json"
                    report_path.write_text('{"stale": true, "passed": true}', encoding="utf-8")
                    command = "from pathlib import Path; from tools.check import check; "
                    command += "raise SystemExit(check(Path(__import__('sys').argv[1])))"
                    process = subprocess.run([sys.executable, "-B", "-c", command, str(root)],
                                             cwd=ROOT, capture_output=True, text=True, timeout=15)
                    self.assertEqual(process.returncode, 0 if scenario == "passed" else 1, process.stderr)
                    report = json.loads(report_path.read_text(encoding="utf-8"))
                    self.assertNotIn("stale", report)
                    self.assertEqual(report["passed"], scenario == "passed")
                    self.assertIn("generated_at", report)
                    if scenario == "passed":
                        self.assertEqual(report["requirements"]["REQ-TEST-001"]["outcome"], "passed")
                        self.assertEqual(len(report["tests"]), 1)
                    elif scenario in ("failed", "skipped"):
                        self.assertEqual(next(iter(report["tests"].values()))["outcome"], scenario)
                    else:
                        self.assertTrue(report["errors"])