---
related_code:
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/module/module_internal.h
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/src/zr_vm_core/function_precall_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - tests/core/test_type_layout_inline_copy.c
  - tests/core/test_precall_frame_slot_reset.c
  - tests/core/test_tail_reuse_callinfo_reset.c
  - tests/core/test_object_call_known_native_fast_path.c
  - tests/core/test_native_inline_span_dispatch.c
  - tests/gc/gc_tests.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_compiler_integration_main.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/module/module_internal.h
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/src/zr_vm_core/function_precall_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
plan_sources:
  - user: 2026-05-16 struct inline stack storage and memcpy parameter passing
  - user: 2026-05-18 real GC/native entry wiring without claiming full ABI completion
  - user: 2026-06-04 align struct value execution with lua/hybridclr and lua/il2cpp architecture
tests:
  - tests/core/test_type_layout_inline_copy.c
  - tests/core/test_precall_frame_slot_reset.c
  - tests/core/test_tail_reuse_callinfo_reset.c
  - tests/core/test_object_call_known_native_fast_path.c
  - tests/core/test_native_inline_span_dispatch.c
  - tests/gc/gc_tests.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_compiler_integration_main.c
  - tests/parser/test_value_type_runtime.c
  - tests/acceptance/2026-05-16-inline-struct-byte-stack.md
  - tests/acceptance/2026-05-18-inline-frame-gc-native-entry.md
doc_type: module-detail
---

# Inline Type Layout And Byte Stack Copy

This module is the first runtime layer for moving `struct` values toward inline, byte-sized stack storage. It does not replace the interpreter's existing fixed-slot frame ABI yet. It provides the typed layout and byte-offset stack primitives that later call-frame migration can use instead of directly assuming `functionBase + slot`.

## Type Layout

`SZrTypeLayout` describes the byte shape of a value stored inline:

- `byteSize` and `byteAlign` describe storage requirements.
- `copyKind` selects POD raw copy versus field-aware copy.
- `dropKind` selects no-op drop versus field-aware drop.
- `fields` describes managed subfields such as embedded `SZrTypeValue` slots.

POD layouts use `memmove` through `ZrCore_TypeLayout_CopyInline`, so overlapping source and destination spans are safe. Field-copy layouts copy unmanaged byte ranges directly and route `SZrTypeValue` fields through the existing value/ownership copy path. Field-drop layouts release only embedded value slots marked with `ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE` through `ZrCore_Ownership_ReleaseValue`; GC-only value fields stay available for mark/rewrite visitors without being treated as owned storage.

`ZR_TYPE_LAYOUT_KIND_VALUE` is a special layout for a standalone `SZrTypeValue`. It always copies through `ZrCore_Value_Copy`, so boxed struct objects still clone instead of being raw-copied by pointer. Dropping a value layout releases the value slot through the ownership runtime.

`ZR_TYPE_LAYOUT_KIND_UNION` is the matching inline layout kind for tagged union values. The layout records the tag byte offset and size, and managed fields carry an `activeTag`. Copy, drop, and GC visitors only process fields whose `activeTag` matches the currently stored union tag. Union copy first drops the destination's old active value payload, then copies non-active byte ranges and active `SZrTypeValue` payload fields through the normal value copy path.

## Byte Stack Primitives

`ZrCore_Stack_SaveByteAddressAsOffset` and `ZrCore_Stack_LoadByteOffsetToAddress` convert between raw stack byte addresses and offsets from `state->stackBase`. `ZrCore_Stack_CopyInline` copies a layout-sized byte span between two stack byte offsets after checking that both ranges fit inside the current stack allocation.

`SZrStackFramePlace` is the typed place bridge between function frame metadata and raw stack bytes. `ZrCore_Stack_MakeFramePlace` resolves a frame base plus a frame-relative byte offset into a checked stack address, absolute stack byte offset, byte size, and relative alignment. `ZrCore_Stack_CopyInlinePlace` then copies between two checked places through `SZrTypeLayout`, rejecting too-small places before reaching the raw copy path.

