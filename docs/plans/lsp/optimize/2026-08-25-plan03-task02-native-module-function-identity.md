---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.h
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 2.3a: Native Module Function Identity

## Goal

Publish a canonical snapshot identity for a resolved non-generic native module
function call. The identity must come from the resolved `SZrTypeMemberInfo` and
the parser semantic context, not from module text, member spelling, or an LSP
lookup.

## Implementation

- `type_inference_member_symbol_id` reuses an existing member `symbolId` and
  otherwise registers one function symbol against the resolved closed callable
  `TypeId`.
- A source declaration continues to use its declaration AST range. A native
  descriptor has no declaration AST, so the registered symbol and the call
  reference fact use an explicit zero `SZrFileRange` rather than the call-site
  range.
- The member record caches the resulting snapshot `SymbolId`; repeated calls
  consume the same canonical identity during that semantic context lifetime.
- Native members with generic parameters remain fail-closed. This slice does
  not bind an open native declaration identity to the first closed call.

## Contract

For:

```zr
var math = import("zr.math");
fn probe(): float { return math.abs(-3.0); }
```

the `CALL` reference fact at `abs` is resolved and has valid `SymbolId` and
closed callable `TypeId`. `ZrParser_SemanticQuery_SymbolAt` returns those same
ids. Both declaration ranges are zero because the native descriptor has no
source declaration. No LSP, AST, or member-name fallback is involved.

## Verification

- RED: the resolved `math.abs` call published no usable `CALL` reference fact,
  so the native identity test failed before `SymbolAt`.
- GREEN: the parser call-fact producer publishes the resolved fact and the
  query returns its canonical identity.
- MSVC static: symbols 18/18, semantic query 29/29, query contract 3/3,
  compiler diagnostics 46/46, and compiler integration 127/127 passed with
  zero failures and real process exit zero.
- Windows GCC 4.8.3 remains blocked before parser compilation by the existing
  `_Thread_local` incompatibility in `zr_vm_core/include/zr_vm_core/profile.h`.
  No GCC test result is claimed.
- No local Clang executable or WSL distribution is available. No Clang result
  is claimed.

## 状态与产出记录

- 完成时间：2026-08-25 18:31:18 +08:00
- 状态：MSVC native non-generic module-function producer 子里程碑完成；
  GCC/Clang executable gate 因环境不可用或既有工具链限制保持未完成，未做
  跨工具链通过声明。
- 完成项目：native resolved call 的 stable SymbolId/closed callable TypeId、
  空 source declaration range、SymbolAt identity parity、generic native
  fail-closed boundary、RED/GREEN、MSVC focused query/diagnostic/compiler-
  integration regression、模块文档与验收记录。
- 后续项目：native generic declaration identity、binary metadata symbol and
  visible-fact parity、external origin URI/relation facts，以及 Task 2 LSP
  consumer migration。
