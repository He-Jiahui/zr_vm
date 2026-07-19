---
related_code:
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_parser/include/zr_vm_parser/artifact_projection.h
  - zr_vm_core/src/zr_vm_core/artifact_encoding.c
  - zr_vm_core/src/zr_vm_core/artifact_identity.c
  - zr_vm_core/src/zr_vm_core/artifact_rows.c
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
  - zr_vm_core/src/zr_vm_core/artifact_signature.c
  - zr_vm_core/src/zr_vm_core/artifact_text.c
  - zr_vm_parser/src/zr_vm_parser/artifact_projection.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_parser/include/zr_vm_parser/artifact_projection.h
  - zr_vm_core/src/zr_vm_core/artifact_encoding.c
  - zr_vm_core/src/zr_vm_core/artifact_identity.c
  - zr_vm_core/src/zr_vm_core/artifact_rows.c
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
  - zr_vm_core/src/zr_vm_core/artifact_signature.c
  - zr_vm_core/src/zr_vm_core/artifact_text.c
  - zr_vm_parser/src/zr_vm_parser/artifact_projection.c
plan_sources:
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
tests:
  - tests/parser/test_artifact_schema.c
  - tests/parser/test_artifact_schema_source_roundtrip.c
  - tests/acceptance/2026-07-19-syntax-01-m4-artifact-schema.md
doc_type: module-detail
---

# Canonical Artifact Schema And Type Projection

## Purpose

M4 defines a stable artifact contract for canonical types and public cross-module binding. It separates syntax, diagnostic semantic state, and executable module state instead of serializing compiler C structs or recovering public behavior from names.

The core schema owns encoding, section validation, row decoding, public identity comparison, signature validation, and readable text projection. The parser bridge owns conversion between M1 canonical `TypeId` nodes and stable signature bytes. Core does not depend on parser AST or semantic-context storage.

## Artifact Roles

- `.zrs` accepts string, syntax-tree, and optional debug sections. Its public identity must be zero. The readable projection exposes syntax text and source ranges without layout, loan, or runtime pointer state.
- `.zri` accepts canonical type, signature, contract, layout, debug, and Semantic IR sections. Its readable projection exposes the semantic payload but is not the executable contract.
- `.zro` accepts canonical public tables, code, relocation bindings, and optional debug data. Syntax-tree and local Semantic IR sections are rejected.

The readable projection contains named section summaries and literal syntax/SemIR content, followed by a deterministic hex payload that permits an exact text-to-binary roundtrip. A parser uses the final `payload-hex` anchor so source content cannot shadow the envelope payload.

## Binary Encoding

The binary envelope uses fixed-width little-endian fields:

- a 112-byte header with `ZRAF` magic, schema version, artifact kind, total length, directory shape, and root public identity;
- 24-byte section directory entries with kind, flags, offset, byte length, element count, and encoded element size;
- explicitly encoded rows rather than native `sizeof(struct)` dumps;
- bounded raw heaps for strings, signatures, code, syntax, Semantic IR, and debug data.

Known mandatory sections must be understood. Unknown optional sections are bounds-checked and skipped. Known sections are unique and kind-specific: `.zrs`, `.zri`, and `.zro` reject projections outside their responsibility. Section payloads cannot overlap the header, directory, another non-empty payload, or the declared artifact length.

The schema defines rows for TypeDef, TypeRef, TypeSpec, MemberDef, PropertyDef, Contract, Layout, and relocation bindings. TypeDef rows carry value/GC/resource/readonly/ref-like/drop flags, value-construction capability, and public constructor token/signature/contract. Layout rows carry version, size, alignment, GC scan kind, ownership-map range, layout hash, and optional StableSlotSource contract hash.

## Stable Signatures

Signature bytes encode canonical structure rather than display spelling. Nodes cover primitive, TypeDef, generic parameter/instance, typed const arguments, array, tuple, union, nullable, function, ref, readonly view, exact owner kind, never, and error types.

Function nodes encode receiver effect, ref-export effect, public effect flags, return type, and the full contract for every parameter: passing form, escape upper bound, entry/exit initialization, temporary acceptance, and call-site marker. Surface `:` versus `->` delimiters do not enter the signature.

Validation is recursive but bounded by a fixed depth and child-count limit. Truncation, illegal node tags, illegal metadata tokens, invalid qualifiers, trailing bytes, and excessive nesting fail before interning. Duplicate structural signatures are safe because each TypeRef/TypeSpec row uses an explicit token and bounded slice; serialized graphs cannot contain pointer cycles.

## Identity And Diagnostics

Public identity separates:

- TypeRef hash;
- TypeSpec hash;
- structural signature hash;
- layout version and hash;
- callable contract hash;
- module hash.

The loader reports a distinct status for every mismatch and records expected/actual hash or version values. Internal header-to-table validation also requires the root TypeRef/TypeSpec token, canonical artifact TypeId, signature token, layout, and contract rows to agree. It never falls back to name-based binding.

Metadata tokens are checked for a nonzero RID and the table required by their row. Member/property accessors, constructor tokens, signature tokens, and relocation targets have independent token-shape checks. Relocation code offsets must point inside the CodeTable.

## Source And Binary Type Projection

`ZrParser_ArtifactType_WriteSignature` recursively projects an immutable canonical type graph into the stable signature grammar. `ZrParser_ArtifactType_InternSignature` validates the complete blob first, reconstructs every child through canonical interning, and returns the existing `TypeId` when the source graph already contains the same type.

`ZrParser_ArtifactType_BuildPublicIdentity` derives stable TypeRef, TypeSpec, and signature hashes from the serialized structure, then combines them with the independently supplied layout, callable-contract, and module identities. The real-source acceptance test compiles a typed function, obtains its canonical function type through the compiler type projection, imports the resulting binary signature, and proves equal `TypeId`, signature bytes, and public identity.

## Safety And Compatibility

The reader validates magic, exact schema version, header/directory sizes, total length, section and row limits, fixed element sizes, token shapes, signature slices, row payloads, internal identity, and section overlap before returning a view. It avoids pointer arithmetic for an untrusted offset until that offset is within the declared buffer.

Local Place graphs, block initialization facts, LoanId/origin/last-use state, local ranges, AST pointers, and raw runtime pointers have no `.zro` row. Those values remain `.zri`, debug-sidecar, or compilation-session data.

The existing `SZrIo` execution format remains a compatibility path for consumers not yet migrated. The new schema is the M4 canonical contract. M5 switches VM, AOT, LSP, reflection, debug, CLI, and legacy writers/loaders to this projection; the formal cutover rejects old schema artifacts and requests recompilation rather than supporting permanent dual-format execution.

## Verification

The focused suite covers all three artifact kinds, exact binary/text roundtrips, readable syntax and Semantic IR payloads, fixed widths, zero/one/256-row tables, duplicate signatures, value-construction capability and constructor identity, repeat-write hash stability, every public mismatch, unknown mandatory versus optional sections, truncation, invalid tokens, count limits, forbidden/duplicate/overlapping sections, recursive signature limits, relocation bounds, and source-compile versus binary-import identity.

Parent regression suites protect the M1 type graph, M2 Place/CFG, M3 pre-execution Semantic IR, legacy metadata token/ZRP formats, runtime binding diagnostics, project imports, and compiler behavior while M5 consumer migration remains pending.
