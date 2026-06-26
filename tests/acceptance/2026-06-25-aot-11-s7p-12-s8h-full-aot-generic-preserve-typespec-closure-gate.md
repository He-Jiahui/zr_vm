# AOT 11-S7P / 12-S8H / 08-S7E full-AOT generic preserve TypeSpec closure gate

时间：2026-06-25 04:14:31 +08:00

## 范围

本切片把 11-S7O / 12-S4I 的 generic preserve TypeSpec 绑定结果接入 full-AOT 闭包校验：

- `ZrParser_Writer_WriteAotCFileWithOptions()` 在 full-AOT 模式下检查 manifest generic preserve roots。
- 如果任一 root 仍没有 `hasTypeSpecBinding`，writer 在生成前返回 `ZR_FALSE`。
- 默认 hybrid 模式保持原行为，仍允许未绑定 generic root 输出 manifest 诊断清单。
- 已绑定 TypeSpec 的 generic root 继续输出 TypeSpec token、signature token 和 signature hash。

## RED / GREEN

RED：`tests/cli/test_cli_aot_writer_options.c` 新增
`test_cli_aot_writer_options_rejects_unbound_generic_preserve_root_in_full_aot`。
该用例构造 `.zrp` `aotMode: "full-aot"` + `preserve kind: "generic" target: "List" arguments: ["Foo"]`，
但不附加匹配的 `TYPE_SPEC` metadata。旧 writer 仍返回 true，测试失败。

GREEN：AOT C writer 增加 full-AOT manifest generic root 闭包门禁。
未绑定 TypeSpec 的 manifest generic root 现在返回 false；hybrid 未绑定 root 和已绑定 TypeSpec root 的既有用例继续通过。

## 验证

- WSL gcc: `zr_vm_cli_aot_writer_options_test` 8/0
- WSL gcc CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5
- WSL clang CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5
- Windows MSVC Debug CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5

## 未关闭项

本切片只关闭 full-AOT 对 manifest generic preserve TypeSpec 绑定结果的消费门禁。
它不声明以下能力完成：

- MethodSpec token 解析或绑定
- 缺失 TypeSpec 自动合成
- generic instantiation table materialization
- mark-and-sweep generic closure
- 跨模块 generic target 绑定
- annotation roots
- 默认最小 metadata 策略
