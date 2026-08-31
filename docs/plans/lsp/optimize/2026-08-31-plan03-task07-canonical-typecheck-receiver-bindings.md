---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck_bindings.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
tests:
  - tests/language_server/test_lsp_interface.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 7.57: Canonical Typecheck Receiver Bindings

## Scope

Make the LSP typecheck pass reuse the canonical `this` and `super` symbols
created during symbol collection. Instance method and constructor bodies must
infer through the exact owner/base prototypes, and super-constructor arguments
must be checked only after their parameter bindings enter the same scope.

## TDD And Implementation

The existing class-member interface RED stopped before navigation with three
diagnostics: `this.hp` could not infer an exact type, `super(seed)` compared
`object` against `int`, and inherited `this.hp` reported `member_not_found`.
The parser already performs recursive inherited-member lookup. The missing
state was in the separate LSP typecheck pass: it opened a fresh callable type
environment, registered only parameters, and recorded super-constructor facts
before even those parameters existed.

`semantic_analyzer_typecheck_bindings.c` now recognizes instance class/struct
methods and meta functions from AST kinds and static flags. It resolves the
existing implicit receiver symbols from the symbol table and registers their
exact SymbolId, TypeId, and source range in the fresh typecheck environment.
Missing canonical symbols fail closed; the helper never creates an ordinary
replacement binding. `super` uses the current prototype's structured
`extendsTypeName`. Super-constructor fact publication now runs after receiver
and parameter registration.

With diagnostics removed, the old fixture reached a second RED: it compared
the class declaration identity to the canonical constructor-call identity at
`new BossHero(...)`. Plan 03 Task 7.21 requires that call to target the exact
constructor declaration. The class navigation assertion therefore now uses
an explicit `boss: BossHero` type reference, while existing constructor tests
continue to own constructor-call navigation.

## Verification

On isolated GCC and Clang builds:

- the class member navigation/completion interface case passes with zero
  fixture diagnostics;
- semantic query diagnostics pass `19/19`, parity passes `15/15`, property
  contracts pass `11/11`, and all 70 LSP source-contract checks pass;
- both interface executables retain expected real exit 1 while the known
  failure set reduces exactly from fixed6 to fixed5.

The broad semantic-analyzer executable retains unrelated pre-existing
fact/ownership/generic failures and is not counted as a green gate. MSVC, the
full 16-target matrix, and stdio smoke were not run for this narrow LSP scope
repair.

## 状态与产出记录

- 完成时间：2026-08-31 13:35 +08:00。
- 状态：Task 7.57 子里程碑已完成；Plan 03 Task 7 继续进行。
- 完成项目：为独立typecheck scope注册canonical `this/super` binding；在参数注册后发布
  super-constructor facts；保留parser继承member递归查找；校正class type reference与
  constructor call identity测试边界；GCC/Clang focused门禁；interface fixed6降为fixed5。
- 后续项目：修复其余五个producer marker，补source/binary/native parity，并完成MSVC、
  16-target matrix与stdio smoke总门禁。