`ZrCore_Function_MakeFrameSlotPlace` is the function-metadata layer over those raw stack places. It looks up a logical stack slot in `SZrFunctionFrameSlotLayout`, then resolves the stored byte offset and byte span against a caller-provided frame value base. `ZrCore_Function_CopyFrameSlotInline` copies between source and destination frame slots only when the runtime layout kind matches the slot kind, so struct layouts cannot accidentally raw-copy into legacy `SZrTypeValue` slots.

These APIs intentionally coexist with the old slot APIs. Existing instructions, closure captures, and return movement still use `TZrStackValuePointer` slot arithmetic. New code that needs inline struct payloads should go through byte offsets and `SZrTypeLayout` instead of adding more raw slot assumptions.

## Prototype Layout Metadata

Compiled prototype metadata now carries `layoutByteSize` and `layoutByteAlign` in `SZrCompiledPrototypeInfo`. `compiler_struct.c` computes these values from the same field offset pass that assigns `fieldOffset` and `fieldSize`: each non-static struct field is aligned, the running size advances by the field byte size, the maximum field alignment is recorded, and the final struct size is rounded up to that maximum alignment.

The fields are serialized into `function->prototypeData`, imported back into parser type prototypes, copied into runtime `SZrObjectPrototype`, and surfaced through debug/intermediate prototype printing and `%type` reflection layout objects. Runtime reflection now prefers the stored whole-struct layout and only falls back to deriving layout from fields when a prototype was produced before these fields existed or comes from a source that has not supplied them yet.

## Runtime Prototype Type Layout Resolver

`ZrCore_Function_ResolvePrototypeFrameTypeLayout` is the current runtime bridge from `SZrFunctionFrameSlotLayout.typeLayoutId` to `SZrTypeLayout`. In this increment the id is still a checked prototype index, not a standalone serialized type-layout table id. The resolver reads the owning entry function's `prototypeData`, validates the encoded prototype count and byte bounds, and builds a per-function cache of layouts.

The resolver succeeds only when it can prove the inline representation is safe:

- POD structs become `ZR_TYPE_LAYOUT_COPY_KIND_POD` and `ZR_TYPE_LAYOUT_DROP_KIND_NONE` only when every non-lifecycle field is either a known primitive scalar with the exact expected byte size and in-bounds offset, a local nested struct whose layout resolves successfully, or absent from the field list.
- Struct/class fields marked as managed, owned, close-capable, or destructor-capable must fit a whole embedded `SZrTypeValue` inside the declared struct layout; those fields become field-aware GC/ownership entries.
- Union payload fields whose variant metadata records a value-sized storage slot become active-tag-managed `SZrTypeValue` fields. If the payload field has a non-zero ownership qualifier, the field is also marked for ownership release during drop.
- Builtin reference fields such as `string` and `object` are accepted only when the recorded field storage is exactly a whole embedded `SZrTypeValue`. They become GC value fields without an ownership metadata flag; pointer-sized reference fields fail resolution and preserve the boxed/old path.
- A field whose serialized type name resolves to another local struct prototype reuses that nested prototype layout. Managed nested `SZrTypeValue` fields are flattened into the parent layout with the parent field offset added, so GC/copy/drop visitors still see exact embedded value locations instead of scanning the whole nested byte span.
- Bad prototype ids, malformed prototype data, unsafe managed field sizes, recursive cache re-entry, unknown non-local field type names, pointer-sized reference fields, and imported layouts without explicit serialized type-layout metadata cache as failed and return `ZR_NULL`.

That failure mode is intentional. Callers must treat a `ZR_NULL` layout as "inline handling unavailable" and keep the boxed or older path rather than pretending the byte span has a proven lifecycle model.

## Function Frame Layout Metadata

`SZrFunction` now carries a sidecar byte-frame description in addition to the existing fixed-slot execution ABI:

- `frameByteSize` and `frameByteAlign` describe the contiguous byte region needed for the function frame.
- `frameSlotLayoutLength` matches the current `stackSize` for compiled functions.
- `SZrFunctionFrameSlotLayout` maps each logical stack slot to `stackSlot`, `byteOffset`, `byteSize`, `byteAlign`, `slotKind`, `isParameter`, and `typeLayoutId`.

