# Plan 03 Task 7.21 Canonical Source Constructor Signature

## 目标

- 让 source class `new Type(...)` 与 struct `init Type(...)` 发布完整 canonical call facts。
- 让 source constructor signature help 在 compiler/symbol state 脱离后仍只消费
  `CallAt/FormatCall`。
- canonical payload 缺失时 fail closed，同时保留 native/imported constructor 的结构化 adapter。

## 完成项目

- Parser canonical consumer RED 固定 class/struct constructor 的 closed callable TypeId、
  receiver `NONE`、参数/命名实参、resolved SymbolId、完整 declaration range 与
  `@constructor(...)` display。
- Type inference 在 construct/struct-init 成功后发布 CALL expression/reference；优先消费已解析
  prototype constructor，bootstrap prototype缺member时通过source TypeDef resolver和精确
  `declarationNode`构造snapshot-local member contract。
- Constructor declaration fact按精确AST identity去重，`DeclarationOf(SymbolId)`与
  `CallAt.targetDeclarationRange`指向同一meta-function，不按member name推断。
- LSP source constructor canonical dispatch前移到compiler-state门禁之前；只有target SymbolId
  可追到source meta-function时才进入，payload移除后不落回request-time AST specialization。
- Native/imported constructor仍走既有structured metadata adapter；native exact-expression
  fail-closed回归保持GREEN。
- `type_inference_call_semantic_facts.c`保持低于1000行；source constructor临时member materializer
  已拆入253行cohesive module。

## 验证

- GCC/Clang/MSVC canonical consumers：`20/20`，真实exit 0。
- GCC/Clang/MSVC semantic query：`30/30`，真实exit 0。
- GCC/Clang/MSVC semantic-query source/binary/native parity：`9/9`，真实exit 0。
- GCC/Clang/MSVC source contracts：`65/65`，真实exit 0。
- 三工具链interface均`111 Pass / 2 Fail`、runner真实exit 1；两个既有marker为
  class-member navigation与reference-call diagnostic，本任务source/native constructor cases
  全部PASS，marker delta 0，不计GREEN。
- 三工具链project均`56 Pass / 4 Fail`、runner exit 0；四个既有imported/native producer
  marker集合不变，未用source fallback兼容。
- Full stdio仍有Task 7.20已记录的generic short-circuit diagnostic前置阻断，本项未计GREEN。

## 状态与产出记录

- 完成时间：2026-08-29 18:20 +08:00。
- 状态：已完成。
- 完成项目：source constructor canonical producer、resolved target/declaration identity、
  snapshot-only signature consumer、source fail-closed/native adapter边界、三工具链focused与
  fixed marker审计。
