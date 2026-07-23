---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/include/zr_vm_core/canonical_consumer.h
  - zr_vm_parser/include/zr_vm_parser/artifact_projection.h
  - zr_vm_core/src/zr_vm_core/artifact_encoding.c
  - zr_vm_core/src/zr_vm_core/artifact_identity.c
  - zr_vm_core/src/zr_vm_core/artifact_rows.c
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/artifact_signature.c
  - zr_vm_core/src/zr_vm_core/artifact_text.c
  - zr_vm_core/src/zr_vm_core/canonical_consumer.c
  - zr_vm_parser/src/zr_vm_parser/artifact_projection.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/include/zr_vm_core/canonical_consumer.h
  - zr_vm_parser/include/zr_vm_parser/artifact_projection.h
  - zr_vm_core/src/zr_vm_core/artifact_encoding.c
  - zr_vm_core/src/zr_vm_core/artifact_identity.c
  - zr_vm_core/src/zr_vm_core/artifact_rows.c
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/artifact_signature.c
  - zr_vm_core/src/zr_vm_core/artifact_text.c
  - zr_vm_core/src/zr_vm_core/canonical_consumer.c
  - zr_vm_parser/src/zr_vm_parser/artifact_projection.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
plan_sources:
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
  - docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
tests:
  - tests/parser/test_artifact_schema.c
  - tests/parser/test_artifact_schema_source_roundtrip.c
  - tests/parser/test_reference_callable_consumers.c
  - tests/parser/test_canonical_consumers.c
  - tests/parser/test_buffer_pool_ffi.c
  - tests/parser/test_property_access_lowering.c
  - tests/parser/test_property_ref_return.c
  - tests/core/test_type_layout_metadata_contracts.c
  - tests/core/test_resource_cross_domain_transfer.c
  - tests/acceptance/2026-07-19-syntax-01-m4-artifact-schema.md
  - tests/acceptance/2026-07-20-syntax-02-m6-artifact-lsp-consumers.md
  - tests/acceptance/2026-07-22-syntax-04-m7-concurrent-major-artifact-aot-lsp.md
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

The schema defines rows for TypeDef, TypeRef, TypeSpec, MemberDef, PropertyDef, Contract, Layout,
relocation bindings, and domain transfer contracts. TypeDef rows carry
value/GC/resource/readonly/ref-like/drop flags, value-construction capability, and public
constructor token/signature/contract. Layout rows carry version, size, alignment, GC scan kind,
ownership-map range, layout hash, and optional StableSlotSource contract hash.

Schema v2 adds the known fixed-width `DOMAIN_TRANSFER_TABLE`. Each row is keyed by an exact
TypeDef token and encodes `Forbidden`, `ValueCopy`, `StructuredClone`, `ImmutableHandle`, or
`ResourceMove`, plus schema identity, provider identity, and DropOnFailure flags. Rows are strictly
ordered and unique. A missing table or TypeDef row means no cross-domain capability (`Forbidden`),
not an invalid artifact. VM and AOT TypeLayout v2 consume the same identity; provider-backed
layouts with GC, ownership, or ref fields are rejected instead of allowing a source-domain edge.

The core canonical consumer projects an optional domain-transfer row only after resolving the
exact TypeDef/TypeSpec identity. `ZrCore_CanonicalConsumer_ResolveDomainTransfer` returns the same
kind, schema/provider identity and flags to VM and AOT consumers; artifacts without the optional
table remain valid and explicitly report no contract. It never reconstructs a row from a source
type name, display text, member name or payload shape. LSP-facing source hover and diagnostics
continue to consume parser canonical ownership facts; binary/source tools may join them only by
the same declaration/type identity.

Contract rows also carry a bounded callable escape mask and an ABI lowering kind.
The encoded slot was reserved in schema v1, so old artifacts decode as `NONE` and
new readers reject unknown values without changing row width. Current lowering
kinds distinguish ZR value-frame calls, explicit native marshalling, and native
direct calls.

## Stable Signatures

Signature bytes encode canonical structure rather than display spelling. Nodes cover primitive, TypeDef, generic parameter/instance, typed const arguments, array, tuple, union, nullable, function, ref, readonly view, exact owner kind, never, and error types.

Function nodes encode receiver effect, ref-export effect, public effect flags, return type, and the full contract for every parameter: passing form, escape upper bound, entry/exit initialization, temporary acceptance, and call-site marker. Surface `:` versus `->` delimiters do not enter the signature.

The writer derives `ref-export effect` from the canonical return node: non-reference returns use
`none`, writable references use `writable`, and readonly references use `readonly`. The importer
reconstructs the return TypeId first and rejects a header marker that does not match its ref access;
the marker is never accepted as an independent source of truth.

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

## Public Ref-Like ABI Validation

`ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi` is the shared VM/AOT gate for
a public ref-like signature. Its expectation names an exact TypeRef token and
hash, exact known type flags, layout version/hash, callable signature token,
escape flags, and target lowering kind. Resolution is token-based; it never uses
type or member display names.

