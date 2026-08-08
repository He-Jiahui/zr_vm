# LSP 02 Switch/Extern Close Safe Fix Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 15:35 +08:00 | 已完成 | switch case header、switch body EOF 与 extern spec close 的结构化 fix、generic LSP action、apply-edit-rebind 验收。 |

## Assertions

- `missing_switch_case_header_close`: primary `[21,22]`，在`[21,21]`插入`)`。
- `missing_switch_body_close`: primary/fix `[35,35]`，插入`}`。
- `missing_extern_spec_close`: primary `[24,25]`，在`[24,24]`插入`)`。
- 每项只含一个 machine-applicable fix；应用后下一版本诊断不再含该 code。

## Evidence

- GCC 11.4、Clang 14、MSVC 19.44：`zr_vm_compiler_semantic_query_diagnostics_test` 均为 `46 Tests / 0 Failures / 0 Ignored`、真实 exit 0。
- 同三套工具链：`zr_vm_language_server_lsp_advanced_editor_features_test` 均为 `0 failure(s)`、真实 exit 0。
- 未关闭 stdio/CLI 全链或其他 delimiter family。
