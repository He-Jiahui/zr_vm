# Plan 03 Task 7.14 Canonical CodeLens Declarations Acceptance

## 验收基线

- 基线 HEAD：`0b3804126db2c4cc8a8cbac62f370780facab521`。
- RED：移除 analyzer symbol table 后 CodeLens 请求成功但返回 `0` 个 lens；
  source contract 因 canonical CodeLens 模块不存在而失败。
- GREEN：同一 fixture 在 symbol table 不可用时返回 helper 声明上的精确
  `2 references`，UTF-16 range/position 均为 `0:3`。
- 版本 2 snapshot 从两个引用收敛到一个引用后只返回 `1 reference`；将
  declaration fact 标记 unresolved 后不返回该 lens。
- 生产 consumer 只调用 `DeclaredSymbols` 与 `ReferencesOf(SymbolId)`；不含
  `allScopes`、`symbolTable`、`FindReferences` 或 symbol name lookup。
- Snapshot AST identity 不匹配时 fail closed；当前 scope 内重复 reference
  range 与重复 declaration AST 各只投影一次。

## 验收结果

- GCC/Clang/MSVC focused CodeLens：`4/4`。
- GCC/Clang/MSVC source contracts：`65/65`。
- GCC/Clang/MSVC advanced editor features：`73/73`。
- Project features：`54 Pass / 6` 个固定 marker，watched-project CodeLens
  case 通过，marker delta 0。
- Full interface：`109 Pass / 4 Fail`，marker delta 0，不计 GREEN。
- Workspace/WSL/MSVC code-test bytes：`5/5`。
- 跨项目 imported、binary/native declaration count 留待 ModuleIdentity relation
  producer，未由本阶段的 source-local consumer 按名称补齐。

## 状态与产出记录

- 完成时间：2026-08-29 02:26 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：symbol-table-detached、stale snapshot、unresolved identity runtime
  guards，canonical-query source contract、既有 test-manifest/UTF-16 regressions、
  三工具链真实进程退出与 byte-exact 审计。
