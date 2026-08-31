---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
tests:
  - tests/language_server/test_lsp_interface.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 7.55: Canonical Local Binding Identity

## Scope

Make LSP-created type-environment bindings reuse the canonical symbol and type
identity already registered for the same source declaration. A local use must
not publish a second resolved SymbolId merely because type inference needs a
binding for the declaration.

## TDD And Implementation

The existing Web URI navigation case was the RED. At the `x` use in
`var x = 10; var y = x`, the semantic snapshot contained one declaration for
SymbolId 1, one orphan resolved read for SymbolId 2, and two later reads for
SymbolId 1. The orphan came from registering the type binding through
`RegisterVariable` after the LSP symbol had already received canonical identity.
`SymbolAt` selected the first equal-range read, so definition lookup could not
find a declaration for SymbolId 2.

The symbols analyzer now passes an available `SZrSymbol` to its internal
type-binding helper. When SymbolId, TypeId, and declaration range are complete,
the helper uses `ZrParser_TypeEnvironment_RegisterCanonicalVariable`; temporary
inference scopes without a declared symbol retain ordinary registration.
Variables, parameters, foreach bindings, implicit runtime symbols, and
property setter/init parameters therefore share the declaration identity
already published to the snapshot. No query ranking, name lookup, source-text
scan, or URI-specific fallback was added.

The Web URI test now also inspects the borrowed reference facts at the use
range. Every resolved read must match the SymbolId returned by `SymbolAt`, and
that symbol must retain the exact declaration range.

## Verification

On the isolated fixed source snapshot, GCC and Clang each returned real exit
zero for:

- semantic-query symbols `22/22`;
- semantic-query parity `15/15`;
- LSP semantic-query diagnostics `19/19`;
- property consumer contracts `11/11`;
- LSP source contracts `70/70`.

The GCC and Clang interface executables both returned their expected nonzero
parent status while the Web URI navigation case changed from fail to pass.
The failure set reduced exactly from fixed8 to fixed7; the remaining seven
known producer markers were unchanged. MSVC, the full 16-target matrix, and
stdio smoke were not run for this narrow identity repair.

## 状态与产出记录

- 完成时间：2026-08-31 11:37 +08:00。
- 状态：Task 7.55 子里程碑已完成；Plan 03 Task 7 继续进行。
- 完成项目：复用 local declaration 的 canonical SymbolId/TypeId/range 注册
  type binding；移除变量、参数、foreach、隐式 runtime 与 property accessor
  参数的第二身份来源；强化 Web URI fact-level regression；GCC/Clang
  `22/15/19/11/70` focused 门禁；interface fixed8 降为 fixed7。
- 后续项目：继续修复其余七个 producer marker，补 source/binary/native parity，
  并完成 MSVC、16-target matrix 与 stdio smoke 总门禁。
