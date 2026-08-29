# Plan 03 Task 7.22 Canonical Super Constructor Signature Acceptance

## 验收基线

- 固定基线 HEAD：`da114f9`；验收只叠加 10 个 Task 7.22 code/test paths。
- Parser RED：class meta-function 的 `super(...)` 没有 canonical CallAt、callable TypeId 或
  resolved base-constructor declaration identity。
- LSP RED：旧 signature helper 在 request-time 搜索 base prototype/constructor 并本地格式化；
  脱离 compiler state 后不可用。
- GREEN：parser 发布同一 CALL/REFERENCE_CALL 合同；LSP 仅消费 `CallAt/FormatCall`，
  payload 移除后 fail closed。

## 验收结果

- GCC 11.4、Clang 14、MSVC 19.44 fixed snapshot 均构建 canonical consumer、source contract、
  interface、stdio server、CLI 及两套 descriptor fixture 成功。
- 三工具链 canonical consumers 均 `21 Tests / 0 Failures / 0 Ignored`，真实 exit 0；source
  contracts 均输出 `PASSED` 且真实 exit 0。
- 三工具链 interface 均 `111 Pass / 2 Fail`、真实 exit 1。两个固定外部失败为
  `LSP Class Member Navigation And Completion` 与
  `LSP Reference Call Diagnostic Is Published From Query Facts`；super signature、definition、
  references、document highlights 目标用例均 PASS。
- 三工具链 full stdio 均在同一既有 generic fixture `short_circuit_unreachable` 断言处退出 1，
  本任务不声明 full stdio GREEN；CLI `--version` 分别输出 GNU、Clang、MSVC Debug 版本并
  真实 exit 0。
- WSL fixed source 与 Windows fixed source 的 10 个 overlay 分别通过 SHA-256 `10/10`；
  `git diff --check` 通过。

## 状态与产出记录

- 完成时间：2026-08-29 20:59 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：parser/LSP RED-GREEN、exact super call range、closed constructor callable TypeId、
  stable SymbolId/declaration range、detached snapshot consumer、fact-removal fail-closed 与
  三工具链 marker 审计。
