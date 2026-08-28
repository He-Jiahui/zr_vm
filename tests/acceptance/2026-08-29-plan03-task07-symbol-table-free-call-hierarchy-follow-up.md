# Plan 03 Task 7.15 Symbol-Table-Free Call Hierarchy Follow-Up Acceptance

## 验收基线

- 基线 HEAD：`4ebbadee990618417cd15434285e27c0a11754b8`。
- RED：prepare method items 后移除 analyzer symbol table，旧 follow-up 因
  `allScopes` 回查不可用而不能投影 canonical receiver method edge。
- GREEN：同一 fixture 在 symbol table 不可用时仍按 SymbolId/TypeId 返回
  一组 outgoing 与一组 incoming call，每组保留两个 exact call-site ranges。
- 将 prepared item 对应 declaration fact 标记 unresolved 后，follow-up 返回
  空；恢复 fact 后不污染 analyzer 生命周期。
- 生产文件不含 `symbolTable`、`allScopes` 或
  `semantic_call_hierarchy_find_symbol`，不按 name/type text/source scan 重建。
- Prepared item 的 version、URI、SymbolId、TypeId、declaration range 和
  selection range 任一不一致均 fail closed。

## 验收结果

- GCC/Clang/MSVC semantic-query parity：`9/9`。
- GCC/Clang/MSVC source contracts：`65/65`。
- GCC/Clang/MSVC advanced editor features：`73/73`。
- 三套 combined type/call hierarchy stdio smoke：真实 exit 0。
- Project features：三套均 `54 Pass / 6` 固定 marker，runner exit 0，delta 0。
- Full interface：三套均 `109 Pass / 4 Fail`，固定 marker delta 0，不计 GREEN。
- External cross-project/binary/native call hierarchy 仍为 producer 边界，未由
  本阶段补名称 fallback。

## 状态与产出记录

- 完成时间：2026-08-29 02:52 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：detached symbol-table runtime guard、unresolved declaration guard、
  canonical source contract、三工具链真实退出与固定 marker 审计。
