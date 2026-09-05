---
related_code:
  - zr_vm_language_server/stdio/stdio_initialize.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server/stdio/stdio_documents.c
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
implementation_files:
  - zr_vm_language_server/stdio/stdio_initialize.c
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
tests:
  - tests/language_server/stdio_save_capabilities_smoke.js
  - tests/language_server/stdio_optional_capabilities_smoke.js
  - tests/language_server/stdio_protocol_inventory.js
  - tests/acceptance/2026-09-05-lsp-save-capability.md
doc_type: milestone-record
---

# Save Notification Capability

## 状态与产出记录

| 开始时间 | 实际完成时间 | 状态 | 完成项目 | 验证结果 |
| --- | --- | --- | --- | --- |
| 2026-09-05 20:13 +08:00 | 2026-09-05 20:37 +08:00 | completed (capability subitem; parent pending) | Remove unhandled willSave publication and unused field; retain willSaveWaitUntil formatting and observable didSave disk refresh. | Initial GCC RED 2/4; three toolchains 13/13 and final save fixture 6/6; mutation fails exactly 2/6; independent re-review approved. |

## Baseline And Repair

The initialize response audit found `textDocumentSync.willSave`
advertised true but no notification dispatcher branch or handler. Before the
repair, a dedicated fixture failed its complete sync-capability object check
for both empty and save-aware clients (2/4 failed, exit 1). The other two cases
passed exact formatting edits, version-2 diagnostics, a second empty formatting
result, didSave and a definition location. RED ran against the color-corrected
GCC binary before this subitem's production edit at 20:13 +08:00.

The repair deletes the initialize field and its unused protocol
constant. It updates the optional capability snapshot and inventory assertion
and registers the dedicated save fixture with CTest. It adds no no-op handler.

Independent review found that the first fixture's didSave checks could be
satisfied by the preceding didChange. The expanded fixture saves a disk file,
loads its exact `BeforeSave` definition, changes disk content to `AfterSave`,
and requires the next didSave to publish diagnostics at generation +1 before
querying the new definition range. A client-only mutation retains the first
save and sends `$/ignoredSave` for the second save of the same URI. On the
unchanged GCC binary it fails exactly those two disk cases (4/6 pass, exit 1);
both time out waiting for the required diagnostic publication. The normal
fixture passes 6/6 on every toolchain. The production handler was not modified
for this mutation. Evidence: `.codex/lsp-save-mutation-red.log`; driver:
`.codex/lsp-save-mutation.js`. Read-only re-review confirmed the coverage gap
was closed and found no new issue.

`willSaveWaitUntil` remains a real formatting request and `didSave` remains a
document-lifecycle notification. No semantic snapshot, type identity or query
ownership is added. The existing formatting request owns its returned JSON;
the client's application of edits publishes the next document version. Complete
formatter semantics and edit staleness remain later plan gates. Module details
are in [Stdio Validation](../../../cli-and-tooling/lsp-stdio-validation.md).

## Verification

```text
cmake --build <build-dir> --target zr_vm_language_server_stdio zr_vm_language_server_lsp_capability_registry_test zr_vm_language_server_stdio_server_lifecycle_test zr_vm_language_server_stdio_optional_capability_allocations_test --parallel 4
ctest --test-dir <build-dir> --output-on-failure -R '^language_server_(lsp_capability_registry|stdio_(optional_capability_allocations|optional_capabilities_smoke|save_capabilities_smoke|color_capability_smoke|protocol_inventory|protocol_conformance|resolve_capabilities_smoke|navigation_capabilities_smoke|file_operation_capabilities_smoke|document_sync_conformance|server_lifecycle|workspace_folders_smoke))$'
git diff --check -- <owned-paths>
```

GCC 11.4 and Clang 14 use the frozen Debug shared builds
`/home/hejiahui/.codex-builds/lsp-optimize-20260905-root/{gcc,clang}`.
MSVC 19.44 uses Debug static at
`E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc` under VsDevCmd.

| Toolchain | Full focused suite | Final expanded fixture |
| --- | --- | --- |
| GCC 11.4 | build exit 0, 13/13 pass, 20.56 s | CTest 1/1, all 6 checks pass |
| Clang 14 | build exit 0, 13/13 pass, 12.80 s | CTest 1/1, all 6 checks pass |
| MSVC 19.44 | build exit 0, 13/13 pass, 121.58 s | direct Node fixture, all 6 checks pass |

Only the save fixture changed after the full suites; its expanded final
version was then rerun on all three unchanged binaries. All 29 exported
code/test files match the workspace and both frozen sources byte-for-byte.
Configured extension unit/noEmit were already verified in Sub04 and were not
rerun for this native-only declaration edit. Whole-suite semantic failures
remain in the baseline ledger; no broader green result is claimed.

These builds use `c95e5387`, exact gitlinks and this task's owned corrections
through `5d8f520f`, followed by this subitem's explicitly listed paths.
Other active runtime/parser/semantic overlays remain outside the frozen source.
The containing commit binds the final implementation, tests, documentation
and record; whole-plan acceptance still requires one integrated committed tree.

No language behavior or syntax design changes are introduced. Phase 00's
complete inventory, integrated failures and phase promotion remain pending.