These fields are kept as an append-only sidecar at the end of `SZrFunction`. That preserves the offsets of existing runtime fields such as constant pools, call-site caches, and child-function graphs while the VM still has native fixtures and copied function graphs that observe the public function ABI.

The layout builder lives in `compiler_typed_metadata.c`. It walks stack slots in order, aligns a byte cursor for each slot, and emits deterministic byte offsets. Ordinary slots keep `sizeof(SZrTypeValue)` storage. Typed slots whose local binding resolves to a local struct prototype use that prototype's `layoutByteSize` and `layoutByteAlign`, are marked `ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT`, and carry the prototype index as the first layout id used by this migration.

Frame byte offsets intentionally start after the legacy fixed-slot mirror, `stackSize * sizeof(SZrTypeValueOnStack)`, so byte-backed inline storage does not alias the dense logical slots that older instructions still read. Compiler frame-layout tests assert this base offset after binary roundtrip. The typed metadata classifier also treats conditional branch operands as relative jump metadata, not value slots, unless an actual extra operand names a slot. That keeps inline receiver temporaries typed when branches and inline member reads share a lowering window.

Function declarations, lambdas, class members, meta functions, tests, and final assembled entry functions all call the builder after `stackSize` and typed local metadata are available. `ZrCore_Function_FindFrameSlotLayout` is the public lookup helper for callers that need a typed place without scanning the array themselves.

The `.zro` binary function format now writes the frame byte header and slot layout array immediately after `stackSize`. `ZR_IO_SOURCE_PATCH_HAS_FUNCTION_FRAME_LAYOUT` gates reading so older binaries still load with empty frame layout metadata. Runtime IO copies the metadata into loaded `SZrFunction` instances, including child functions, so tooling and the next VM-stack migration layer can inspect the same byte-frame shape after a binary roundtrip.

`ZrCore_Function_GetFrameStorageSlotCount` computes the number of legacy `SZrTypeValueOnStack` allocation units needed to hold the byte frame. It returns the greater of the old logical `stackSize` and the rounded-up `frameByteSize`. VM precall now uses that storage count for `functionTop` and stack growth, while still using `stackSize` for logical slot semantics. Padding storage slots beyond `stackSize` are reset to null during call setup so existing stack scanning and frame teardown do not observe stale `SZrTypeValue` contents.

VM resolved/prepared pre-call now takes the first real payload-movement step for inline parameters. When a callee has inline struct parameter layouts, exact-args fast-path probes fall back to the generic pre-call path so the layout hook can run. After the callee frame storage is initialized, the runtime derives the caller argument start from the previous VM call-info frame base and calls `ZrCore_Function_CopyInlineFrameParameters` with the prototype resolver. This copies caller inline frame payload bytes into the callee inline parameter span only when both caller and callee layout metadata can prove the operation. Missing caller metadata, missing frame layout, or resolver failure leaves the existing boxed call behavior in place.

Byte-backed VALUE parameters use the same frame-layout boundary. `ZrCore_Function_CopyValueFrameParametersFromFrame` resolves the source and destination VALUE slots through `SZrFunctionFrameSlotLayout`; if the source byte-backed slot is still null but the dense caller slot is materialized, the dense slot is used as the source. After copying into the callee byte span, the helper also mirrors the value into the callee dense slot. Generated AOT C still has mixed consumers during the byte-frame migration: field stores can read byte-backed VALUE storage, while typed arithmetic and branch lowering can read dense slots. Keeping both views synchronized at the call boundary prevents a staged direct call from entering a callee with only one half of the VALUE parameter initialized.

Tail-call frame reuse also keeps byte-backed VALUE parameters synchronized. When a VM frame is reused for a callee without inline-struct parameter layouts, the runtime copies VALUE parameters from the caller's logical frame value slots into the reused byte-backed parameter spans and mirrors them to dense slots before reinitializing frame storage. Destination VALUE parameter slots release existing owned values before being overwritten, so owner payloads left by a previous frame do not leak or double-release.

