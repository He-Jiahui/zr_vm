---
related_code:
  - zr_vm_cli/src/zr_vm_cli/commands/test_command.c
  - zr_vm_cli/src/zr_vm_cli/testing/test_runner.c
  - zr_vm_cli/src/zr_vm_cli/testing/test_process.c
  - zr_vm_parser/include/zr_vm_parser/test_contract.h
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/commands/test_command.c
  - zr_vm_cli/src/zr_vm_cli/testing/test_runner.c
  - zr_vm_cli/src/zr_vm_cli/testing/test_process.c
plan_sources:
  - docs/plans/syntax/2026-07-20-14-test-function-harness-design.md
tests:
  - tests/testing/test_runner.c
  - tests/cli/test_cli_args.c
  - tests/cmake/run_testing_reference.cmake
  - tests/fixtures/projects/testing_reference
doc_type: module-detail
---

# ZR VM Test Command

## Command Surface

`zr_vm test <target>` accepts one `.zr` module or one `.zrp` project. Options
are `--filter <glob>`, `--list`, `--jobs <n>`, and `--timeout <duration>`.
Discovery compiles each source in Test phase and consumes only its typed
TestManifest; it does not scan function text or synthesize a hidden entrypoint.

Stable case ids are sorted as `module::qualifiedName#ordinal(arguments)`. The
same ids drive list output, glob filtering, deterministic seed calculation,
and the exact internal worker selection. Exact worker ids do not interpret
glob characters.

## Execution Model

Cases from one module remain serial. Different module groups may execute in
parallel up to `--jobs`. The outer runner starts a child process per selected
case, captures stdout/stderr, enforces timeout with termination and kill
fallback, and reports pass/fail/skip/timeout/crash separately. The child
recompiles only through the Test-phase path and executes in a fresh global
state with the official `zr.testing` provider.

On Windows, every child argument is quoted with the `CommandLineToArgvW`
backslash/quote rules, stdin is an inheritable `NUL` handle, and launch failure
reports the `CreateProcessA` error code. After an assertion exception is caught,
the worker copies the structured failure and resets the VM thread/call stack
before freeing the compiled case function. This keeps an expected assertion
failure distinct from an isolate crash on all supported toolchains.

Manifest identity and source invocation have deliberately different shapes.
The runner keeps the canonical `module::qualifiedName` in discovery, filtering,
reporting, and seed calculation. The generated one-source harness appends the
original source and invokes only the final local function name, because `::` is
an identity separator rather than current ZR call syntax. Empty or malformed
manifest names fail harness construction instead of being emitted as source.

Exit code 0 means all selected cases passed or skipped, 1 means assertion/test
failure or timeout, 2 is command/selection misuse, and 3 is runner/isolate
failure. Summary output includes counts, deterministic seed, jobs, timeout,
target, and module graph hash.

## Limits

The current runner provides process isolation, deterministic discovery,
bounded output, filtering, listing, parallel module groups, timeout, sync/async
execution, and structured exit codes. Debug manifest projection is not yet an
accepted Gate 14 M4 consumer.
