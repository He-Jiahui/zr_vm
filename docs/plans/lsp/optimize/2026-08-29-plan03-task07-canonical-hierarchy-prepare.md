# Plan 03 Task 7.17 Canonical Hierarchy Prepare

## 目标

- 让 source-local call/type hierarchy 的 prepare 请求脱离 LSP symbol table。
- 从当前文档坐标直接查询 parser `SymbolAt`，再用 declaration identity 选择
  canonical semantic record。
- 缺少 resolved SymbolId、TypeId、declaration node 或 declaration fact 时
  fail closed，不按名称、token 或 AST pairing 重建。

## 完成项目

- Call/type hierarchy prepare 先获取当前 analyzer snapshot，将 LSP position
  转为带 source identity 的 zero-width file range，再调用
  `ZrParser_SemanticQuery_SymbolAt`。
- Call prepare 继续按 exact declaration AST identity 归一同一 callable 的重复
  semantic rows，并以 `DeclarationOf` 校验最终 SymbolId/TypeId/range。
- Type prepare 只接受 `SymbolAt` 返回的 exact type record 和匹配的
  `DeclarationOf` fact。
- 删除两个 prepare 对 `LspSemanticQuery_ResolveAtPosition` 和 LSP symbol kind
  的依赖；follow-up 的 stable identity/range/version 合同保持不变。
- 测试在 prepare 前移除 analyzer symbol table；旧实现仅 type/method prepare
  两项 RED，新实现恢复 exact hierarchy items。

## 验证

- GCC/Clang/MSVC semantic-query parity：`9 Pass / 0 Fail`，真实 exit 0。
- GCC/Clang/MSVC source contracts：`65 Pass / 0 Fail`，真实 exit 0。
- GCC/Clang/MSVC advanced editor features：`73 Pass / 0 Fail`，真实 exit 0。
- 三套 combined type/call hierarchy stdio smoke：真实 exit 0。
- 三工具链 project features 均保持 `54 Pass / 6` 个既有 marker，runner
  exit 0，marker delta 0。
- 三工具链 full interface 均保持 `109 Pass / 4` 个既有 marker，集合不变，
  exit 1，不计 GREEN。
- Cross-project、binary/native external hierarchy 仍等待 ModuleIdentity relation
  producer；本任务未增加名称或 metadata text fallback。

## 状态与产出记录

- 完成时间：2026-08-29 03:22 +08:00。
- 状态：已完成。
- 完成项目：canonical `SymbolAt` hierarchy prepare、detached-symbol-table
  RED/GREEN、declaration identity 校验、三工具链 focused/advanced/stdio 与固定
  marker delta 验证。
