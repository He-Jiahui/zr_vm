# Canonical Generic Receiver Signature Fact

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-12 11:55 +08:00 | 已完成 | LSP 08 第十三个独立合同：闭合generic receiver signature只消费canonical call fact，缺失时fail closed。 |

## Contract

- `Box<int>.shape(matrix)`通过`ZrParser_SemanticQuery_CallAt`与`ZrParser_SemanticQuery_FormatCall`提供完整闭合label：`fn shape<const N: int>(value: Matrix<int, 4>): Matrix<int, 4>`。
- call fact仍有效时，signature help必须与该canonical label完全一致。
- 当同一expression的`hasCallInfo`不可用时，source class/struct/interface method declaration identity会阻断后续LSP method fallback；不得按receiver AST、open generic declaration、method name或const-generic substitution重建签名。

## TDD Evidence

RED：删除`Box<int>.shape(matrix)`的canonical call fact后，旧`signature_resolve_method_help`经`signature_prepare_ast_specialized_receiver_member`重新构建闭合generic label，negative test失败。

GREEN：canonical resolver失败后由`DefinitionOf`确认精确callee为source free/class/struct/interface declaration。匹配时直接unavailable，保留external provider与未发布call fact的callable value边界。

## Validation

独立GCC、Clang、MSVC快照均直接执行：

| Toolchain | Facts | Local query | Expression hover | Local hover | Interface | Project | stdio/CLI |
| --- | --- | --- | --- | --- | --- | --- |
| GCC | 13/13 | 32/32 | 9/9 | 12/12 | 110/110 | 58/58 | 2/2 |
| Clang | 13/13 | 32/32 | 9/9 | 12/12 | 110/110 | 58/58 | 2/2 |
| MSVC | 13/13 | 32/32 | 9/9 | 12/12 | 110/110 | 58/58 | 2/2 |

每个测试进程与`ctest -R "^(language_server_stdio_smoke|cli_integration)$"`均真实exit 0。本leaf不表示L8整体完成。

## Open Scope

- callable value、closure和其他没有精确source declaration call fact的上下文仍需要parser support，LSP不得扩张AST fallback。
- binary/native/provider与其余signature fallback删除继续按独立plan record验收。
