# AOT 11-S7Q / 12-S3A / 08-S7F Manifest Generic Preserve Instantiation Root

时间：2026-06-25 04:50:01 +08:00

## Scope

本切片把已经绑定到当前模块 `TYPE_SPEC` 的 `.zrp` manifest generic preserve root 物化为 writer 可见的 generic instantiation root 身份。它复用 `SZrGenericInstantiationTable`，用 TypeSpec token 作为当前 closed TypeSpec-backed instance identity 的 base token，并记录 `cInstanceId` 与 `shareKind`。

不包含：MethodSpec 解析、缺失 TypeSpec 合成、跨模块 generic target 绑定、反射动态泛型实例、完整 mark-and-sweep generic closure。

## RED

- `test_cli_aot_writer_options_materializes_bound_generic_preserve_instantiation_root` 先引用 `SZrAotManifestGenericRoot` 上缺失的 `hasGenericInstantiationBinding`、`genericInstantiationBaseToken`、`genericInstantiationInstanceId`、`genericInstantiationShareKind` 字段。
- WSL gcc 构建 `zr_vm_cli_aot_writer_options_test` 失败，错误为 `SZrAotManifestGenericRoot has no member named ...`。

## GREEN

- `SZrAotManifestGenericRoot` 新增 generic instantiation 绑定字段。
- `ZrCli_Compiler_ApplyProjectAotPreserveRules()` 在注入 generic preserve roots 时创建临时 `SZrGenericInstantiationTable`。
- 已匹配 `GENERIC_INST` `TYPE_SPEC` 的 root 会把 concrete argument 文本转换为 resolved type arguments，写入 table，并保存 `baseToken`、`cInstanceId`、`shareKind` 到 writer root。
- AOT C manifest 诊断输出：
  - `manifest.genericRoot[0].genericInstance.baseToken`
  - `manifest.genericRoot[0].genericInstance.id`
  - `manifest.genericRoot[0].genericInstance.shareKind`

## Verification

- WSL gcc direct: `zr_vm_cli_aot_writer_options_test` 9/0。
- WSL gcc CTest: `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model` 5/5。
- WSL clang same filtered CTest 5/5.
- Windows MSVC Debug same filtered CTest 5/5.

## Notes

This is a stable bridge from manifest TypeSpec identity to a generic instantiation identity. It is not yet the final open-generic base token model, nor the full 12-S3 mark-and-sweep generic instance closure.
