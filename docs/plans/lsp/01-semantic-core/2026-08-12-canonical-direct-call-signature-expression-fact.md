# Canonical Direct-Call Signature Expression Fact

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-12 04:40 +08:00 | 已完成 | LSP 08 第十二个独立合同：source `FUNCTION_DECLARATION` 直接调用只按canonical call fact提供signature help；fact不可用时fail closed。 |

## Contract

- direct source function call首先通过`ZrLanguageServer_LspCanonicalSignatureHelp_Resolve`消费`ZrParser_SemanticQuery_CallAt`和`ZrParser_SemanticQuery_FormatCall`。
- 当callee的definition fact精确指向`ZR_AST_FUNCTION_DECLARATION`而该调用没有canonical call fact时，`ZrLanguageServer_Lsp_GetSignatureHelp`返回unavailable；不得继续调用局部overload、callee-name或AST-text fallback。
- callable variable assignment不是function declaration。它尚未具备等价`CallAt`事实，仍为独立parser support边界；本记录不将旧行为当作canonical授权。

## TDD Evidence

RED在source `inspect(1)`上取得有效canonical signature后，把同一expression fact的`hasCallInfo`置为false。旧实现仍从local overload/callee-name路径返回`inspect(value: int): int`，测试失败。

GREEN只在canonical signature resolver失败后检查callee的精确definition fact。声明为`FUNCTION_DECLARATION`的直接调用立即fail closed；事实仍存在时显示canonical label。回归保留`pub var runBossScenario = runBossScenarioImpl`的callable value合同，避免把未发布fact的value调用误分类为source function declaration。

## Validation

固定LSP source overlay在独立GCC、Clang和MSVC快照中重建并直接执行：

| Toolchain | Facts | Local query | Expression hover | Local hover | Interface | Project | stdio/CLI |
| --- | --- | --- | --- | --- | --- | --- | --- |
| GCC | 13/13 | 32/32 | 9/9 | 12/12 | 108/108 | 58/58 | 2/2 |
| Clang | 13/13 | 32/32 | 9/9 | 12/12 | 108/108 | 58/58 | 2/2 |
| MSVC | 13/13 | 32/32 | 9/9 | 12/12 | 108/108 | 58/58 | 2/2 |

每套测试进程与`ctest -R "^(language_server_stdio_smoke|cli_integration)$"`均真实exit 0。此leaf不宣告L8整体完成。

## Open Scope

- callable variable、closure和其他非`FUNCTION_DECLARATION` call context需要parser先发布同样精确的call fact，LSP不得扩张本leaf的fallback。
- binary/native/provider与完整L8 fallback收敛仍按各自plan record验收。
