---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_layout.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_projections.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - tests/parser/test_ssa_primitive_source_frame.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
plan_sources:
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
  - docs/parser-and-semantics/ssa-dead-source-places.md
  - docs/parser-and-semantics/ssa-host-primitive-layout.md
tests:
  - tests/parser/test_ssa_primitive_source_frame.c
  - tests/acceptance/ssa-primitive-source-frame.md
  - docs/acceptance/ssa-primitive-source-frame.md
doc_type: module-detail
status: primitive-source-frame-focused-windows-green-accepted
---

# Primitive Source Frame Attachment

## Purpose and interface

Actual literal i64 SCRIPT graphs can lose their unused addresses through
[source compaction](ssa-dead-source-places.md). An explicit layout table can
then describe their surviving primitive storage; the host adapter is one
[finite row producer](ssa-host-primitive-layout.md). This entry attaches owned
packed storage to such a current graph without rewriting its instructions.

```c
TZrBool ZrParser_ExecIr_AttachPrimitiveSourceFrame(
        SZrExecIrFunction *function, const SZrSemanticContext *context,
        const SZrExecIrLayout *layouts, TZrUInt32 layoutCount,
        TZrUInt32 frameByteLimit, SZrExecIrDiagnostic *diagnostic);
```

The following is the implemented finite contract. Actual RED passed source
prerequisites and failed 9 of 18 frame cases with the callable stub. Fresh
Windows GREEN passed 19/19 (2 prerequisites, 3 features, 14 guards), independent
prerequisites 2/2, and host 15/15 plus compaction 30/30 regressions. Configure,
build and CTest exited 0 without observed UBSan diagnostics.

## Admission and caller evidence

The graph is independently owned, current, unsealed and frameless. Its context
is the same live canonical interner snapshot used by actual source compaction.
Graph, context, explicit rows and optional diagnostic allocations must remain
valid, readable/writable as appropriate and independently nonoverlapping.
Shape and address arithmetic checks do not establish arbitrary-pointer validity.

The narrow value form is actual canonical PRIMITIVE i64, flags zero, UNKNOWN
ownership, NONNULL nullability and a valid nonzero ordinary definition
instruction. An ordinary Core-valid value may have definition zero; this
producer explicitly refuses that narrower unsupported form before indexing
`definition - 1`. Only CONSTANT/NOP/RETURN instructions are admitted.
Places, EXTERNAL_ENTRY values, reference/GC
representations, rich binding/deopt/state metadata and phi/effect/memory forms
are unsupported. An empty state map retains its original pointer and complete
identity header, including functionToken/signatureHash/generation. It has no
layoutHash field. Existing frame attachment is unsupported and remains intact.

The interface checks current storage consistency; it cannot independently prove
that the caller supplied the original source context or certify a canonical
callable. The real fixture carries actual SemIR/context/source provenance across
compaction. It does not infer context identity by searching for a matching hash.

## Explicit rows and packed mapping

Each surviving value uses a unique explicit row for its actual TypeId. Rows
have unique nonzero IDs and types, nonzero layout hash, positive byte size and
power-of-two alignment. Missing, duplicate and malformed rows are refusals.
The supplied row geometry is the evidence; attachment does not default to host
sizeof/alignof or use an address-size guess.

Each packed request uses the actual ValueId/TypeId, SCALAR class, zero flags and
the whole nonempty instruction lifetime `[0, instructionCount)`. Lifetimes
overlap, so storage is not reused. Parameter prefix and return buffer are zero.
`frameByteLimit == 0` supplies no additional limit. The existing
`ZrParser_ExecIr_LayoutPackedFrame` computes the candidate geometry and hash.

Before publication, logicalSlotCount, storageSlotCount and slotCount must equal
the actual value count. Logical/physical mapping must be bijective; each slot's
slotId is the real ValueId, with matching TypeId/size/alignment. Check offset
alignment, checked bounds, nonoverlapping byte spans and total frame geometry.
The packed frame hash is nonzero. Its existing geometry hash does not include
the explicit row fingerprint/identity; this finite entry does not redefine it.
The row table must remain available to AOT and other consumers.

## Transaction and diagnostic order

1. Check native storage mathematical shape/span, then Core `VERIFY_ALL`.
   Malformed storage is refused at preflight. After valid storage preflight,
   logical Core failures retain their precise verifier diagnostic, including
   when the input is also sealed. Complete failure preservation applies under
   the independent live/readable/writable storage caller preconditions.
2. Refuse a valid sealed graph with SEALED; apply narrower metadata/type/row
   admission and identity consistency checks.
3. Build the packed request/result and owned frame header/slots. Validate its
   complete counts/mapping/geometry before exposing it.
4. Verify a borrowed function view carrying the candidate frame and derived
   contract layout hash. Never free that borrowed view as an owned graph.
5. Publish only `function->frameLayout` and `function->contract.layoutHash`
   after every fallible step succeeds. All other graph/state/source records
   are retained. On failure, release candidate allocations and preserve the
   complete original graph, including an already attached frame.

The fixture observes unchanged body/source/table digests and includes frame
header/slot contents in failure digests. This is local observation evidence,
not a portable serialized identity. OOM and overflow handling remain explicit;
exhaustive allocation-failure injection is a separate coverage gate.

## Actual source and canonical AOT consequence

Independent prerequisites prepare real `return 9;\n` and `return 8;\n`, verify
the original graph and Oracle with one actual place callback, compact it in the
same live context, then verify and run Oracle with zero place callbacks. They
create a real host row and append it to the real module table. They do not call
frame attachment, so frame feature failures occur after genuine prerequisites.

After successful attachment, the fixture verifies geometry, Core VERIFY_ALL,
Oracle 9/8 and preserved source maps. It calls
`LowerAotWithCanonicalCallable` with the same attached output, actual module
constants/layout table/live context and actual SemIR.callableTypeId. Expected
projection is NOARGS_I64 with the actual return TypeId. `valueSlots` maps to
zero-based physical indexes; corresponding `frameSlots.slotId` is the actual
ValueId. The projection preserves constants, layouts and source maps and has
`runnable == false`. This proves canonical projection, not native execution.

The [acceptance record](../acceptance/ssa-primitive-source-frame.md) tracks
execution evidence and limits. Complete ABI/source certification and descriptor
runtime remain unaccepted. The new TU compiled with MSVC, exit 0 and no warnings
observed in its compile log; no MSVC runtime matrix is claimed. Descriptor
runtime, artifact retention, native execution, Linux/MSVC runtime matrices and
SSA47 remain OPEN. No network, FFI, providers, hotpatch or security functions
are exercised by the pure local fixture.
