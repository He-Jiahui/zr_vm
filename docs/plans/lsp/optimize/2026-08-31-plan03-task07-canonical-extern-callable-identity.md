---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_extern_bindings.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
tests:
  - tests/parser/test_reference_fact_emission.c
  - tests/language_server/test_lsp_interface.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 7.56: Canonical Extern Callable Identity

## Scope

Make a source extern function keep one callable SymbolId, TypeId, overload set,
and exact declaration range across parser runtime/compile-time environments,
LSP symbols, and every call reference. No consumer may reconcile competing
extern identities by name or query ranking.

## TDD And Implementation

The parser RED first showed that `RegisterFunctionEx` did not publish an extern
declaration range: the resolved call returned declaration offset zero instead
of the exact `NativeAdd` token. After range publication, the LSP definition
passed but references exposed two resolved call identities, `6/2` and `1/2`.
Both semantic records pointed at the same extern AST and declaration offset.

The second identity came from delayed compiler extern predeclaration while the
first variable initializer was inferred in a child type environment. LSP and
compiler type conversion produced slightly different inferred payloads, so
signature comparison alone treated the same declaration as another overload.

Parser callable registration now derives the extern name range, publishes its
declaration fact, and rejects a duplicate declaration by exact AST identity or
exact source-aware range across the full parent environment chain. The LSP
symbols analyzer takes the runtime callable as canonical, injects its identity
into the compile-time binding, and assigns the same identity and overload set
to the LSP symbol. No name lookup, source scan, or query-priority fallback was
added to request handling.

The extern binding producer lives in the dedicated
`semantic_analyzer_extern_bindings.c` module. The general symbol collector
only consumes the canonical callable result and projects it into the symbol
table, keeping environment construction out of the already-large traversal
module.

The parser regression recreates runtime registration, canonical compile-time
injection, and delayed predeclaration from a child environment. It requires one
binding in each root environment, none in the child, and matching declaration
and call facts. The interface regression additionally requires runtime,
compile-time, symbol-table, and both call-site identities to match before it
checks definition, references, highlights, signature help, and completion.

## Verification

On the isolated fixed source snapshot, GCC and Clang each returned real exit
zero for:

- reference facts `7/7`, semantic facts `17/17`;
- semantic query/symbols/relations/calls/contract `30/22/23/30/6`;
- canonical consumers `21/21`, compiler diagnostics `64/64`, type inference
  `124/124`;
- LSP diagnostics/parity/property/source contracts `19/15/11/70`.

The GCC and Clang interface executables retained their expected nonzero parent
status while the extern navigation/signature case changed to pass. The known
failure set reduced exactly from fixed7 to fixed6. MSVC, the full 16-target
matrix, and stdio smoke were not run for this narrow producer repair.

## 状态与产出记录

- 完成时间：2026-08-31 13:10 +08:00。
- 状态：Task 7.56 子里程碑已完成；Plan 03 Task 7 继续进行。
- 完成项目：发布 extern exact declaration fact；按 AST/range 在 parent-chain
  去重 callable registration；统一 runtime/compile-time/LSP/call-site identity；
  提取独立extern binding producer；增强 parser 与 interface fact-level 回归；
  GCC/Clang focused 门禁；interface fixed7 降为 fixed6。
- 后续项目：继续修复其余六个 producer marker，补 source/binary/native parity，
  并完成 MSVC、16-target matrix 与 stdio smoke 总门禁。
