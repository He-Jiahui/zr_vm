# AOT 12-S7ZZD / 11-S7 Member Token Remap ABI Publication

时间：2026-07-01 18:43:00 +08:00

## 状态

完成一个 12-S7 / 11-S7 支撑子切片：generated AOT C 现在把 emitted zrp metadata pruning 产生的 retained
member-token `sourceToken -> targetToken` remap sidecar 发布到公共 AOT ABI descriptor/codeRegistration。

完整 11-S7 / 12-S7 仍未关闭；cross-module target/provider binding、真正的 export manifest/table rewrite/publication、
完整 metadata sweep/pruning、完整 trim analyzer 和 runtime ABI drift deopt coverage 仍待后续。

## 完成项目

- `ZR_VM_AOT_ABI_VERSION` 升到 `12u`。
- 公共 ABI 新增 `SZrAotMemberTokenRemap`，并在 `SZrAotCodeRegistration` 与 `ZrAotCompiledModule` 中发布
  `memberTokenRemaps/memberTokenRemapCount`。
- AOT C emitter 从 `SZrAotCEmbeddedZrpMetadata.memberTokenRemapEntries` 生成
  `static const SZrAotMemberTokenRemap zr_aot_member_token_remaps[]`，并在 descriptor 与 codeRegistration 中指向同一张表。
- generated C 新增 `code_stripping.memberTokenRemaps` 与逐项 source/target token marker，便于验收和后续跨模块 remap 审计。
- runtime descriptor validation 校验 descriptor/codeRegistration remap table 指针与数量一致，并拒绝 count/table 空形态不一致。

## RED/GREEN

- RED：`tests/parser/test_aot_c_code_stripping.c` 在已有 MethodDef pruning fixture 上要求 generated C 发布
  source `0x03000002` -> target `0x03000001` 的 `SZrAotMemberTokenRemap` 表，并要求 descriptor/codeRegistration
  都绑定该表。旧实现只改写 `zr_aot_method_tokens[]`，测试失败 `Expected Non-NULL`。
- GREEN：生成 C 同时输出 compacted method-token table、member-token remap marker、静态 ABI remap 表，以及
  descriptor/codeRegistration 两处绑定；相邻 sidecar 单测仍通过。

## 验证

- WSL GCC：direct `zr_vm_aot_c_code_stripping_test` 10/0、`zr_vm_aot_c_source_contracts_test` 24/0、
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 3/0；focused CTest
  `aot_c_(code_stripping|zrp_metadata_pruning|zrp_metadata_export_token_remap|metadata_binding_loader)` 4/4。
- WSL Clang：direct source contracts 24/0；同 focused CTest 4/4。
- Windows MSVC Debug：direct source contracts 24/0；同 focused CTest 4/4。

## 备注

本切片只把现有 pruning sidecar 发布到 generated C ABI surface。它不声明 cross-module export manifest/table
真实绑定、跨模块 provider target resolution、attribute/annotation 抑制策略、完整 metadata sweep 或 full trim analyzer 已完成。
