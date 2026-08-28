# Plan 03 Task 7.16 Symbol-Table-Free Type Hierarchy Follow-Up Acceptance

## 验收基线

- 基线 HEAD：`3dfbbdbf04b2036ee8183042bd6a4b995a641256`。
- RED：prepare `Base`/`Derived` items 后移除 analyzer symbol table，旧
  super/subtype follow-up 因 position query 和 `allScopes` target lookup 失败。
- GREEN：同一 fixture 在 symbol table 不可用时仍返回 exact `Base` supertype
  与 `Derived` subtype，identity 不受 display-name mutation 影响。
- 将 source item 的 declaration fact 标记 unresolved 后 follow-up 返回空；
  stale version 继续返回空。
- 生产文件不含 `symbolTable`、`allScopes` 或
  `semantic_type_hierarchy_find_symbol`，不扫描 inheritance header/type name。
- Item URI/version/SymbolId/TypeId/declaration range/selection range 任一不一致
  均 fail closed。

## 验收结果

- GCC/Clang/MSVC semantic-query parity：`9/9`。
- GCC/Clang/MSVC source contracts：`65/65`。
- GCC/Clang/MSVC advanced editor features：`73/73`。
- 三套 combined type/call hierarchy stdio smoke：真实 exit 0。
- Project features：三套均 `54 Pass / 6` 固定 marker，runner exit 0，delta 0。
- Full interface：三套均 `109 Pass / 4 Fail`，固定 marker delta 0，不计 GREEN。
- External type hierarchy 仍为 ModuleIdentity producer 边界，未由 consumer
  按名称补齐。

## 状态与产出记录

- 完成时间：2026-08-29 03:07 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：detached symbol-table relation projection、unresolved/stale guards、
  canonical source contract、三工具链真实退出与固定 marker 审计。
