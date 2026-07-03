# AOT 12-S7ZZF / 11-S7 Member Token Remap Duplicate Validation

时间：2026-07-01 19:27:51 +08:00

## 状态

完成一个 12-S7 / 11-S7 runtime ABI drift/descriptor validation 支撑子切片：已发布的
`SZrAotMemberTokenRemap` ABI table 现在会拒绝重复 source member token 或重复 target member token，避免 compacted
metadata remap table 在后续跨模块 export-token publication/provider binding 前出现一对多或多对一歧义。

完整 11-S7 / 12-S7 仍未关闭；cross-module provider binding、真实 export manifest/table rewrite/publication、完整
metadata sweep/pruning、完整 trim analyzer、annotation policy 和更完整的 runtime ABI drift/deopt coverage 仍待后续。

## 完成项目

- root runtime 与 mirrored AOT runtime 都在 member-token remap entry 形态校验后扫描既有 entries。
- descriptor validation 拒绝重复 `sourceToken`，诊断包含 `index`、`previousIndex` 和重复 token。
- descriptor validation 拒绝重复 `targetToken`，诊断包含 `index`、`previousIndex` 和重复 token。
- descriptor diagnostics 复用手写 bad descriptor 动态库 fixture，分别覆盖 duplicate source 与 duplicate target。
- source contract 同步锁定 root/mirrored runtime 的 duplicate source/target 比较和诊断文本。

## RED/GREEN

- RED：新增 duplicate source 与 duplicate target descriptor 用例后，旧 runtime 接受两条合法形态但重复的 remap entries，
  WSL GCC 两个新用例均失败 `Expected FALSE Was TRUE`。
- GREEN：runtime 在 descriptor validation 阶段拒绝重复映射，分别报出
  `member token remap duplicate sourceToken index=1 previousIndex=0 sourceToken=0x03000001` 和
  `member token remap duplicate targetToken index=1 previousIndex=0 targetToken=0x03000001`。

## 验证

- WSL GCC：direct descriptor diagnostics 5/0、source contracts 24/0；focused CTest
  `aot_c_(descriptor_diagnostics|code_stripping|zrp_metadata_export_token_remap|metadata_binding_loader)` 4/4。
- WSL Clang：direct descriptor diagnostics 5/0、source contracts 24/0；同 focused CTest 4/4。
- Windows MSVC Debug：descriptor diagnostics 5/0/5 ignored（Unix-only shared-library branch）、source contracts 24/0；
  同 focused CTest 4/4。

## 备注

本切片只收紧已发布 member-token remap ABI 的唯一性约束。它不声明跨模块 provider target resolution、真实 export
manifest/table rewrite/publication、attribute/annotation policy、完整 metadata sweep 或 full trim analyzer 已完成。
