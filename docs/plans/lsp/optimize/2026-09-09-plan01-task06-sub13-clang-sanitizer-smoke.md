---
plan_id: optimize
task: plan01-task06-sub13
status: completed
related_code:
  - tests/language_server/stdio_smoke.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
  - docs/cli-and-tooling/lsp-stdio-validation.md
doc_type: plan-record
---

# Plan 01 Task 6 Sub13: Clang Sanitizer Smoke

## 状态与产出记录

- Started: 2026-09-09 05:52 +08:00
- Completed: 2026-09-09 05:55 +08:00
- Source: 9f8f123f plus the shared working-tree overlay.
- Status: Clang sanitizer full-smoke replay accepted under the documented shadow-memory budget.
- Outputs: sanitizer replay evidence and parent validation crosswalk.
- Remaining gates: production 512 MiB peak on all required uninstrumented targets and broader plan acceptance.

## Validation

The Clang 14 ASan/UBSan/LSan stdio server and CLI were rebuilt in the existing
exclusive ext4 cache. The full `stdio_smoke.js` workload ran with
`ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES=1073741824`, the documented sanitizer-only
allowance for ASan shadow memory. The unchanged protocol, latency, exit-status,
stderr and peak assertions all executed.

The run exits 0. Peak working set is 686,510,080 bytes (654.71 MiB), below the
1 GiB sanitizer allowance. The server receives `exit`, closes with status 0 and
empty stderr. No ASan, UBSan or LSan diagnostics are present. Log:
`.codex/lsp-optimize-validation/plan01-task06-sub13-clang-stdio.log`.

```text
ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES=1073741824 node tests/language_server/stdio_smoke.js <clang-build>/bin/zr_vm_language_server_stdio <clang-build>/bin/zr_vm_cli
```

The sanitizer allowance does not alter the frozen production 512 MiB budget.
GCC's uninstrumented replay is 37.03 MiB and MSVC's is 45.06 MiB; those runs
remain the production peak evidence. Sub12's one-byte negative replay continues
to prove that a budget failure occurs after normal server teardown.

## Scope Boundary

This is a validation-only subtask. It changes no source, sanitizer option,
production limit or protocol fixture. The Clang result closes the sanitizer
replay gap left by Sub12 while the separate production peak requirement remains
explicitly open until its full target matrix is accepted.
