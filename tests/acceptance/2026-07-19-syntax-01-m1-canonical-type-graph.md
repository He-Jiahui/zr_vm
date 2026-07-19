# Syntax Plan 01 M1 Canonical Type Graph Acceptance

## Status

- State: completed.
- Plan: `docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md`, M1 Type graph.
- Scope: canonical type identity, typed generic arguments, interning, current inferred-type adapters, callable contracts, value-construction capability binding, compiler publication boundaries, structured member return types, and LSP `TypeId` formatting.

## Acceptance Inventory

- Canonical node coverage: primitive, nominal, generic parameter, generic instance, array, tuple, union, error, never, ref, owner, readonly view, nullable, and function.
- Structural identity: in-session structural hash, exact equality after hash match, verified collision-chain lookup, interning, monotonic `TypeId` lookup, and explicit lifecycle reset/free ownership.
- Callable identity: normalized parameter type/passing/escape/initialization/temporary/marker combinations, receiver effect, return type, and known effect bits.
- Generic contracts: generic arguments distinguish type, const literal, and open const parameter identities; open const parameters use owner symbol plus ordinal and recursively close through nested generic/union projections.
- Compatibility projection: current primitive, named, generic instance/parameter, array, tuple, union, ownership, nullable, and function paths publish canonical IDs; invalid kind, arity, or constraint checks publish no partial prototype, type-environment, symbol, or semantic-type state.
- Value construction: nominal or closed-generic capability plus a public recursively substituted constructor match is required, including open type/const arguments and projected unions; runtime reflection Type receives no implicit static construction path.
- Structured returns and LSP: source members retain structured inferred returns through closed generic prototypes; signature help specializes them structurally, while metadata/native members retain the compatibility string fallback. A real document reference fact formats tuple type `(int, bool)` from `TypeId` in hover.
- Stress: 100,000 unique IDs, sampled duplicate re-intern, repeated index growth, an observed collision-chain relookup, and formatting a 256-level constructed type.

## TDD Evidence

- Primitive identity initially failed because no canonical graph API existed, then passed after the first primitive interning slice.
- Each later node family, callable contract, construction capability, formatter, compatibility adapter, function registration, tuple AST, and LSP integration was added from a focused failing test.
- The final stress test timed out after five seconds with the full-table scan implementation (`timeout` exit 124).
- Review-driven RED cases reproduced nominal alias splitting, malformed callable acceptance, missing generic construction inheritance, use-site range leakage, generic-function nominal projection, invalid function publication, union compiler projection, rank-format ambiguity, open const identity loss, string-only member return specialization, non-atomic class/struct generic failures, const constructor substitution loss, and projected-union constructor mismatch.
- The same target passes within the five-second gate after indexed interning; the final target contains 18 tests.

## Regression Evidence

- `zr_vm_canonical_type_graph_test`: 18 tests, 0 failures.
- `zr_vm_parser_test`: 75 tests, 0 failures.
- `zr_vm_union_test`: 69 tests, 0 failures.
- `zr_vm_type_inference_test`: 118 tests, 0 failures.
- `zr_vm_semantic_facts_test`: 10 tests, 0 failures.
- `zr_vm_cfg_union_exhaustiveness_test`: 2 tests, 0 failures.
- `zr_vm_language_server_expression_fact_hover_test`: 6 cases passed, including TypeId formatting.
- `zr_vm_language_server_lsp_interface_test`: complete interface suite passed, including closed generic const signature help.
- `zr_vm_metadata_token_model_test`: 21 tests, 0 failures.
- `zr_vm_metadata_type_ref_binding_test`: 9 tests, 0 failures.
- `zr_vm_zrp_metadata_format_test`: 13 tests, 0 failures.
- `zr_vm_aot_c_value_construction_guardrail_test`: 3 tests, 0 failures on GCC and Clang; MSVC passed the Windows-applicable test and ignored two Unix-only execution gates.

## Compiler Matrix

- GCC 11.4.0, WSL Debug: fresh build in `/home/hejiahui/zr_vm-syntax-m1-gcc-20260719-r8`; all listed targets and binaries passed.
- Clang 14.0.0, WSL Debug: fresh build in `/home/hejiahui/zr_vm-syntax-m1-clang-20260719-r8`; all listed targets and binaries passed.
- MSVC 19.44.35228 x64 Debug: `build-syntax-01-m1-msvc`; all listed targets and binaries passed.

## Boundaries

- M1 keeps `SZrInferredType` as the compatibility input; it does not claim its removal.
- M1 does not serialize the graph into `.zrs/.zri/.zro`; artifact schema and loader validation remain M4.
- M1 adds canonical identity beside current union execution lowering; M2/M3 replace AST-derived Place/operation decisions rather than duplicating them in this milestone.
- Warnings observed in broad parent builds originate in pre-existing, unrelated worktree sources; the new canonical type units compile without new warnings.

## Final Gate

- Compiler matrix: passed.
- Final Critical/Important review: GO, 0 Critical and 0 Important.
- Documentation and diff checks: passed before scoped staging.
- Milestone commit: recorded in `docs/plans/syntax/01-canonical-type-place-cfg-artifact/m1-type-graph.md` after creation.
