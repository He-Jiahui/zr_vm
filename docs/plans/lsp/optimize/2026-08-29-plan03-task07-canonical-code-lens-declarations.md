# Plan 03 Task 7.14 Canonical CodeLens Declarations

## 目标

- 让 CodeLens 从 immutable parser snapshot 枚举精确声明。
- 直接按 SymbolId 查询引用，不把声明 range 转回 position 后重入 LSP
  references consumer。
- 从大型 editor feature 文件提取 cohesive CodeLens 模块，同时保留
  compiler-owned test manifest lenses。

## 完成项目

- `DeclaredSymbols` 提供精确 SymbolId、TypeId、声明 AST identity 与 range；
  `ReferencesOf(SymbolId)` 提供 resolved reference facts。
- 删除 `symbolTable->allScopes` 遍历、symbol range lookup 与
  `ZrLanguageServer_Lsp_FindReferences` 回调链。
- 新增 symbol-table-detached runtime test，验证仍返回精确 `2 references`
  与 UTF-16 declaration range；版本更新后只消费当前 semantic snapshot，
  unresolved declaration identity 保持 unavailable。
- 新增 source contract，禁止 CodeLens 使用 symbol table、name lookup 或
  position-based references fallback。
- 当前 file-version AST 必须与 analyzer snapshot 一致；引用按当前
  AST scope 查询，同 source/range 重复 fact 只计一次，同 declaration AST 的
  重复 symbol row 只投影一次。
- 将 CodeLens 从 1192 行的 `lsp_editor_features.c` 提取到独立模块，原文件
  降至 918 行。

## 验证

- GCC/Clang/MSVC CodeLens focused：`4 Pass / 0 Fail`，真实 exit 0。
- GCC/Clang/MSVC source contracts：`65 Pass / 0 Fail`，真实 exit 0。
- GCC/Clang/MSVC advanced editor features：`73 Pass / 0 Fail`，真实 exit 0。
- GCC/Clang/MSVC project features 保持 `54 Pass / 6` 个既有 marker，
  watched-project CodeLens case 通过，runner 真实 exit 0，marker delta 0。
- 三工具链 full interface 保持 `109 Pass / 4` 个既有 marker，集合不变，
  真实 exit 1，不计 GREEN。
- WSL 与 MSVC snapshot 对 workspace code/test overlay 均为 `5/5`
  byte-exact。
- 本阶段仅完成 source-local CodeLens。跨项目 imported、binary 与 native
  declaration/reference count 必须由 ModuleIdentity relation producer 发布后
  再接入，不复用既有 name-keyed imported aggregation。

## 状态与产出记录

- 完成时间：2026-08-29 02:26 +08:00。
- 状态：已完成。
- 完成项目：canonical declaration enumeration、SymbolId reference count、
  no-symbol-table/stale/unresolved RED/GREEN、CodeLens 模块提取、三工具链
  focused/advanced 验证与既有 interface marker delta 复核。
