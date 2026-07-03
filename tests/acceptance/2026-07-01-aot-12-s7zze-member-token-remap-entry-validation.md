# AOT 12-S7ZZE / 11-S7 Member Token Remap Entry Validation

时间：2026-07-01 19:04:27 +08:00

## 状态

完成一个 12-S7 / 11-S7 支撑子切片：runtime descriptor validation 现在会逐项校验 generated AOT ABI
中的 member-token remap entry，拒绝 source/target 不是非零本模块 `MEMBER_DEF` token 的 remap。

完整 11-S7 / 12-S7 仍未关闭；cross-module provider binding、真正的 export manifest/table rewrite/publication、
完整 metadata sweep/pruning、完整 trim analyzer 和更完整的 runtime ABI drift/deopt coverage 仍待后续。

## 完成项目

- root runtime 与 mirrored AOT runtime 都引入 `aot_runtime_member_token_remap_entry_is_valid()`。
- descriptor validation 在 pointer/count/null 形态校验后扫描 `descriptor->memberTokenRemaps`。
- invalid entry 诊断包含 remap index、source token 和 target token，便于后续跨模块 remap 审计。
- source contract 同步锁定 root/mirrored runtime 的 `metadata_token.h` 依赖、`MEMBER_DEF` table 校验、RID 非零校验和诊断文本。
- descriptor diagnostics 新增坏 descriptor 动态库 fixture，把有效 `.zro` blob 嵌入 ABI 形态正确但 remap entry 错误的 shared library。

## RED/GREEN

- RED：`test_aot_c_descriptor_diagnostic_rejects_invalid_member_token_remap_entry` 使用
  source `0x02000001`、target `0x03000001` 的 bad descriptor；旧 runtime 接受该 descriptor 并执行成功，WSL GCC 失败
  `Expected FALSE Was TRUE`。
- GREEN：runtime 先加载有效 embedded `.zro`，随后在 descriptor validation 阶段拒绝该 remap entry，并写出
  `member token remap entry invalid index=0 sourceToken=0x02000001 targetToken=0x03000001`。

## 验证

- WSL GCC：direct descriptor diagnostics 3/0、source contracts 24/0；focused CTest
  `aot_c_(descriptor_diagnostics|code_stripping|zrp_metadata_export_token_remap|metadata_binding_loader)` 4/4。
- WSL Clang：direct descriptor diagnostics 3/0、source contracts 24/0；同 focused CTest 4/4。
- Windows MSVC Debug：descriptor diagnostics 3/0/3 ignored（Unix-only shared-library branch）、source contracts 24/0；同 focused CTest 4/4。

## 备注

本切片只收紧已发布 member-token remap ABI 的 entry 形态。它不声明跨模块 provider target resolution、真实 export
manifest/table rewrite/publication、attribute/annotation policy、完整 metadata sweep 或 full trim analyzer 已完成。