ZR value-frame lowering and explicit native marshalling are accepted when every
field matches. Native direct lowering is rejected for ref-like values even when
the layout matches, because the ZR representation may contain managed base/offset
or guard state rather than a platform ABI pointer. Callers must publish and
consume an explicit marshaller contract before crossing that boundary.

## Source And Binary Type Projection

`ZrParser_ArtifactType_WriteSignature` recursively projects an immutable canonical type graph into the stable signature grammar. `ZrParser_ArtifactType_InternSignature` validates the complete blob first, reconstructs every child through canonical interning, and returns the existing `TypeId` when the source graph already contains the same type.

`ZrParser_ArtifactType_BuildPublicIdentity` derives stable TypeRef, TypeSpec, and signature hashes from the serialized structure, then combines them with the independently supplied layout, callable-contract, and module identities. The real-source acceptance test compiles a typed function, obtains its canonical function type through the compiler type projection, imports the resulting binary signature, and proves equal `TypeId`, signature bytes, and public identity.

`ZrCore_Artifact_ReadCallableSignatureSummary` validates a complete function signature before
projecting receiver effect, ref-export effect, effect flags, parameter count, and whether any
`ref`/`ref readonly` parameter has a function-scoped escape bound. The core canonical consumer
cross-validates this summary against the root ContractRow. A stale parameter count, scoped flag,
receiver effect, ref-export effect, or callable effect therefore fails VM and AOT opening with the
same `INVALID_SIGNATURE` result.

## Safety And Compatibility

The reader validates magic, exact schema version, header/directory sizes, total length, section and row limits, fixed element sizes, token shapes, signature slices, row payloads, internal identity, and section overlap before returning a view. It avoids pointer arithmetic for an untrusted offset until that offset is within the declared buffer.

Local Place graphs, block initialization facts, LoanId/origin/last-use state, local ranges, AST pointers, and raw runtime pointers have no `.zro` row. Those values remain `.zri`, debug-sidecar, or compilation-session data.

The existing `SZrIo` execution format remains a compatibility path for consumers not yet migrated. The new schema is the M4 canonical contract. M5 switches VM, AOT, LSP, reflection, debug, CLI, and legacy writers/loaders to this projection; the formal cutover rejects old schema artifacts and requests recompilation rather than supporting permanent dual-format execution.

### Executable function entry stack boundary

The executable `SZrIo` compatibility format advances to source patch 36 and writes
`vmEntryClearStackSizePlusOne` immediately after each function's `stackSize`. The value is the
compiler-owned entry clear boundary used by VM pre-call setup; it is not reconstructed from the
instruction stream, current stack depth, or call-site cache state. Readers for patch 36 load the
field directly, while older supported patches retain the existing derived/default compatibility
path.

Property accessor roundtrip depends on this field because getter/setter calls can capture a receiver,
create operator/RHS temporaries, and then enter another VM function. Preserving the exact clear
boundary prevents a loaded function from clearing a captured receiver or leaving a stale temporary.
The M3 roundtrip test compares the source and loaded function's stack size, entry clear boundary,
member-entry identity, instruction bytes, and execution result before declaring artifact parity.

### Property-reference execution contract

Syntax 05 M4 preserves reference properties through the existing executable artifact envelope. The
visible property and linked getter keep their canonical identity and reference TypeId; the executable
stream preserves appended property-reference create/load/store opcodes and the frame metadata needed
to refresh a managed base. The execution SemIR sidecar uses stable `PROPERTY_REF_GET`, `DEREFERENCE`,
and `PROPERTY_REF_STORE` operation ids, so source and reloaded functions project the same semantic
operations even when the interpreter later quickens a call site.

Local PlaceId, LoanId and source-region arrays remain compilation-session facts and are not serialized
as raw pointers. Artifact consumers recover executable managed-reference behavior from the encoded
property/accessor/TypeId and operation payload; they do not infer it from member spelling or display
text. C and LLVM AOT consume those same stable operations through the shared managed-reference runtime
helpers. An old or native artifact without the required structured reference contract is unavailable
rather than being treated as a direct pointer.

## Verification

The focused suite covers all three artifact kinds, exact binary/text roundtrips, readable syntax and Semantic IR payloads, fixed widths, zero/one/256-row tables, duplicate signatures, value-construction capability and constructor identity, repeat-write hash stability, every public mismatch, unknown mandatory versus optional sections, truncation, invalid tokens, count limits, forbidden/duplicate/overlapping sections, recursive signature limits, relocation bounds, and source-compile versus binary-import identity. Reference-callable coverage starts with the production source query TypeId, proves signature import returns the same TypeId, then verifies the same bytes and ContractRow through VM and AOT projections, including negative ref-export and scoped-flag mismatches.

Parent regression suites protect the M1 type graph, M2 Place/CFG, M3 pre-execution Semantic IR, legacy metadata token/ZRP formats, runtime binding diagnostics, project imports, and compiler behavior. Syntax 02 M6 additionally proves one reference-callable signature starts at the source contract, survives binary signature import, and is consumed byte-for-byte by VM and AOT while the LSP projects the same source callable facts.
