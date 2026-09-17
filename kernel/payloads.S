/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

.section .rodata.apps, "a"
.global app0_start, app0_end, app1_start, app1_end, app2_start, app2_end
app0_start:
.incbin APP0_BIN
app0_end:
app1_start:
.incbin APP1_BIN
app1_end:
app2_start:
.incbin APP2_BIN
app2_end:
.section .note.GNU-stack, "", %progbits