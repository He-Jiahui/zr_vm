# Plan 03 Task 7.19 Canonical Callable-Value Shadow Acceptance

## 验收基线

- 基线HEAD：`82633faa05476ab300220b8175bec728721a1c7c`。
- RED：删除source direct-function fallback后，现有canonical-consumer测试中identifier callable
  alias与lambda callable两项因`hasVisibleVariable`阻断runtime function metadata而失败，
  `CallAt`没有exact `ZR_SEMANTIC_REFERENCE_CALL`。
- GREEN：callable-value metadata在同scope变量遮蔽下仍参与call resolution，alias/lambda重新发布
  canonical TypeId、SymbolId、declaration range与`FormatCall`文本。
- Ordinary nullable callable variable仍遮蔽同名named function；named function redundant optional
  call仍拒绝。LSP没有恢复callee-name、symbol-table、initializer AST或argument-count fallback。
- 不修改Syntax05 property路径、ownership外部测试、binary/native metadata provider或semantic-query
  public schema。

## 验收结果

- 三工具链parser/query矩阵：canonical consumers `19/19`、type inference `124/124`、
  semantic query `30/30`、compiler integration `127/127`，全部真实exit 0。
- 三工具链LSP focused：semantic-query parity `9/9`、source contracts `65/65`，全部真实exit 0。
- GCC/Clang ownership shadowing `44/44`。MSVC external 47-case A/B均只保留同一Weak
  callable marker，证明本次overlay没有新增失败。
- Full interface三套均`111 Pass / 2 Fail`且新增canonical signature用例全部PASS；两个外部
  marker集合一致，runner真实exit 1，不计GREEN。
- `git diff --check`与source fallback search通过；exact-path audit排除所有外部dirty路径。

## 状态与产出记录

- 完成时间：2026-08-29 16:09 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：parser support-first RED/GREEN、canonical signature fallback deletion、三工具链
  focused真实退出、ownership/interface固定marker A/B、module/plan/acceptance记录。
