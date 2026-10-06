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
  - tests/parser/test_ssa_primitive_source_frame_oom.c
  - tests/parser/support/ssa_primitive_source_frame_oom_shim.h
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - tests/parser/test_ssa_primitive_source_frame_oom.c
  - tests/parser/support/ssa_primitive_source_frame_oom_shim.h
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-execbc-vm.cmake
plan_sources:
  - .codex/plans/20261006-ssa-primitive-frame-oom.md
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
  - docs/parser-and-semantics/ssa-dead-source-places.md
  - docs/parser-and-semantics/ssa-host-primitive-layout.md
tests:
  - tests/parser/test_ssa_primitive_source_frame.c
  - tests/acceptance/ssa-primitive-source-frame.md
  - docs/acceptance/ssa-primitive-source-frame.md
  - tests/parser/test_ssa_primitive_source_frame_oom.c
  - tests/acceptance/ssa-primitive-source-frame-oom.md
  - docs/acceptance/ssa-primitive-source-frame-oom.md
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

## Finite OOM successor: actual allocator failures

**Focused Windows direct-source OOM gate accepted.** Root registered and built
the standalone OBJECT and executable, then ran four complete suites under
Windows x64 clang-cl 19 Debug assertions and UBSan: host 15, frame 19, storage
4 and OOM 2, totaling 40 Unity cases, zero failures and ignored cases. Configure,
build and CTest each naturally exited 0. No UBSan diagnostics or build warnings
were observed. Final receipt has `validated: true`; Root's final Git transaction
remains pending, and this record assigns no new commit or RED run.
The preceding frame evidence remains historical dependency evidence. See
the [OOM evidence record](../acceptance/ssa-primitive-source-frame-oom.md) and
[two-case guide](../../tests/acceptance/ssa-primitive-source-frame-oom.md).

The new test uses the shared real NINE source fixture from the existing 9/8
pipeline. Each case independently compacts the actual source graph in its live
canonical context, creates the actual host i64 row and appends it to the real
module. It preserves the source graph, semantic/context pools, source maps and
module tables as separate caller owners. The new test does not execute Oracle
or lower a native/backend artifact. Its two direct faults are genuine NULL
results from selected allocation wrappers, without malformed metadata:

| Actual attachment call site | Request | Failure consequence |
| --- | --- | --- |
| `exec_ir_source_frame.c:313`, `calloc` | `valueCount * sizeof(SZrExecIrPackedValue)` | Returns `OUT_OF_MEMORY` before packed construction; no direct temporary owner exists. |
| `exec_ir_source_frame.c:337`, `malloc` | `sizeof(SZrExecIrFrameLayout)` | Returns `OUT_OF_MEMORY` through cleanup after real packed construction; direct values and owned packed arrays are released. |

The compacted fixture has one surviving actual value. The test compares the
allocator arguments to the real count and `sizeof` expressions; it does not
hardcode a guessed allocation width or request an oversized allocation.
The diagnostic is prefilled, then compared byte-for-byte to a zeroed expected
record with only OOM code and the original function token set. Failure checks
retain the entire original graph header, contract, pool identities and active
contents, plus original source/context and module-table observations.

Only an independent OBJECT target compiles the actual production attachment TU
with a forced test shim. The shim includes the real CRT and packed-layout
declarations before redirecting call sites. The normal parser archive and
shared fixture are not injected. Existing static-archive extraction permits
the explicit object to supply Attach while the ordinary archive supplies real
dependencies.
Actual compile commands contain exactly two production frame TU entries and
one forced-include entry belonging solely to the standalone OBJECT. The normal
parser entry has no shim. The sole Attach link-map record resolves to explicit
`exec_ir_source_frame.c.obj`, without a parser-archive member prefix. Source-global
properties, renamed CRT declaration flags, production test hooks and source
copies are not part of this route.

Wrappers use fixed global owner records and never Unity-assert inside the
production stack. They count the two direct attachment allocation sites and
their wrapped frees. A separate PackedFrameLayoutFree call wrapper invokes the
original helper and observes clearing of its still-live header; it never reads
freed arrays. Packed construction also allocates ordering, mapping and slot
storage in another TU. Those allocations and Core frees are neither injected
nor dynamically counted by this finite ledger, so this gate cannot certify
total heap allocation/free balance or an exhaustive helper OOM matrix.

After each failure, the test disables injection and retries Attach on the
**same live graph**, without clone, rebuild, restoration or recompaction. A
successful retry may publish only frameLayout and contract.layoutHash; the
remaining header, pool contents, source maps and caller tables must match the
original observation. Actual ValueId/TypeId slot geometry, Core VERIFY_ALL and
source-map correspondence are then checked. Immediately after Attach returns,
the test records any header publication before asserting: header ownership
moves to the output graph, and successful packed slots move with it. Core
FreeFunction owns their later teardown; those frees do not increase wrapped
free counts. Teardown disables injection, frees the actual graph, releases any
still-owned direct temporary allocation after an assertion failure, then frees
fixture and state.

The unchanged production attachment module passed this finite gate. Acceptance
covers only the two direct Attach allocation sites, preservation and recovery
on the same live graph. Packed-helper allocations remain uninstrumented; no total heap
balance or exhaustive allocator OOM claim is made. Ordinary static/shared route
execution, full MSVC/Linux matrices, descriptor/native/retention acceptance,
sensitive boundaries and full SSA47 remain OPEN. Root owns the final Git
transaction; no new commit is assigned here.

Initial aggregate driver native83590 was actually observed exiting 1: its
expected inventory was storage 17 / total 53 instead of actual 4 / 40. All
functional child phases had naturally exited 0. Recovery driver native7660 was
actually observed naturally terminal with exit 0 after reconciling the actual
source inventory, 28 source pins, three immutable logs, five metadata files,
four executables and compile/link isolation. It reran no tests and changed no
source or functional logs. The historical failed receipt and earlier g2:18
wrong-header-pin preflight failure remain retained; the preflight failure
preceded native configure/build/test phases. The correct frame-layout header
is `zr_vm_parser/include/zr_vm_parser/exec_ir_frame_layout.h`.

Final receipt: `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/primitive-frame-oom-r2-receipt.json`,
SHA-256 `522FA6E2E2CD2FF8776EAE86BA682DF1EB1E3511478D6B26F217350E79DEEC87`.
All 28 source pins, five metadata files, four executables and three immutable
logs were freshly checked against the receipt and matched. Historical receipt:
`primitive-frame-oom-receipt.json`, SHA-256
`A0AF1B6825597C3F68941C0B4EE19286D8DCC59126253A948F02E88A97D329E0`.
Earlier preflight record: `primitive-frame-oom-preflight-failure.json`.
Both historical records remain in the same report directory.
