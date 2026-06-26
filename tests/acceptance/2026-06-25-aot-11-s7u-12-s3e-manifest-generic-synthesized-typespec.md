# AOT 11-S7U / 12-S3E / 12-S4M / 08-S7J: manifest generic synthesized TypeSpec binding

时间：2026-06-25 06:03:45 +08:00

## Scope

- 当 manifest generic preserve root 没有现成 `TYPE_SPEC` record，但当前函数 metadata 里存在同名 open
  `TYPE_REF` 或 `TYPE_DEF` base record 时，为该 root 合成 TypeSpec/signature binding。
- 合成后的 root 继续进入 generic instantiation identity materialization，并在 full-AOT gate 中被视为闭合。
- 保留无 open base metadata 时的 full-AOT 拒绝行为。
- 不声明 MethodSpec 解析、跨模块 generic target 绑定、反射动态实例、annotation roots 或完整
  mark-and-sweep generic closure 完成。

## RED

- 新增 `test_cli_aot_writer_options_synthesizes_missing_generic_preserve_type_spec_from_open_type_ref`。
- 测试只构造 `TYPE_REF(List)` 和 metadata string heap，不提供 `TYPE_SPEC(List<Foo>)`。
- full-AOT `.zrp` generic preserve `List<Foo>` 初始失败：
  - `Expected TRUE Was FALSE`
  - 失败点是 `hasTypeSpecBinding`，说明当前 bridge 只能消费已存在 TypeSpec。

## GREEN

- `compiler_aot.c` 在现有 TypeSpec 匹配失败后，会查找同名 open `TYPE_DEF`/`TYPE_REF` base record。
- 找到 open base 后追加 synthesized `TYPE_SPEC`/`SIGNATURE` metadata record pair 和 deterministic
  `GENERIC_INST` signature hash。
- synthesized binding 复用现有 `SZrGenericInstantiationTable_GetOrAddResolved()` 路径生成
  `baseToken` / `cInstanceId` / `shareKind`。
- generated C manifest diagnostic 在 full-AOT 下可输出 synthesized TypeSpec token/signature/hash
  以及 open-base generic instance token。

## Verification

- WSL gcc direct: `zr_vm_cli_aot_writer_options_test` 13/0.
- WSL gcc CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5.
- WSL clang CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5.
- Windows MSVC Debug CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5.
