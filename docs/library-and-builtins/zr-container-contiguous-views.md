---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_contract_conf.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_contiguous_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_field_scalar_locals.c
implementation_files:
  - zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.c
  - zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.h
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_contiguous_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_contiguous_view.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_fields.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_field_scalar_locals.c
plan_sources:
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
  - user: 2026-08-05 完成 Syntax 10C official provider convergence
tests:
  - tests/parser/test_span_core.c
  - tests/parser/test_span_semantic_ir_cases.c
  - tests/parser/test_buffer_pool_ffi.c
  - tests/parser/test_aot_c_value_type_shared_library_smoke.c
  - tests/library/test_official_provider_convergence.c
  - tests/acceptance/2026-08-05-syntax-10c-official-provider-convergence.md
doc_type: module
---

# `zr.container` Contiguous Views

`zr.container` publishes `Span<T>` and `ReadOnlySpan<T>` as non-owning, ref-like,
inline contiguous views. The compiler recognizes them through protocol bits and
member-contract roles. It does not compare the type or member names when choosing
view, slice, readonly-conversion, bounds, or lifecycle semantics.

The owning `zr.container` descriptor has Runtime phase and public contract hash
`zr.container:v1:container-span-protocols`. Provider convergence validates that
the descriptor is an official N1 owner and does not duplicate Task or iteration
protocol TypeDefs.

## Public Surface

`Array<T>.span()` creates a mutable `Span<T>`. Both view types expose `length`,
indexing, and `slice(start, length)`; `Span<T>` additionally permits index writes
and exposes `asReadOnly()`.

```zr
let container = import("zr.container");
var values = container.Array<int>();
values.add(3);
values.add(5);

var cells = values.span();
cells[1] = 8;
var tail = cells.slice(1, 1);
var readonlyTail = tail.asReadOnly();
return readonlyTail[0];
```

`Span<T>` may weaken implicitly to `ReadOnlySpan<T>` when the element TypeId is
identical. The reverse conversion and element-type changes are rejected. Exact
overload matches rank ahead of readonly weakening.

## Structured Contract

The native descriptor publishes these protocol identities:

- `REF_LIKE` on both view types.
- `CONTIGUOUS_VIEW_MUTABLE` on `Span<T>`.
- `CONTIGUOUS_VIEW_READONLY` on `ReadOnlySpan<T>`.
- `CONTIGUOUS_SOURCE_OWNER` and `CONTIGUOUS_SOURCE_NATIVE_PINNED` for later
  owner-backed and pinned-native providers.

The descriptor also publishes roles for source, start, length, slice,
readonly conversion, and source-to-view creation. Parser projection preserves
those facts on canonical imported TypeDefs and generic instances. New providers
must implement the protocol/role contract rather than relying on `Span`,
`ReadOnlySpan`, `slice`, or `asReadOnly` spelling.

## Representation And Empty Values

The current inline representation carries a source value, a signed start, and a
signed length. Default construction is legal and produces an empty view. An empty
view can be sliced at `(0, 0)` and converted to readonly, but it cannot be indexed.
No heap wrapper or native callback is required for ordinary view creation, slice,
conversion, index, or length access.

The runtime checks:

- index: `0 <= index && index < length`;
- slice: `0 <= start`, `0 <= sliceLength`, `start <= length`, and
  `sliceLength <= length - start`.

The subtraction form avoids overflow from `start + sliceLength`.

## Semantic IR Facts

Every lowered view publishes `SZrSemanticContiguousViewFact`. The fact carries
the view Place and ValueId, source Place and source kind, start/length ValueIds,
region, readonly capability, known numeric facts, and the source loan identity.
Every index publishes `SZrSemanticBoundsFact` with the checked index/length,
proof kind, lower/upper proof bits, and whether the runtime branch was elided.

Elision is valid only when constant numeric facts prove both bounds. Dynamic or
partially proved access retains the same runtime bounds behavior on VM and AOT.
The validator rejects an elided fact without both proof bits, known operands, and
the constant proof kind.

## Lifetime Contract

Array views keep their GC-managed source reachable through the inline source
value. Owner-backed and pinned-native facts additionally require a source loan:

- owner source uses a mutable loan and blocks source move/drop/reuse while the
  view value remains live;
- pinned-native source uses a shared loan and blocks drop/unpin while the view
  value remains live;
- reborrowed views reuse a compatible source loan instead of manufacturing a
  name-based relationship.

Loan liveness is seeded from the view ValueId, then follows value-to-place and
load use chains. This gives non-lexical behavior: a source operation conflicts
only when a later use keeps the view loan live.

M4 freezes these generic source and lifetime facts. M5 supplies concrete
`PoolLease<T>` and `Ptr<u8>` providers. Their descriptors publish the existing
source protocols and create/length roles; compiler lowering carries the resolved
receiver Place into the source loan and gives every created or loaded view a fresh
ValueId. No provider name or member spelling participates in loan selection.

`PoolLease<T>.close()` is rejected while a later view use keeps the mutable owner
loan live. `Ptr<u8>.close()` is rejected under the equivalent native-pinned shared
loan. Once the last view use is past, close/return is legal and idempotent. The
provider-specific storage, generation, pin count, and exception cleanup behavior
is documented in `zr-pooling-and-pinned-ffi-views.md`.

## AOT Boundary

Strict AOT C uses the same inline TypeLayout and bytecode field operations as the
VM. A scalar-local operand feeding an inline primitive field store is lowered
directly from its generated C local; it is not read from a dense value slot that
the scalar optimization intentionally skipped. The shared-library smoke writes a
binary artifact, emits strict AOT C, compiles a real shared library, and executes
the empty slice/readonly/length path through the AOT runtime.

## Verification

`zr_vm_span_core_test` covers descriptor projection, mutation, slicing, readonly
weakening, invalid strengthening, default/empty behavior, bounds failures,
overload ranking, allocation-free lowering, proof-based check elimination,
structured SemIR facts, owner/native lifecycle conflicts, and a full compact GC
while an array-backed Span remains live followed by mutation through that view.

`zr_vm_aot_c_value_type_shared_library_smoke_test` covers binary artifact output,
generated inline field lowering, shared-library loading, and VM/AOT result
equivalence. Unix GCC and Clang execute the real generated library; the Windows
MSVC target retains its existing platform-ignore boundary for this Unix-specific
shared-library smoke.