Single-result VM post-call now has the matching limited return hook for payloads that are already inline on both sides. `ZrCore_Function_TryCopyInlineFrameReturnValue` derives the callee result slot and caller return destination slot from their call-info frame bases, resolves both prototype layouts, checks that the two layouts are byte/lifecycle compatible, then copies the inline span before the callee frame is dropped. If either side is missing frame metadata, the destination is not a caller inline slot, or the two layouts cannot both be resolved, the helper declines and the old `SZrTypeValue` return path remains in force.

Tail-call frame reuse stays deliberately conservative for inline parameters. Until the runtime has a real in-place move operation for overlapping inline payloads with ownership fields, `ZrCore_Function_TryReuseTailVmCall` refuses callees with inline struct parameter layouts. The interpreter then falls back to the non-reuse call path, where the already-inline pre-call copy hook can run without treating raw struct bytes as ordinary stack slots.

## Value-Type Execution Shape

The 2026-06-04 value-type runtime slice follows the same architectural split as the HybridCLR/IL2CPP implementations under `lua/`: inline frame bytes are the canonical value storage, and object/boxed values are materialization bridges. A source `$Struct(...)` constructor can still seed an object-shaped receiver for existing member/constructor dispatch, but the compiler records a separate inline result slot for struct values. Constructor receiver copyback then copies the callee `this` inline payload back into the caller inline result span before the callee frame is dropped. The 2026-06-17 union slice reuses the same idea for typed union locals: a constructor carrier object can be materialized into the target inline frame span as `[tag][payload]` bytes when the destination slot has union inline layout metadata. Union local-to-local declaration initialization and simple assignment are now kept on source inline local slots by the compiler, so `SET_STACK` can copy the existing tag/payload bytes without routing through an object-shaped temporary; this path is scoped to union layouts and leaves the existing inline struct argument/copy lowering unchanged. Constructor assignment into an existing typed union local uses the same `SET_STACK` interception: the inline-frame copy hook checks both the physical source slot and the logical frame value slot for a constructor carrier, drops the destination's old active union payload through the resolved union layout, and then writes the new tag/payload bytes. When the active variant contains embedded value or owner payload fields, the union type layout uses the stored tag to copy/drop/visit only that variant's fields.

Typed union inline slots also participate in the interpreter member-read path for pattern matching. When `GET_MEMBER` targets an inline union slot, `execution_inline_frame_try_get_member_by_name_to_slot` recognizes `__zr_unionVariant` and `__zr_unionPayloadN` as pseudo-members. The tag bytes are read from the inline span and matched against serialized variant metadata to produce the variant name; payload reads use the active variant's payload field byte offset/size/align metadata to load POD fields into a normal result slot. The same pseudo-member path now supports targeted writes used by explicit owner `move` cleanup, and struct-field inline union values can copy owner payloads both from typed locals into fields and from fields back into typed locals. Broader expression/member mutation matrices remain staged, but the current copy/drop path is owner-aware for these typed-local and struct-field cases.

Inline frame initialization also handles managed fields. `ZrCore_Function_InitInlineStorage` zeroes the inline byte span, resolves the prototype layout, recursively initializes nested inline structs, and resets embedded `SZrTypeValue` field slots to null. That keeps string/object fields in value types visible to later GC visitors without treating uninitialized struct bytes as arbitrary stack values.

Return movement now distinguishes physical stack slots from logical frame value slots. `function_move_returns` and the single-result post-call fast path resolve the callee return source through the callee frame layout before doing the ordinary `SZrTypeValue` copy. Inline struct returns still go through `ZrCore_Function_TryCopyInlineFrameReturnValue`; scalar/object returns whose source lives in frame bytes now read from the logical `FRAME_VALUE_SLOT` rather than from a stale physical stack position.

