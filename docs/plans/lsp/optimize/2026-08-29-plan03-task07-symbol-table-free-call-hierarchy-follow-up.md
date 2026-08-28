# Plan 03 Task 7.15 Symbol-Table-Free Call Hierarchy Follow-Up

## 目标

- 让已准备的 source-local call hierarchy item 只凭 immutable semantic
  snapshot identity 完成 incoming/outgoing follow-up。
- 删除 call hierarchy 对 LSP `symbolTable->allScopes` 的展示符号回查。
- 保持 stale、unresolved、range-tampered item fail closed，且不增加名称、
  类型文本或源码扫描 fallback。

## 完成项目

- Follow-up 以 URI、document version、SymbolId、callable TypeId、
  `DeclarationOf`、semantic symbol declaration range 和 selection range
  重解析 item。
- Related call item 的名称、kind、声明范围和选择范围来自同一 parser
  semantic symbol/reference snapshot；incoming/outgoing 只消费
  `IncomingCalls`、`OutgoingCalls` 和 exact declaration identity。
- 删除 `semantic_call_hierarchy_find_symbol`、`symbolTable`、`allScopes`
  遍历和 position-based follow-up query；lambda 不再需要独立 symbol-table
  失败回退。
- 保留 prepare 阶段 exact declaration AST identity 归一化，以处理同一声明
  的 compiler callable row 与 LSP display row；不按名称选择记录。
- 新增 symbol-table-detached method follow-up 与 unresolved declaration
  fail-closed runtime 覆盖；source contract 阻止上述回查重新进入生产代码。

## 验证

- GCC/Clang/MSVC semantic-query parity：`9 Pass / 0 Fail`，真实 exit 0；
  其中保留 source、binary、native snapshot parity cases。
- GCC/Clang/MSVC source contracts：`65 Pass / 0 Fail`，真实 exit 0。
- GCC/Clang/MSVC advanced editor features：`73 Pass / 0 Fail`，真实 exit 0。
- GCC/Clang/MSVC combined type/call hierarchy stdio smoke：真实 exit 0。
- 三工具链 project features 均保持 `54 Pass / 6` 个既有 marker，runner
  真实 exit 0，marker delta 0。
- 三工具链 full interface 均保持 `109 Pass / 4` 个既有 marker，集合不变，
  真实 exit 1，不计 GREEN。
- 跨项目、binary/native external call hierarchy 仍等待 ModuleIdentity relation
  producer；本阶段不复用 name-keyed import aggregation。

## 状态与产出记录

- 完成时间：2026-08-29 02:52 +08:00。
- 状态：已完成。
- 完成项目：symbol-table-free hierarchy follow-up、canonical item re-resolution、
  detached/unresolved RED/GREEN、三工具链 focused/advanced/stdio 验证及固定
  interface/project marker delta 复核。
