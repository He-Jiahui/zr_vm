# Plan 03 Task 7.24 Canonical Source Hover Acceptance

## 验收基线

- 基线 HEAD：`935d44b1428c06ee2b50c9e0565e09d117979705`；验收只叠加 10 个 Task 7.24
  code/test paths。
- RED：移除 analyzer symbol table、reference tracker 与 AST 后，source-local hover 原 consumer
  无法构建；SymbolAt 未直接携带 exact reference range。
- GREEN：semantic query 复制 parser `SymbolAt` view，独立 projector 只消费 SymbolId、TypeId、
  signature、declaration identity 与 resolved reference range。

## 验收结果

- GCC 11.4、Clang 14、MSVC 19.44 semantic-query parity 均 11/11，新增 detached source hover
  case PASS；parser semantic-query symbols 均 `21 Tests / 0 Failures / 0 Ignored`。
- GCC/Clang source contracts 输出 `PASSED`；MSVC Task724 原缓存 source-contract 真实 exit 0。
  隔离 HEAD snapshot 的两条 constructor source-contract marker 来自未叠加的既有 Task 7.21/7.22
  工作区事实，与 Task 7.24 source-hover gate 无关。
- 三工具链 interface 均 `111 Pass / 2 Fail`、真实 exit 1。固定 marker 为
  `LSP Class Member Navigation And Completion` 与
  `LSP Reference Call Diagnostic Is Published From Query Facts`；source hover、extern type hover、
  extern layout metadata hover 全部 PASS，新增 marker 0。
- MSVC 使用隔离 `HEAD + 10 exact overlays` snapshot，避开并行 benchmark 会话对
  `tests/CMakeLists.txt` 的未完成修改；GCC/Clang 使用固定 WSL source/cache。所有 focused test
  均直接执行，未使用会被 PowerShell 展开的 bash `$?` wrapper。
- `git diff --check` 通过；生产代码中不存在临时 `canonical-hover`/`extern-scan` 调试输出。

## 状态与产出记录

- 完成时间：2026-08-30 01:45 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：parser/LSP RED-GREEN、exact range、detached source hover、source-contract gate、extern
  FFI metadata regression closure、三工具链 parity/parser/interface 验证与既有 marker 分类。
