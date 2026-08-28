# Plan 03 Task 7.16 Symbol-Table-Free Type Hierarchy Follow-Up

## 目标

- 让 source-local type hierarchy 的 super/subtype follow-up 脱离 LSP
  symbol table 和 position re-resolution。
- 只按 relation target SymbolId/TypeId、declaration identity 与 captured
  document version 投影 hierarchy items。
- 对 stale、unresolved 或 range-tampered item 保持 fail closed，不增加名称
  或 inheritance text fallback。

## 完成项目

- Follow-up 以 URI、version、SymbolId、TypeId、semantic type declaration
  range 与 `DeclarationOf` selection range 重解析 prepared item。
- Relation target 只通过 `BaseTypesOf`/`DerivedTypesOf` 的 stable ids 查找
  semantic type record，并校验 exact declaration fact 与 relation range。
- Item name 与 protocol kind 从已验证的 semantic type declaration 投影；
  删除 `semantic_type_hierarchy_find_symbol` 和 `symbolTable->allScopes` 遍历。
- Prepare 仍从请求 position 获取初始 local target；已准备 item 的 follow-up
  不再把 selection range 转回 position 重入 LSP semantic query。
- 新增 detached-symbol-table super/subtype runtime test、unresolved declaration
  fail-closed test，以及禁止 symbol-table fallback 的 source contract。

## 验证

- GCC/Clang/MSVC semantic-query parity：`9 Pass / 0 Fail`，真实 exit 0；
  保留 source、binary、native snapshot parity cases。
- GCC/Clang/MSVC source contracts：`65 Pass / 0 Fail`，真实 exit 0。
- GCC/Clang/MSVC advanced editor features：`73 Pass / 0 Fail`，真实 exit 0。
- GCC/Clang/MSVC combined type/call hierarchy stdio smoke：真实 exit 0。
- 三工具链 project features 均保持 `54 Pass / 6` 个既有 marker，runner
  exit 0，marker delta 0。
- 三工具链 full interface 均保持 `109 Pass / 4` 个既有 marker，集合不变，
  exit 1，不计 GREEN。
- Cross-project、binary/native external type hierarchy 仍等待 ModuleIdentity
  relation producer，本阶段未复用 type name 或 imported symbol aggregation。

## 状态与产出记录

- 完成时间：2026-08-29 03:07 +08:00。
- 状态：已完成。
- 完成项目：symbol-table-free type hierarchy follow-up、canonical relation
  target projection、detached/unresolved RED/GREEN、三工具链 focused/advanced/
  stdio 与固定 marker delta 验证。
