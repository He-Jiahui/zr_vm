# Syntax Plan 01 M4 Canonical Artifact Schema Acceptance

## Status

- State: completed; implementation, compatibility regressions, staged-snapshot matrix, and final Critical/Important review passed.
- Plan: `docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md`, M4 artifact schema.
- Scope: versioned `.zrs/.zri/.zro` envelopes, canonical public type/contract identity, bounded decoding, readable text projection, and canonical source/binary type projection.

## Acceptance Inventory

- Fixed binary contract: `ZRAF` magic, schema version 1, a 112-byte little-endian header, 24-byte section-directory entries, and explicitly encoded fixed-width rows.
- Artifact responsibilities: `.zrs` carries syntax/source state with zero public identity, `.zri` carries readable canonical semantic state, and `.zro` carries executable public tables/code/relocations without AST pointers or local flow facts.
- Stable tables: StringHeap, TypeDef, TypeRef, TypeSpec, MemberDef, PropertyDef, SignatureHeap, Contract, Layout, Code, Relocation, Debug, SyntaxTree, and SemanticIR.
- Stable public identity: TypeRef, TypeSpec, structural signature, layout version/hash, callable contract, and module hashes remain independent.
- Signature grammar: primitive, TypeDef, generic parameter/instance, typed const arguments, array, tuple, union, nullable, function, ref, readonly view, exact owner kind, never, and error.
- Callable contract: receiver/ref-export effects and full parameter passing, escape, initialization, temporary, and call-site contracts are encoded independently from surface delimiters.
- Type capabilities: value, GC, resource, readonly, ref-like, drop, value-constructible, public constructor token/signature/contract, layout ownership map, and StableSlotSource contract.
- Parser bridge: canonical `TypeId` graphs serialize recursively to signature bytes and binary signatures re-intern through the canonical graph.
- Text projection: readable `.zrs/.zri` section summaries and literal syntax/SemIR content roundtrip exactly through a deterministic final `payload-hex` envelope.

## TDD And Safety Evidence

- The initial focused target failed because `zr_vm_core/artifact_schema.h` did not exist.
- Final `zr_vm_artifact_schema_test`: 13 tests, 0 failures.
- Roundtrip coverage includes `.zrs`, `.zri`, and `.zro`, repeat encoding, fixed widths, zero and 256 rows, duplicate signatures, value construction metadata, and real-source projection.
- Mismatch coverage checks exact TypeRef, TypeSpec, signature, layout version, layout hash, callable contract, and module status plus expected/actual diagnostic fields.
- Malformed-input coverage rejects unknown mandatory sections, accepts bounded unknown optional sections, and rejects truncation, count excess, illegal tokens, duplicate/forbidden/overlapping sections, invalid element widths, truncated signature slices, excessive recursion/children, and out-of-range relocations.
- The reader validates header/directory shape, declared size, offsets, lengths, overlap, token table/RID, row contracts, and complete signature consumption before exposing a view or interning a type.
- Source-level test compiles `identity(value: int): int`, projects the compiler's canonical function type, imports the binary signature, and proves identical `TypeId`, signature bytes, and public identity.

## MSVC Evidence

- Toolchain: MSVC 19.44.35228 x64 Debug, `build-syntax-01-m1-msvc`.
- `zr_vm_artifact_schema_test`: 13 tests, 0 failures.
- `zr_vm_canonical_type_graph_test`: 18 tests, 0 failures.
- `zr_vm_place_cfg_graph_test`: 4 tests, 0 failures.
- `zr_vm_pre_semantic_ir_test`: 6 tests, 0 failures.
- `zr_vm_metadata_token_model_test`: 21 tests, 0 failures.
- `zr_vm_metadata_runtime_binding_compatibility_test`: 17 tests, 0 failures.
- `zr_vm_zrp_metadata_format_test`: 13 tests, 0 failures.
- `zr_vm_project_import_canonicalization_test`: 35 tests, 0 failures.
- `zr_vm_compiler_integration_test`: 127 tests, 0 failures.
- Total selected matrix: 254 tests, 0 failures.

## Staged-Snapshot Evidence

- GCC 11.4 Debug: all 9 targets and 254 tests passed in `/home/hejiahui/zr_vm-syntax-m4-staged-gcc-20260719-r3`; marker `GCC_INDEX_MATCH`, final marker `GCC_M4_MATRIX_PASS`.
- Clang 14.0 Debug: all 9 targets and 254 tests passed in `/home/hejiahui/zr_vm-syntax-m4-staged-clang-20260719-r3`; marker `CLANG_INDEX_MATCH`, final marker `CLANG_M4_MATRIX_PASS`.
- Each snapshot byte-compared every staged implementation/test/document path with the Git index before configuration.
- Snapshot assembly populated checked-out submodule contents because `git checkout-index` materializes only gitlinks.
- Snapshot assembly also overlaid the existing dirty baseline `zr_vm_core/include/zr_vm_core/profile.h` and `zr_vm_core/src/zr_vm_core/profile.c`, because committed `value.h` already references `ZR_PROFILE_HELPER_VALUE_CONSTRUCT`; neither file is part of the M4 staged diff.
- GCC/Clang warnings were existing dispatch extensions, descriptor initializers, const qualifiers, unused helpers, and test prototype warnings outside the M4 artifact implementation.

## Review And Boundaries

- Final review result: GO, 0 Critical and 0 Important findings remaining.
- Review found and fixed unsafe pointer construction from untrusted offsets, raw-byte versus row count limits, struct-padding comparison, unvalidated text views, ambiguous payload anchors, multi-row layout selection, token RID validation, and parser enum/nominal import initialization before this gate was closed.
- The implementation is split by encoding, rows, signatures, identity, text, schema validation, and parser projection; every new implementation file remains below the 1000-line module threshold.
- Existing `SZrIo` serialization remains an explicitly documented compatibility path. M5 owns consumer migration for VM, AOT, LSP, reflection, debug, CLI, and legacy writers/loaders.
- Formal old-schema rejection belongs to the M5 cutover; M4 defines and validates the canonical schema without introducing permanent dual-format execution.

## Final Gate

- M4 plan acceptance clauses: passed.
- MSVC compatibility matrix: passed, 254/254.
- GCC staged-snapshot matrix: passed, 254/254.
- Clang staged-snapshot matrix: passed, 254/254.
- Final Critical/Important review: GO (0 Critical, 0 Important).
- `git diff --cached --check`: passed before adding this record and is rerun at the commit gate.
