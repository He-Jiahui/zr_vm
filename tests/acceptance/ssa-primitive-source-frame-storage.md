---
related_code:
  - tests/parser/test_ssa_primitive_source_frame_storage.c
  - tests/parser/test_ssa_primitive_source_frame.c
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
doc_type: test-acceptance-guide
status: primitive-source-frame-storage-focused-windows-green-accepted
---

# Primitive source frame storage acceptance guide

## Prerequisites and finite inventory

Run the existing `zr_vm_ssa_primitive_source_frame_test --prerequisites-only`
first. Its two cases use the real 9/8 source pipeline, same-context compaction,
Oracle observations and actual host row/module append without frame attachment.
The new storage suite then builds its own real nine-source fixture per case;
it verifies one compacted value with actual canonical INT64 type and flags zero.

| Unity case | Expected refusal |
| --- | --- |
| `test_value_count_exceeds_capacity` | `INVALID_RANGE` |
| `test_canonical_length_exceeds_capacity` | `INVALID_RANGE` |
| `test_canonical_capacity_product_overflow` | `CAPACITY_OVERFLOW` |
| `test_final_frame_alignment_overflow` | `CAPACITY_OVERFLOW` |

Each case captures valid graph/source/module digests before perturbing fields.
It captures the post-call faulted header or row, restores every saved field,
then checks the capture, refusal code, function token and preservation digests.
No source digest or canonical lookup runs with inflated array metadata. The
alignment case retains actual type and power-of-two alignment greater than one;
its `UINT32_MAX` size tests final rounding arithmetic rather than a host ABI.
Before full output-header restoration, the value-capacity case records any
unexpected new frame in a global owner initialized before setup assertions.
Teardown frees that owner's slots/header and then normal output/fixture/state;
it never calls `FreeFunction` on the shallow captured header.

## Root-owned direct checkout route

Use the existing configured build beneath `E:/cargo-targets/zr_vm/build` and
actual checkout sources. Root registers executable target
`zr_vm_ssa_primitive_source_frame_storage_test` and CTest
`ssa_primitive_source_frame_storage` in the direct driver and ordinary parser
test registration. Do not create a source copy, replace dependencies or use
`WILL_FAIL` to manufacture acceptance.

After actual configuration and building the new target plus the existing frame
target, run the prerequisite binary and this focused CTest selection:

```powershell
$frameBuild = 'E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2'
& "$frameBuild/bin/zr_vm_ssa_primitive_source_frame_test.exe" --prerequisites-only
ctest --test-dir $frameBuild -C Debug --output-on-failure -R '^(ssa_primitive_source_frame_storage|ssa_primitive_source_frame)$'
```

`$frameBuild` is the actual configured E-drive build directory recorded by
the immutable receipt's metadata and binary paths.
Prerequisites must pass 2/2, storage 4/4 and frame regression 19/19, with natural
exit 0, zero failures/ignored cases and no observed UBSan diagnostic. Source,
metadata, log and binary pins must match the actual run.

## Decision boundary

Current status is **focused Windows GREEN accepted**. Root's actual configure,
build, independent prerequisites and two-suite CTest all exited 0: prerequisites
2/2, storage 4/4 and frame regression 19/19, zero failures/ignored cases and no
observed UBSan diagnostic. Native job 69101 naturally ended exit 0. The actual
storage test SHA-256 is
`4A0F1E6ED8A47D77363441B9775C7483C9F90B5893B1F4652A6418471D942471`.
The immutable receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/primitive-source-frame-storage-receipt.json`,
SHA-256 `24311804C1681F02DCEC07B0C396A32B24317C1E7D541F0AB6F679704B72DAC1`.

The accepted first adapter
commit `aeef5d80e91c37947477e760c7a19ca8356a2111` and its MSVC production TU
compile-only result remain historical dependencies. No additional host 15 or
compaction 30 suite was run for this gate. No new
production compile is required unless that source changes or a concrete
portability issue needs investigation. Linux GCC/Clang and full SSA47 remain
OPEN, as do descriptor/native/retention acceptance. No network/security/provider
or external messaging path is exercised.

See the [module guide](../../docs/testing-and-validation/ssa-primitive-source-frame-storage.md)
and [immutable evidence record](../../docs/acceptance/ssa-primitive-source-frame-storage.md).
