---
related_code:
  - tests/parser/test_ssa_primitive_source_frame_storage.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-execbc-vm.cmake
implementation_files:
  - tests/parser/test_ssa_primitive_source_frame_storage.c
plan_sources:
  - .codex/plans/20261005-ssa-primitive-frame-storage-guards.md
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
tests:
  - tests/parser/test_ssa_primitive_source_frame_storage.c
  - tests/parser/test_ssa_primitive_source_frame.c
doc_type: test-harness-module
status: primitive-source-frame-storage-focused-windows-green-accepted
---

# Primitive source frame storage test harness

## Responsibility and dependencies

This module supplements the accepted [primitive source frame adapter](../parser-and-semantics/ssa-primitive-source-frame.md)
with four concrete storage and arithmetic refusal cases. It calls the current
production API directly. The existing frame suite owns successful attachment,
canonical AOT projection and the actual 9/8 source prerequisites; this supplement
does not duplicate those positive cases.

`tests/parser/test_ssa_primitive_source_frame_storage.c` owns its local state,
fixture, compacted output and Oracle result. The shared fixture remains the
single implementation of the actual parse, canonicalization, preparation,
compile/validate/assemble, finalization and module pipeline. `prepare()` composes
that fixture with same-context dead-place compaction and the actual host row
constructor. It verifies the original graph, observes one original place call,
verifies the compacted graph and observes zero place calls, then appends the
actual INT64 layout to the module. Preparation does not attach a frame.

## Four fault observations

| Case | Temporary change to valid owned metadata | Required diagnostic |
| --- | --- | --- |
| `test_value_count_exceeds_capacity` | Set live value capacity below the unchanged count | `INVALID_RANGE` |
| `test_canonical_length_exceeds_capacity` | Set canonical length to saved capacity plus one | `INVALID_RANGE` |
| `test_canonical_capacity_product_overflow` | Set canonical capacity to `SIZE_MAX / sizeof(SZrCanonicalTypeNode) + 1` | `CAPACITY_OVERFLOW` |
| `test_final_frame_alignment_overflow` | One actual value; actual row type and alignment greater than one; byte size becomes `UINT32_MAX` | `CAPACITY_OVERFLOW` |

The first three cases retain the actual allocation and pointer. The last case
retains the actual row ID, type, hash and power-of-two alignment and exercises
final packed alignment arithmetic. Its modified geometry is not a positive host
ABI row and does not require a frame payload allocation.

## Restoration and ownership

`observe_valid()` records the compacted function/frame, original function,
source/context and module/table while all headers are valid. Each case saves its
owned fields, installs one fault, calls attachment, captures the faulted fields
after the call, restores all fields, and only then performs its first assertion
or digest. Comparing the captured fields prevents restoration from hiding a
producer mutation. Post-restoration digests establish preservation of the valid
owned graphs, source and table; the diagnostic also identifies the actual output
function token.

`SourceDigest` hashes canonical array length and must never run under the
inflated-length fault. Lookup, assertions and teardown also follow restoration.
No case supplies an arbitrary address, freed pointer, out-of-allocation pointer,
interior alias or huge allocation. These tests do not prove allocator extent or
turn independent-storage caller preconditions into extra diagnostic promises.
The value-capacity case captures any unexpected newly published frame in a
global owner before restoring the complete output header. This owner starts
null before setup assertions. Teardown frees the Oracle result, conditionally
frees that frame's slots and header, then frees output, fixture and state. It
never frees a shallow captured function. This makes assertion-failure cleanup
well-defined without claiming allocator fault injection.

## Validation ownership and boundary

Root owns target registration, actual direct-checkout builds, receipts and Git.
The executable target is `zr_vm_ssa_primitive_source_frame_storage_test`; its
CTest name is `ssa_primitive_source_frame_storage`. Root's actual Windows
clang-cl 19 Debug/assertions/UBSan run passed independent prerequisites 2/2,
storage 4/4 and frame regression 19/19, with zero failures/ignored cases and no
observed UBSan diagnostic. The accepted storage test pin is
`4A0F1E6ED8A47D77363441B9775C7483C9F90B5893B1F4652A6418471D942471`;
the linked evidence record owns the immutable receipt and current source pins.
No additional host/compaction suites or new MSVC compilation were run for this
gate. Linux GCC/Clang and full SSA47 remain OPEN. No descriptor/native/retention or sensitive network,
security, provider or external messaging path is exercised.

See the [evidence record](../acceptance/ssa-primitive-source-frame-storage.md)
and [test acceptance guide](../../tests/acceptance/ssa-primitive-source-frame-storage.md).
