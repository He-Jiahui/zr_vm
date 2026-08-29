# Plan 03 Task 7.19 Canonical Callable-Value Shadow

## 目标

- 删除 source direct-function signature help 的 compiler overload、symbol-table、callee-name
  与 initializer AST fallback。
- 让 source callable alias 与 lambda 在正常 lexical shadowing 下仍发布 canonical
  `CallAt/FormatCall` facts。
- 保持普通变量遮蔽同名函数、named function optional-call拒绝与 external callable
  canonical contract不变。

## 完成项目

- `SZrFunctionTypeInfo` 结构化区分普通函数元数据与 callable-value binding。
- Lambda、source identifier alias 与 external callable alias 使用 callable-value registration；
  duplicate identity同时包含binding kind。
- Function lookup逐scope处理同名变量：只允许当前scope的callable-value metadata，并停止
  parent函数搜索；普通变量继续遮蔽普通函数。
- Primary call inference始终请求runtime function metadata，由type environment统一执行shadowing；
  prototype/compile-time lookup仍在可见变量存在时关闭。
- `lsp_signature_help.c` 删除source direct-function overload resolution、symbol-table position
  lookup、initializer identifier extraction与argument-count candidate fallback，未命中canonical
  query时直接fail closed。
- Source contract禁止上述三类fallback入口重新出现。
- 新增module文档说明binding、scope、identity、lifetime与LSP fail-closed合同。
- 大型 `type_system.c` 只修改其既有type-environment职责；本切片不做无关拆分。
  大型 `lsp_signature_help.c` 净删除357行legacy fallback与dead helper，没有迁移到新大文件。

## 验证

- GCC/Clang/MSVC canonical consumers：`19/19`，真实 exit 0。
- GCC/Clang/MSVC type inference：`124/124`，真实 exit 0。
- GCC/Clang/MSVC semantic query：`30/30`，真实 exit 0。
- GCC/Clang/MSVC compiler integration：`127/127`，真实 exit 0。
- GCC/Clang/MSVC semantic-query parity：`9/9`，真实 exit 0。
- GCC/Clang/MSVC source contracts：`65/65`，真实 exit 0。
- GCC/Clang ownership shadowing基线：`44/44`，真实 exit 0。MSVC当前外部47-case
  snapshot在parent与overlay均为同一`46 Pass / 1 Fail`，唯一marker为
  `test_weak_callable_optional_and_direct_call_contracts`，delta 0，不计GREEN。
- 三工具链interface均为`111 Pass / 2 Fail`，两个外部overlay marker固定为
  class-member navigation和reference-call diagnostic；本任务direct/callable/lambda/receiver
  canonical signature cases全部PASS，full runner真实exit 1，不计GREEN。
- Full stdio仍会在本任务signature场景前被既有generic short-circuit diagnostic缺失阻断，
  未用marker白名单或LSP fallback绕过，也未计GREEN。

## 状态与产出记录

- 完成时间：2026-08-29 16:09 +08:00。
- 状态：已完成。
- 完成项目：callable-value binding identity、lexical function shadowing、canonical call fact
  restoration、source signature compiler/name fallback删除、三工具链focused与固定marker审计。
