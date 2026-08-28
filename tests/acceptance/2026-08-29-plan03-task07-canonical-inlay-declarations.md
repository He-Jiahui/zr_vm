# Plan 03 Task 7.10 Canonical Inlay Declarations Acceptance

## 验收基线

- 基线 HEAD：`e6d5a332113f0f556b327b12c568a6e8331bfbe7`。
- 生产合同：resolved declaration fact 的 SymbolId、TypeId、declaration range
  与 AST identity 必须和 canonical symbol record 一致。
- 消费合同：inlay hint 不得遍历 LSP symbol table、按名称查找 declaration，
  或在请求期间运行 type inference 补 facts。

## 验收结果

- GCC/Clang/MSVC parser query：`20/20`。
- GCC/Clang/MSVC focused inlay：`11/11`。
- GCC/Clang/MSVC source contracts：`61 Pass / 0 Fail`。
- GCC/Clang/MSVC focused stdio inlay smoke：真实 exit 0。
- Full interface：三套均为固定 `109 Pass / 4 Fail`，marker delta 0，不计
  GREEN。
- Full stdio：GCC 在本项场景前被既有 generic fixture
  `short_circuit_unreachable` diagnostic 缺失阻断，不计 GREEN。

## 状态与产出记录

- 完成时间：2026-08-29 00:40 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：exact declaration query、canonical inlay type/range projection、
  no-symbol-table/no-name/no-request-inference source contract、三工具链 focused
  runtime 与 stdio 证据。
