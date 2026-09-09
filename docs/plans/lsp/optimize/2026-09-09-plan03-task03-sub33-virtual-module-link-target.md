---
plan_id: optimize
task: plan03-task03-sub33
status: completed
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
tests:
  - tests/language_server/test_lsp_virtual_module_link_target_cases.h
  - tests/language_server/test_lsp_semantic_query_parity.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.33: Virtual Module Link Target

## Failure And Change

Definition on a rendered native module link constructed a target URI and
returned the placeholder `(0,0)-(0,0)`, even when no target descriptor existed.
The registered fixture links to `zr.math` and an unregistered module. GCC RED
has 30 passing cases and two failures, exit 1: the real target needs its module
identifier `(0,15)-(0,22)`, and the missing target must fail with no locations.

The existing virtual module-link branch now resolves the target descriptor and
uses the shared native declaration projection with MODULE kind and that exact
descriptor identity. It publishes a location only after this succeeds. The
content-aware converter handles the copied range for the unopened target document.
No declaration coordinates or descriptor identity are reconstructed from spelling.
Descriptor pointers are borrowed only during this lookup; output text/URI retain
their existing GC ownership and result entries retain caller cleanup.

This is a scoped change to the existing oversized interface branch. The two
regressions live in a separate header and use the shared result cleanup helper.
Project-scoped plugin selection and binary virtual documents remain separate
unaccepted requirements. Reference evidence and projection design remain those
recorded in [Sub31](2026-09-09-plan03-task03-sub31-native-virtual-declaration-projection.md).

## Verification

Source: `e55e4f5f` plus these three code/test files and the existing shared
worktree overlay. Final normal smoke also contains support commit `1efb5e22`
([Task 6.15](2026-09-09-plan01-task06-sub15-smoke-transport-error-evidence.md)).
Build commands use `cmake --build <build> --target <targets> --parallel 16`:

```text
zr_vm_language_server_semantic_query_parity_test
zr_vm_language_server_lsp_interface_test
zr_vm_language_server_lsp_semantic_snapshot_test
zr_vm_language_server_lsp_source_contracts_test
zr_vm_language_server_stdio
```

GCC 11.4 Debug static uses `/home/hejiahui/.codex-builds/l8-callable-value-gcc`;
Clang 14 Debug shared ASan/UBSan/LSan uses
`/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang`; MSVC 19.44 Debug
shared uses `E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc-current`, adding
`--config Debug` through the `using-vsdevcmd` wrapper. All builds exit 0.

| Toolchain | Parity | Snapshot Executable | Source Contracts | Final Full Stdio |
| --- | --- | --- | --- | --- |
| GCC | 32/32, exit 0 | 0 failures, exit 0 | exit 0 | exit 0, 36.45 MiB |
| Clang | 32/32, exit 0 | 0 failures, exit 0 | exit 0 | exit 0, 701.79 MiB |
| MSVC | 32/32, exit 0 | 0 failures, exit 0 | exit 0 | exit 0, 45.80 MiB |

The matching `<build>/bin/` executables were run directly, including snapshot.
Clang parity/snapshot/contracts contain no sanitizer report. All full interface
runners retain 114 PASS / 2 FAIL, exit 1, with the same two Sub32 failures:

- LSP Class Member Navigation And Completion
- LSP Hover And Completion Surface Explicit Exact Type Failures

Clang interface retains 20144 bytes / 422 allocations. Project tests were not
rerun for this virtual-document-only branch; the last Sub32 result remains
55 PASS / 7 FAIL with Clang 19160 bytes / 481 allocations. Parser code is unchanged.

Initial normal GCC/MSVC stdio passed with peaks 36.56/45.38 MiB. Initial Clang
stdio exited 1 after the test helper replaced its original error with SyntaxError.
An unchanged parsing-probe replay passed at 707.30 MiB without reproducing the
original error. Task 6.15 fixes evidence preservation and adds a registered
four-case regression. Normal full smoke then passed on all three builds, using
`node tests/language_server/stdio_smoke.js <stdio-executable> <cli-executable>`.
All servers exited 0 with empty stderr. Clang retains leak detection, UBSan halt
and the established 1 GiB sanitizer-only budget; GCC/MSVC retain 512 MiB.

Logs are `.codex/lsp-optimize-validation/plan03-task03-sub33-`
`{red,build,parity,interface,snapshot,contracts,stdio}-*.log` and the final
`plan01-task06-sub15-stdio-{gcc,clang,msvc-final}.log`. Initial failures and probe
results remain separate from final passing evidence. All command sessions ended.

## 状态与产出记录

- Started: 2026-09-09 12:03 +08:00.
- Completed: 2026-09-09 12:29 +08:00.
- Status: virtual module links require an existing target descriptor and select
  its exact rendered module identifier; two RED/GREEN cases accepted on all three
  toolchains. Parent gates remain open.
- Outputs: scoped definition branch, two regressions, module/plan/index updates
  and this record. Source versions and validation commands are above.
- Remaining: actual binary/plugin project-scoped virtual URI and parser origin
  producers, multi-definition relation matrix, canonical receiver acquisition,
  aggregate failures/leaks and Plan 03 Tasks 3/7/8 plus native/Web acceptance.
