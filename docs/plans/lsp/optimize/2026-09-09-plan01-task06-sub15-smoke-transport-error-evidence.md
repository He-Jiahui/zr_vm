---
plan_id: optimize
task: plan01-task06-sub15
status: completed
related_code:
  - tests/language_server/stdio_smoke.js
  - tests/CMakeLists.txt
tests:
  - tests/language_server/stdio_smoke_outcome.test.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
doc_type: milestone-record
---

# Plan 01 Task 6.15: Smoke Transport Error Evidence

## Failure And Change

During Task 3.33 validation, Clang full smoke exited 1 in
`awaitLspRequestOutcome` with `Unexpected token i in JSON at position 1`.
The helper parsed every exception message as a JSON-RPC error and replaced
non-JSON exceptions with a new SyntaxError. The original failure text was lost;
its similarity to the client's timeout message does not prove that a timeout
caused that run. An unchanged replay with a JSON parsing probe passed without
reproducing the error. Both logs remain under `plan03-task03-sub33-stdio-clang`.

Two deterministic negative cases reject with timeout and transport-close errors.
RED passes the success/protocol cases and fails these two cases because the
helper replaces both exception objects. The helper now rethrows the original
exception when JSON decoding fails, retaining request ID, stderr and stack.
Valid JSON-RPC errors still return their structured code/message/data outcome.

The smoke entry point is guarded so its helper can be imported without launching
a server or creating fixtures. A separate four-case script uses the existing
plain Node/assert convention and is registered as
`language_server_stdio_smoke_outcome`; it also runs with the default WSL Node 12.
No deadline, protocol assertion, memory budget or server code changes here.

## Verification

Source: `e55e4f5f` plus this milestone, the Task 3.33 native module-link change
and the pre-existing shared worktree overlay. Only this milestone's five-line
CTest registration is owned in the otherwise dirty `tests/CMakeLists.txt`.

Initial `node --test` RED on Windows Node 22.13.1: 2 PASS / 2 FAIL, exit 1.
That draft runner passed after the helper fix on Windows and WSL Node 22.13.1.
The final plain Node runner removes its dependency on `node:test`.

All three stdio build targets regenerated successfully, with no new C compilation.
`ctest --test-dir <build> --output-on-failure -R '^language_server_stdio_smoke_outcome$'`
passes 1/1, exit 0 in all three builds (MSVC adds `-C Debug`). It runs all four
success/protocol/timeout/close assertions. GCC uses Node 12.22.9; Clang and MSVC
CTest use Node 22.13.1. The script consumes no external fixture or server.

Normal full smoke uses
`node tests/language_server/stdio_smoke.js <stdio-executable> <cli-executable>`:

| Build | Node | Exit | Peak Bytes | Peak MiB |
| --- | --- | --- | --- | --- |
| GCC 11.4 Debug static | WSL 12.22.9 | 0 | 38223872 | 36.45 |
| Clang 14 Debug shared ASan/UBSan/LSan | WSL 12.22.9 | 0 | 735879168 | 701.79 |
| MSVC 19.44 Debug shared | Windows 22.13.1 | 0 | 48025600 | 45.80 |

All complete explicit exit, status 0 and empty stderr checks. Clang retains
`ASAN_OPTIONS=detect_leaks=1`, `UBSAN_OPTIONS=halt_on_error=1` and the established
sanitizer-only `ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES=1073741824`. GCC/MSVC retain
the 512 MiB default. No sanitizer report occurs. Build directories are
`/home/hejiahui/.codex-builds/l8-callable-value-gcc`,
`/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang` and
`E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc-current`.

Logs use `.codex/lsp-optimize-validation/plan01-task06-sub15-`:
`red-windows`, `green-windows`, `green-wsl-node22`,
`register-{gcc,clang,msvc}`, `ctest-{gcc,clang,msvc}` and
`stdio-{gcc,clang,msvc-final}.log`. The first WSL draft invocation failed because
Node 12 has no `--test` option; the first MSVC invocation used nonexistent
`bin/Debug` paths and failed with ENOENT before server startup. Those are retained
as `green-wsl.log` and `stdio-msvc.log`, not counted as passing validation.

## 状态与产出记录

- Started: 2026-09-09 12:19 +08:00.
- Completed: 2026-09-09 12:29 +08:00.
- Status: original transport exceptions preserved; registered regression and
  full stdio passed on GCC, Clang and MSVC. All command sessions ended.
- Outputs: smoke helper, four-case regression, CTest registration, module
  contract, plan/index updates and this record; source and commands are above.
- Remaining: the original transient Clang failure's raw cause was not recovered.
  Parent protocol/performance/native/Web gates and aggregate LSP failures remain
  open; this evidence does not accept those gates.
