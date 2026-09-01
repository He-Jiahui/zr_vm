---
related_code:
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_canonical_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_canonical_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
plan_sources:
  - user: 2026-08-13 require provider and binary callable values to publish canonical facts without fabricated source identity
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_native_callable_signature_cases.h
  - tests/parser/test_canonical_consumers.c
  - tests/acceptance/2026-08-14-lsp-l8-external-callable-value-canonical-facts.md
doc_type: module-detail
---

# External Callable-Value Canonical Facts

## Purpose

Binary metadata and native descriptor providers can expose callable members even when no source declaration, AST declaration node, or source `SymbolId` exists. Assigning such a member to a local value must preserve its callable type and display contract without inventing source identity or asking the LSP to reconstruct the callable from a member name.

This module boundary publishes the callable once in parser-owned semantic state. Hover and signature help then consume the same canonical `CallAt` fact as source calls.

## Fact And Identity Contract

An external callable value retains:

- a canonical function `TypeId`;
- the resolved receiver `TypeId` when the call is a member call; free calls keep it invalid;
- structured parameter/return types and passing modes;
- the provider-derived canonical signature display;
- the exact local call target range.

It deliberately retains no resolved source target:

- `reference.isResolved == false`;
- `reference.symbolId == ZR_SEMANTIC_ID_INVALID`;
- `CallAt.hasResolvedTarget == false`;
- `targetDeclarationRange` is empty.

Binary `.zro` coordinates and descriptor-provider generations remain owned by their metadata adapters. The parser does not copy those coordinates into a fake source declaration.

## Data Flow

```text
imported module member metadata
  -> exact external member reference fact
  -> dedicated external-callable contract table
  -> local callable alias in the type environment
  -> primary call inference
  -> canonical CALL reference + expression fact
  -> SemanticQuery_CallAt / FormatCall
  -> LSP hover and signature help
```

The dedicated external table owns one deep-copied callable contract. Registering a local alias copies that contract into the normal callable lookup array while preserving `isExternalCallable`. The call reference therefore remains unresolved even though overload resolution has a complete callable type.

The alias registration gate is parser-owned: a reference must carry the complete external owner,
metadata token, signature token, and signature hash before it can register a callable contract. A
member-call fact additionally carries the canonical receiver `TypeId`; a free or constructor call
must leave that field invalid. `CallAt` and `FormatCall` consume this same fact for source,
`.zro`, and native descriptor callables.

## Fail-Closed Rules

The LSP does not search by descriptor name, local variable name, inferred display type, or initializer AST. A direct external callable alias first resolves through canonical `CallAt/FormatCall`. If the expression call payload is unavailable, signature help rejects the legacy callable-environment fallback by locating only the exact fact-owned call target range.

This guard is intentionally narrower than a general function-type check. Native member calls and provider metadata paths that are not owned by this exact call fact continue through their existing structured adapters.

Receiver projection follows the same boundary. A construct or chained receiver with no exact expression fact cannot be re-inferred from its AST. A binary property chain may proceed only when `PropertyAt` has already published the exact property identity and type; property/accessor names do not authorize recovery.

For native external receiver calls, the generic receiver hover projection declines facts marked
`hasExternalTarget`; the structured metadata provider then owns the hover and signature contract.
This routing is identity-based and does not recover a target from a member name or display text.

## Lifetime And Memory

External callable contracts are deep-copied because parameter and return inferred types own nested arrays. Type-environment destruction frees both the external contract table and local aliases through the same function-info payload destructor. Strings remain state-managed objects and are borrowed by the table.

Duplicate registration requires the same canonical `TypeId`, name, and signature display. Alias lookup cannot manufacture a contract when the external table does not already contain the exact identity.

## Test Coverage

Project tests cover both a generated binary metadata module and a reloadable descriptor plugin:

- assigning an imported function member to a local callable value;
- exact canonical `TypeId` and signature display;
- unresolved reference identity and empty declaration range;
- signature help and hover consuming the same fact and range;
- clearing the expression call payload makes `CallAt`, formatter, and signature help fail closed;
- existing native/provider receiver and module callable parity remains green.

`test_canonical_consumers.c` remains the lower canonical-query regression gate. The project runner must be checked for Unity `Fail -` markers because its process currently returns zero even when an individual project case fails.

All three supported toolchains completed this coverage on 2026-08-22: canonical consumers `19/19`, semantic facts `14/14`, semantic query `29/29`, local hover, interface, project, and stdio/CLI all had zero Unity failure markers. Peak stdio working sets were `33.08 MiB` (GCC), `32.32 MiB` (Clang), and `39.13 MiB` (MSVC), below the `512 MiB` budget.

The 2026-09-01 callable-parity slice was revalidated in fresh GCC and Clang snapshots. Parser
semantic-query symbols passed `24/24`; the focused parser/query/LSP matrix passed with real exit 0,
and both project runners passed all four new callable-value/receiver cases. The runners still
reported 14 unrelated historical markers, so this slice does not claim a full project or stdio
matrix. MSVC and the complete Task 8 acceptance remain pending.

## Open Work

- The oversized `type_system.c` can later extract external callable storage into a cohesive type-environment module; this change keeps the existing ownership boundary to avoid a cross-cutting refactor during L8 convergence.
- Any shared-library MSVC test seam that directly reaches non-exported metadata-provider internals remains separate infrastructure work; this leaf is validated with the repository's supported static Debug configuration.
- Full L8 fallback deletion and protocol/performance acceptance remain separate milestones.
