---
related_code:
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - tests/cli/test_cli_repl_physical_lines.py
  - tests/cmake/cli-repl-physical-line-tests.cmake
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
plan_sources:
  - user: 2026-10-16 active CLI BUG review and root-authorized real-entry RED fixture
tests:
  - tests/cli/test_cli_repl_physical_lines.py
  - tests/cmake/cli-repl-physical-line-tests.cmake
doc_type: testing-guide
---

# REPL physical input lines

The current stdin loop reads at most 1023 bytes per fgets call, strips trailing
CR/LF, and appends a newline for every chunk. A long physical line is therefore
rewritten before submission. A chunk containing only the remaining line ending
can also be mistaken for an empty physical line, submitting incomplete code.
No production repair or actual RED result is claimed by this fixture delivery.

The independent fragment registers six `cli_repl_physical_lines_` cases. It
depends on the existing `zr_vm_cli_executable` target, with no new native target
or replacement implementation. Python launches that actual executable with no
arguments, sends ASCII bytes through stdin, and observes combined stdout/stderr.
The short LF/CRLF controls and the 1023/2046 crossings require a complete unique
string payload to occur contiguously exactly once. The exact-boundary case puts
1023 source bytes before LF; the CRLF split puts 1022 before CR, causing LF to
arrive alone. Both continue an unfinished declaration on the next physical line,
submit it with a genuine blank line, then evaluate the published binding. Every
case also evaluates `7 + 8`, requires its result 15, rejects English error/fatal/
failed diagnostics, and checks process exit zero. Complete payload assertions
remain the primary oracle even if diagnostic localization changes.

Each case has its own process. Timeout handling kills and reaps only that child;
all pipes are drained by communicate. Inputs and captured outputs remain in
memory, with no generated source fixture or source snapshot. TEMP, TMP and TMPDIR
are explicitly set to a fresh per-case directory; Python runs with `-B` and also
passes PYTHONDONTWRITEBYTECODE to its child. Existing case directories are refused.
The test does not delete unknown files produced there.

Root should configure `ZR_VM_REPL_LINE_PYTHON_EXECUTABLE` to
`E:/cargo-targets/zr_vm/cache/python-3.12.10-embed-amd64/python.exe` and
`ZR_VM_REPL_LINE_TEMP_ROOT` to
`E:/cargo-targets/zr_vm/tmp/review-bugfix-20261016/cli-repl-physical-lines`.
The portable default uses the external CMake binary tree. Root owns shared
registration, native build, actual RED/GREEN receipts, and post-exit fingerprint
cleanup. Child manifest is under
`E:/cargo-targets/zr_vm/reports/review-bugfix-20261016/cli-repl-physical-lines`.
No cleanup space or native validation is credited at fixture preparation time.