Interpreter native and generic call paths stage call windows outside the caller frame storage when frame layout metadata is active. `execution_prepare_frame_layout_call_window` snapshots the logical callable and arguments, materializes inline struct arguments where needed, reserves scratch storage at or beyond `ZrCore_Function_GetCallInfoFrameStorageTop`, and copies the snapshot there. Known native calls, known native member calls, generic calls, meta calls, dynamic calls, and the `SUPER_DYN_TAIL_CALL_NO_ARGS` native fallback use staged windows so temporary frames do not overlap inline local payload bytes. Native and meta-call results are written back through the logical return destination, preserving the caller frame layout.

Ownership and typed branch/equality opcodes are part of the same boundary. When frame layout metadata is active, ownership casts/releases, object conversion, typed equality, typed comparisons, and fused signed branch tests read the logical `FRAME_VALUE_SLOT` rather than assuming the physical `BASE(slot)` storage unit is the canonical value. Weak reference expiry additionally verifies that a candidate slot is still weak and still points to the expiring weak ref before clearing it, which prevents a release path from nulling unrelated frame-layout values that happen to share an old weak-reference side table entry.

## GC, Drop, And Native Entries

The frame byte layout is now used by real runtime entries when metadata is present and the resolver proves the inline layout.

GC stack scanning keeps the callable slot on the legacy path, then treats `callInfo->functionBase.valuePointer + 1` as the frame byte base. Ordinary stack slots that intersect an inline struct span are skipped by raw slot scanning. `ZrCore_Function_VisitInlineFrameGcValues` then visits only the embedded `SZrTypeValue` fields declared by the resolved layout. Minor collection rewrite uses the same layout visitor, so forwarded embedded values are rewritten in place instead of leaving stale object pointers inside raw struct bytes.

Frame teardown and tail-call reuse call `ZrCore_Function_DropInlineFrameValues` before old frame storage is overwritten or reused. For inline single-result returns, post-call captures the callee metadata first, copies the inline return payload to the caller destination, and then drops the captured callee frame layout so ordinary return movement cannot erase the metadata needed for cleanup. Tail reuse also checks `ZrCore_Function_FrameStackSlotIntersectsInlineStruct` while releasing old storage slots, so raw inline bytes are cleared as storage units rather than being interpreted as ordinary `SZrTypeValue` slots. The drop helper preflights all inline layouts before performing any field drop; if any layout cannot be resolved, it fails without partially dropping the frame.

Native dispatch now seeds `ZrLibCallContext` with the current VM frame's metadata function, callable base, local frame base, and inline argument start. `ZrLib_CallContext_InlineArgumentSpan` can be called from a real known-native callback and returns an address, byte size, alignment, and type id for inline struct arguments that already exist in the VM frame layout. Stack-root callbacks use the existing stack-layout anchor; stable-argument native lanes now also adopt a lightweight inline-frame anchor, so a callback that grows or relocates the stack before asking for the span receives the relocated frame payload address without converting stable argument copies back to old stack slots. The span API requires the resolved frame slot to be both inline and marked as a parameter, so inline locals cannot be exposed through an argument index. When an argument is an inline struct parameter, ordinary `ZrLib_CallContext_Argument` and the typed `Read*` helpers report it as unavailable instead of returning a stable `SZrTypeValue` copy, because the inline payload bytes are not a boxed value. Object known-native direct-binding fast paths also clear their local call-context copies before filling fields, so absent inline frame metadata remains explicit absence rather than uninitialized state. When the current call has no frame layout, the argument is not an inline parameter, or metadata resolution is unavailable, the span API reports unavailable and the existing boxed/native behavior remains unchanged.

## Current Boundary

The implemented boundary covers:

