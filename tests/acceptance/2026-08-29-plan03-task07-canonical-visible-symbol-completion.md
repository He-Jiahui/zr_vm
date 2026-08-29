# Plan 03 Task 7.23 Canonical Visible Symbol Completion Acceptance

## 验收基线

- 固定基线 HEAD：`b730b40d6f85fcf5d28e590681213cc512704524`；验收只叠加 14 个
  Task 7.23 code/test paths。
- Parser RED：compiled source function visible fact 缺 named canonical signature，extern block
  declarations 未进入 `VisibleSymbols`。
- LSP RED：脱离 analyzer symbol table 后 lexical completion 不可用；unknown exact TypeId 没有
  明确 fail-closed detail。
- GREEN：scope producer 与 query 发布完整 exact facts，LSP projector 仅消费
  `VisibleSymbols` 并复制为 protocol-owned completion items。

## 验收结果

- GCC 11.4、Clang 14、MSVC 19.44 fixed snapshot 均构建 parser symbols、semantic-query parity、
  source contracts、hover/inlay、interface/project、stdio server 与 CLI 目标成功。
- 三工具链 parser symbols 均 `21 Tests / 0 Failures / 0 Ignored`；parity 均 `10/10`；hover
  `11/11`、inlay `13/13`；source contracts 输出 `PASSED`。上述测试真实 exit 0。
- 三工具链 interface 均 `111 Pass / 2 Fail`、真实 exit 1。固定 marker 为
  `LSP Class Member Navigation And Completion` 与
  `LSP Reference Call Diagnostic Is Published From Query Facts`；exact-type、unannotated return、
  extern function 与 extern type completion 用例全部 PASS。
- 三工具链 project 均 `56 Pass / 4 Fail`、process exit 0；四个固定 marker 为 imported
  constructor/meta call、network native receiver、native receiver callable parity 与 descriptor
  receiver provider generation，marker delta 0。
- 三工具链 full stdio 均在同一既有 generic fixture `short_circuit_unreachable` 断言处退出 1，
  本任务不声明 full stdio GREEN。CLI `--version` 分别输出 GNU 11.4、Clang 14 与 MSVC 19.44
  Debug 版本，真实 exit 0。
- WSL 与 Windows fixed snapshot 的 14 个 overlay 分别通过 SHA-256 `14/14`；
  `git diff --check` 通过。Windows source-contract snapshot 额外验证 LF byte hash，避免 archive
  行尾转换污染多行 source-contract 结论。

## 状态与产出记录

- 完成时间：2026-08-29 23:35 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：parser/LSP RED-GREEN、stable visible-symbol ordering、extern declarations、callable
  signature display、detached consumer、exact-type fail-closed、source-contract gate、三工具链
  focused/marker/CLI/stdio 已知阻塞分类与 overlay byte audit。
