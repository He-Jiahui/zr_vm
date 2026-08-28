# Plan 03 Task 7.17 Canonical Hierarchy Prepare Acceptance

## 验收基线

- 基线 HEAD：`678fa6b4ebe6fe251f32041696553f0412b0957c`。
- RED：文档分析完成后、prepare 请求前移除 analyzer symbol table；旧实现的
  local type hierarchy 与 receiver-method call hierarchy prepare 失败，同一
  parity runner 其余 `7/9` 通过。
- GREEN：prepare 以文档 file position、snapshot source identity、parser
  `SymbolAt` 和 `DeclarationOf` 返回 exact type/callable item，symbol table
  保持不可用。
- Call duplicate semantic rows只按 declaration AST identity 归一；type row
  必须与 `SymbolAt` 的 SymbolId/TypeId/declaration node 完全一致。
- 两个 hierarchy 生产模块均不含
  `ZrLanguageServer_LspSemanticQuery_ResolveAtPosition`、`symbolTable` 或
  `allScopes`。

## 验收结果

- GCC/Clang/MSVC semantic-query parity：`9/9`。
- GCC/Clang/MSVC source contracts：`65/65`。
- GCC/Clang/MSVC advanced editor features：`73/73`。
- 三套 combined type/call hierarchy stdio smoke：真实 exit 0。
- Project features：三套均 `54 Pass / 6` 固定 marker，runner exit 0，delta 0。
- Full interface：三套均 `109 Pass / 4 Fail`，固定 marker delta 0，不计 GREEN。
- External hierarchy 仍为 ModuleIdentity relation-producer 边界，consumer 未按
  名称、token 或 metadata text 补齐。

## 状态与产出记录

- 完成时间：2026-08-29 03:22 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：symbol-table-free hierarchy prepare、canonical SymbolAt/
  DeclarationOf exactness、三工具链真实退出与固定 marker 审计。