- POD inline copy with overlap-safe byte movement.
- Managed inline copy/drop for layouts containing embedded `SZrTypeValue` fields.
- Standalone value layout copy/drop that preserves existing `SZrTypeValue` ownership and struct clone semantics.
- Sequential frame layout calculation with per-slot alignment.
- Stack-base-relative byte offset load/save/copy.
- Typed stack frame places that resolve frame-relative byte offsets and copy layout-sized spans without exposing raw slot arithmetic to callers.
- Function-level frame slot places and layout-kind-checked frame slot inline copy.
- Struct prototype metadata round-tripping of whole-value `layoutByteSize` and `layoutByteAlign`.
- Function frame sidecar metadata for parameters, locals, generated stack slots, and `.zro` runtime loading.
- VM precall reserves enough legacy stack allocation units to cover `frameByteSize`, including prepared-call fallback and tail-call frame reuse.
- VM resolved/prepared pre-call copies already-inline caller frame payloads into callee inline parameter spans when both frame layouts and prototype layout resolution are available.
- VM resolved/prepared pre-call copies byte-backed VALUE parameters from original frame-layout source slots and mirrors them into the callee dense slot, with dense source fallback when the caller byte slot was not materialized.
- Tail-call VM frame reuse copies byte-backed VALUE parameters through logical frame slots, mirrors dense slots, and releases overwritten VALUE owners before reuse.
- VM single-result post-call copies already-inline callee return payloads into caller inline destinations before dropping the callee frame when both layouts resolve compatibly.
- Source-level struct constructors keep boxed/object materialization separate from the inline result span and copy the mutated constructor receiver back into inline frame storage.
- Ordinary scalar/object returns read callee logical frame value slots when the physical stack slot is no longer the canonical source.
- Known native, known native member, generic, meta, dynamic, and dynamic tail-call native fallback paths stage call windows outside caller frame storage so temporary frames cannot overwrite inline struct payload bytes.
- Ownership, object-conversion, typed equality/comparison, and fused branch opcodes read logical frame value slots under frame-layout metadata; weak expiry clears only matching weak slots.
- Tail-call frame reuse refuses inline-parameter callees until a layout-aware overlapping move exists, causing those calls to fall back to the non-reuse path instead of raw slot copying.
- Runtime prototype-index layout resolution for provable primitive POD fields, value-sized builtin reference fields, managed embedded `SZrTypeValue` fields, and local nested struct fields with managed embedded values, with checked failure for unsafe, pointer-sized reference, recursive, imported, or otherwise unknown metadata.
- GC mark and minor rewrite traversal for embedded inline-frame values through the resolved field layout.
- Frame post-call and tail-call reuse drop wiring for owned embedded inline values.
- Native callback inline argument spans in the real dispatch context for already-inline VM frame payloads, including span refresh after native callback stack relocation in stack-root, stable fast/inline-pinned, and generic dispatcher lanes, with non-parameter inline slots rejected, ordinary boxed argument reads blocked for inline struct parameters, and missing/non-inline metadata preserving boxed argument reads.
- Runtime validation for local struct field mutation, frame-byte probes, by-value parameter mutation, by-value return mutation, large POD values, managed string fields, GC scanning of embedded value fields, constructor copyback, and nested struct field copy.
- Typed union constructor materialization into inline frame bytes for POD and value-sized payload fields, using the same type-layout bridge as struct inline values.
- Typed union constructor assignment into existing inline locals, including replacement of an active owner payload before writing the new variant.
- Typed union inline pseudo-member reads for `__zr_unionVariant` and `__zr_unionPayloadN`, allowing `switch`/`using` pattern matching to read tag and POD payload bytes from inline local slots.
- Typed union inline pseudo-member writes for explicit `move` cleanup and struct-field inline union owner payload copy in both directions between typed locals and fields.
- Active-tag-aware inline union copy/drop/GC traversal for embedded value payload fields, including ownership release for payload fields with an ownership qualifier.

The remaining migration work is to move source-to-argument materialization and the full return/tail-call by-value payload model onto the byte-frame ABI beyond the already-inline pre-call copy, already-inline single-result post-call copy, and conservative tail-reuse fallback; broaden inline union member-level store/mutation matrices beyond typed-local copy, struct-field owner payload copy, and explicit move cleanup; extend union owner-payload stress coverage beyond the current multi-field and struct-field regressions if future layout limits appear; replace `typeLayoutId` as prototype index with an explicit module/function type-layout table if broader metadata needs it; implement source-level `%extern struct` platform ABI marshaling for native calls; expose a text `.zri` frame layout section if needed by tooling; and remove boxed-struct fallback paths only after escape, closure, native ABI, and reflection cases have equivalent inline handling.
