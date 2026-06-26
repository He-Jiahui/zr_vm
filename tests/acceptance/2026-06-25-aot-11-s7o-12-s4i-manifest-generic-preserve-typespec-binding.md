# AOT 11-S7O / 12-S4I / 08-S7D manifest generic preserve TypeSpec binding

时间：2026-06-25 04:00:47 +08:00

## 范围

本切片把 `.zrp` `preserve` 中 `kind: "generic"` 的 target/arguments 从纯文本 writer root
推进到“可绑定已有 TypeSpec metadata”的窄通道：

- `SZrAotManifestGenericRoot` 新增 `hasTypeSpecBinding`、`typeSpecToken`、`signatureToken`、
  `signatureHash`。
- CLI AOT preserve bridge 在注入 generic root 时扫描当前函数 metadata token records。
- 当已有 `TYPE_SPEC` 的签名 blob 匹配当前 emitted shape
  `GENERIC_INST(TYPE_REF target, args...)` 时，把 TypeSpec token、paired signature token 和 signature hash
  写入 writer options。
- generated C manifest 诊断输出 `manifest.genericRoot[i].typeSpecToken`、
  `manifest.genericRoot[i].signatureToken`、`manifest.genericRoot[i].signatureHash`。

## RED / GREEN

RED：`tests/cli/test_cli_aot_writer_options.c` 先新增
`test_cli_aot_writer_options_binds_generic_preserve_to_type_spec_token`，并断言
`SZrAotManifestGenericRoot` 存在 TypeSpec binding fields。旧实现编译失败，因为 writer root 只有
target/arguments。

GREEN：测试 fixture 手工附加 `List<Foo>` 的 `TYPE_SPEC` record、paired `SIGNATURE` record 和
`GENERIC_INST(TYPE_REF "List", TYPE_REF "Foo")` signature blob。generic preserve root 现在绑定到：

- TypeSpec token `0x07000001`
- Signature token `0x08000001`
- Signature hash `0x123456789abcdef0`

generated C 同步输出对应 manifest diagnostics。

## 验证

- WSL gcc: `zr_vm_cli_aot_writer_options_test` 7/0
- WSL gcc CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5
- WSL clang CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5
- Windows MSVC Debug CTest:
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  5/5
- `git diff --check` 退出 0，仅报告 LF/CRLF 提示

## 未关闭项

本切片只绑定当前模块中已经存在且签名匹配的 `TYPE_SPEC`。
它不声明以下能力完成：

- MethodSpec token 解析或绑定
- 缺失 TypeSpec 自动合成
- generic instantiation table materialization
- mark-and-sweep generic closure
- 跨模块 generic target 绑定
- annotation roots
- full-AOT 缺失实例编译期诊断
