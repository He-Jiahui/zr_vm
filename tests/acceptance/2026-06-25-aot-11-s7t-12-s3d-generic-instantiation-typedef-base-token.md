# AOT 11-S7T / 12-S3D / 12-S4L / 08-S7I: generic instantiation TypeDef base token

时间：2026-06-25 05:41:31 +08:00

## Scope

- 让 manifest generic preserve root 的 TypeSpec binding 支持 `GENERIC_INST(TYPE_DEF target, args...)`。
- 当 TypeSpec 的 base 节点是 `TYPE_DEF` 时，用匹配的 current-module `TYPE_DEF` token 作为
  generic instantiation base token。
- 保留前一切片的 `TYPE_REF` open-base 行为和 closed `TYPE_SPEC` fallback。
- 不声明 MethodSpec 解析、缺失 TypeSpec 合成、跨模块 generic target 绑定或完整 mark-and-sweep generic closure 完成。

## RED

- 新增 `test_cli_aot_writer_options_materializes_generic_preserve_instantiation_type_def_base_token`。
- 测试构造 `TYPE_DEF(List)` 与 `TYPE_SPEC(GENERIC_INST(TYPE_DEF List, Foo))` 元数据后，要求
  `genericInstantiationBaseToken == ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u)`。
- 初始失败：
  - `Expected TRUE Was FALSE`
  - 失败点是 `hasTypeSpecBinding`，说明 TypeSpec 匹配路径只接受 `TYPE_REF` base。

## GREEN

- `zr_cli_aot_generic_root_matches_type_spec_signature()` 现在接受 `TYPE_REF` 或 `TYPE_DEF` 作为
  `GENERIC_INST` base 节点。
- `zr_cli_aot_preserve_resolve_generic_root_base_token()` 从已绑定 TypeSpec 的 base 节点选择对应 token 表：
  `TYPE_REF` 查同名 TypeRef，`TYPE_DEF` 查同名 TypeDef，找不到时回退 TypeSpec token。
- generated C manifest diagnostic 现在可输出 `manifest.genericRoot[0].genericInstance.baseToken = 0x02000001`。

## Verification

- WSL gcc direct: `zr_vm_cli_aot_writer_options_test` 12/0.
- WSL gcc CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5.
- WSL clang CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5.
- Windows MSVC Debug CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5.
