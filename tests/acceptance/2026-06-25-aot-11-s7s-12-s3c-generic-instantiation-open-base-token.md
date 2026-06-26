# AOT 11-S7S / 12-S3C / 12-S4K / 08-S7H: generic instantiation open base token

时间：2026-06-25 05:28:38 +08:00

## Scope

- 让 TypeSpec-bound `.zrp` manifest generic preserve root 在当前模块存在同名 `TYPE_REF` 元数据记录时，
  使用 open generic base token 作为 `SZrGenericInstantiationRecord.baseToken`。
- 保留兼容回退：没有可匹配 `TYPE_REF` 时，仍使用既有 closed `TYPE_SPEC` token，避免破坏前一切片验收。
- 不声明 MethodSpec 解析、缺失 TypeSpec 合成、跨模块 generic target 绑定或完整 mark-and-sweep generic closure 完成。

## RED

- 新增 `test_cli_aot_writer_options_materializes_generic_preserve_instantiation_open_base_token`。
- 测试构造 `TYPE_REF(List)` 与 `TYPE_SPEC(List<Foo>)` 元数据后，要求 manifest generic instantiation
  `baseToken == ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u)`。
- 初始失败：
  - `Expected 83886081 Was 117440513`
  - 即期望 `0x05000001`，实际仍为 closed `TYPE_SPEC` `0x07000001`。

## GREEN

- `compiler_aot.c` 新增 current-module `TYPE_REF` signature 匹配与 base token 解析。
- `zr_cli_aot_preserve_bind_generic_root_instantiation()` 先解析 open base token，找到匹配 `TYPE_REF` 时传给
  `ZrParser_GenericInstantiationTable_GetOrAddResolved()`；找不到时回退 `root->typeSpecToken`。
- generated C manifest diagnostic 现在可输出 `manifest.genericRoot[0].genericInstance.baseToken = 0x05000001`。

## Verification

- WSL gcc direct: `zr_vm_cli_aot_writer_options_test` 11/0.
- WSL gcc CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5.
- WSL clang CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5.
- Windows MSVC Debug CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5.
