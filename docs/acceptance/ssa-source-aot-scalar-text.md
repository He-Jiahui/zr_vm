---
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text_internal.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c
  - zr_vm_core/src/zr_vm_core/aot_ir.c
  - zr_vm_parser/include/zr_vm_parser/aot_ir_projection_descriptor.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_module_contract.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_primitive_layout.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text_internal.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_text.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_scalar_llvm.c
plan_sources:
  - docs/plans/ssa/07-aot-backends/02-c-llvm-lowering.md
  - docs/parser-and-semantics/ssa-source-module-contract.md
tests:
  - tests/parser/test_ssa_source_aot_scalar_text.c
  - tests/parser/test_ssa_source_aot_descriptor.c
  - tests/parser/test_ssa_aot_scalar_text.c
  - tests/parser/test_ssa_aot_scalar_arithmetic.c
  - tests/parser/test_ssa_aot_scalar_conditional.c
doc_type: acceptance
status: finite-source-scalar-text-green
---

# Actual Source Scalar Text Acceptance

## Current decision

**Finite actual source scalar text GREEN at r3 after committed genuine RED.**
Failed implementation runs r1/r2 and diagnostic r4 remain historical evidence.
The complete C/LLVM milestone and whole 47-shape matrix remain OPEN.

Scope: the real finalized `9;` / `8;` SCRIPT pipeline, original module binding,
compaction preserving source maps, primitive frame/layout, canonical lowering,
host target and borrowed descriptor, then the existing C/LLVM buffer emitters.
See the [module contract](../parser-and-semantics/ssa-source-aot-scalar-text.md)
and [test guide](../../tests/acceptance/ssa-source-aot-scalar-text.md).

Root route is `01a0fe2b-2f6b-7063-9a67-ac0e08c0d82d`.
Sources are compiled directly from `E:/Git/zr_vm`.
The build directory is
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`.
Report names below are relative to
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b`.
No source snapshots or copies form this evidence.

## Committed genuine RED

Commit `9d6df0fdee43351b196c24e40a1537afdf14fa50`, parent
`5c2941ce5fa092f1c4e8fb25e2f3987dd1dcf9cc`, records the actual failing feature
tests. Author and committer are He-Jiahui <814111601@qq.com>. The parent includes
foreign work; it is the real observed parent, not a rewritten task-only history.
The committed paths are the direct-validation CMake file, source-execbc CMake
registration and `test_ssa_source_aot_scalar_text.c`.

Configure/build and two prerequisite cases exited 0. Four real source feature
cases (9/8 × C/LLVM) failed only the FEATURE RED acceptance assertion; feature
mode exited 4. Root's RED receipt pins the actual compilation graph and native
child terminal states. Default refusal/buffer cases were not proved at RED.

| Historical evidence | SHA-256 |
| --- | --- |
| `source-scalar-text-red-r2-receipt.json` | `054AA9678BEFDB6B2F5F5961251F3AF3414FFC83D28B4D2D4A99F62ACA3089BF` |

Prior source-binder GREEN `170f777` and primitive-frame OOM GREEN `0cca964`
retain their own finite scope. They do not prove this emitter feature.

## Retained failed implementation run r1

Windows x64 clang-cl 19 Debug assertions/UBSan configured and built with exit 0.
The five selected CTest suites exited 8: source scalar text failed 12 of 22
Unity cases, while descriptor20 and the three prior standalone regressions
passed. The receipt has `validated=false`. Neither successful compilation
nor its filename containing “green” makes this run functional GREEN.

The source profile wrongly required empty NOP range offsets to be zero.
Real descriptor empty ranges retain valid pool cursors. The repair preserved
source maps, frame slots and state-map records.
The final r3 evidence below verifies the cursor and actual NOP type corrections.

| Failed historical evidence | SHA-256 |
| --- | --- |
| `source-scalar-text-green-r1-receipt.json` | `139AE3E4CFEE3ABC1840FC90FCD2B33A8EB91FD8ADCE706ACD4C320269865648` |
| `source-scalar-text-green-r1-ctest.log` | `5006F34A4E1E96E39395633D6E98EE0C2CA09B459A1529A258491E453B672A92` |

## Retained failed implementation run r2

After the empty-range cursor change, the source suite still failed 12/22 at
the same actual NOP instruction 2. The change has not established a functional
repair or proved that cursor rejection was the only cause. Root subsequently
collected all NOP fields in the bounded r4 run; at r2 validation stayed pending.

Root driver PID89587 naturally exited 1. Configure PID31768 and build PID12432
exited 0; CTest PID4640 exited 8. Descriptor20 and the three prior standalone
regressions passed. MSVC r2 child PID6600 exited 0 with zero warnings; its Root
driver PID35982 naturally exited 0. This is changed-TU compile-only evidence.

| Failed r2 evidence | SHA-256 |
| --- | --- |
| `source-scalar-text-green-r2-receipt.json` | `78DFB90FA2307A6402DD33CD19108DEF2E01545716C4BC38AFC844C98D0871D1` |
| `source-scalar-text-green-r2-ctest.log` | `9D20A0AEC7402A46CE48802DADD80362891933EEB504A735823815568C3B8AC0` |

## Actual NOP diagnosis r4 and compile-only r3

The bounded r4 run observed the actual NOP with type token 1, matching the real
return ABI; flags, match/layout tokens, effects, binding/deopt references and
all range counts were zero. Its empty result range was `{offset=1,count=0}`.
These numbers describe that observation, not IDs or type constants hardcoded
into the profile. The consumer now requires the actual ABI return type token
instead of zero, and accepts valid empty cursors. It retains all other inert
constraints. The refusal fixture uses a genuinely different nonzero token,
checks Core accepts the perturbation, restores the record and asserts the tokens
differ. It does not clear producer fields to force admission.

Root PID45065 naturally exited 0. Configure PID8804, build PID6916 and
prerequisite PID34140 exited 0; the four isolated feature cases PID34720 exited
4. This diagnostic run retains the feature refusal; it is not functional GREEN.
MSVC r3 child PID35048 naturally exited 0 with zero warnings; it is compile-only.

| Diagnosis/compile evidence | SHA-256 |
| --- | --- |
| `source-scalar-text-red-r4-receipt.json` | `D928A8F6E6115468275348F2926CF69D0859B51A0086404B637D4985D8FF0850` |
| `source-scalar-text-red-r4-features.log` | `F6B653F51008D11D5A8E5C04E3E1BF302EE69D0ED2E4FF77AB9F4203F4AD1706` |
| `source-scalar-text-msvc-r3-receipt.json` | `F961D2DB64B6336B9E1EA4BDC3D92D1D6D6108C98F9F8740A878FDF11184B41D` |

## Final finite functional r3 and source pins

Root PID78969 naturally exited 0. Configure PID33716, build PID17708 and CTest
PID34064 naturally exited 0. All five selected suites passed: source22 and
descriptor20 = 42 Unity cases with zero failures/ignored, plus the three prior
standalone regressions run without arguments. The actual build recorded zero
warnings and no observed UBSan diagnostic. Both repaired constraints are tested
against the real descriptor without erasing its NOP type/cursors, source maps,
frame/value slots or empty state map. Projection runnable stays false.

| Final evidence | SHA-256 |
| --- | --- |
| `source-scalar-text-green-r3-receipt.json` | `C2472439DCB8E1290728B8630B5CDBAEBD731F824F8689FE2C42262C2B29DE5B` |
| `source-scalar-text-green-r3-configure.log` | `CCFD6EB2E5545F701BD8293DFF3D3DD760466C622EBF642F2FC6CED3B620DB9E` |
| `source-scalar-text-green-r3-build.log` | `C65FAF47933ADEBEFD9111573EE839E49CF7DC88E3F8BF0113C6DB57CF66ACFF` |
| `source-scalar-text-green-r3-ctest.log` | `087DB0015B16686F0C3ED78E563FFCEBDC9AD1EE6B9AA1CC7BA10445FAC4DDD0` |
| `tests/parser/test_ssa_source_aot_scalar_text.c` | `E55152A322E8AF062745B9F8AC87AD231E8D6FFDB11FE8AEC28774BAD4E19723` |
| `backend_aot_ir_scalar_text.c` | `9B820C49B93DB63C2A70C44DB5344F88F8A02EE910B9C5C4851064EE80E5F8F0` |
| `backend_aot_ir_scalar_text.h` | `50B5EC5A63F2B6376BF5A44FC9CA6ECE09DD28EBF8645A89224690A6E0F04C9B` |

The last two source names are under
`zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/`.
The receipt pins the actual direct-source metadata in the exact build directory:
`source-direct-target-sources.tsv`, `source-direct-target-settings.tsv`,
`source-direct-header-hashes.tsv` and `compile_commands.json`.
Each actual source path must be rooted in the checkout; pins observe real
owners/source files and are not an alternate source snapshot.

The r1/r2 MSVC compile results preceded the complete repair. Final r3 MSVC
child PID35048 compiled the pinned shared scalar implementation with exit 0 and
zero warnings; receipt `F961D2DB64B6336B9E1EA4BDC3D92D1D6D6108C98F9F8740A878FDF11184B41D`
above identifies this changed-TU compile-only proof. It does not imply a full
MSVC matrix or generated-code execution. Root will record any GREEN commit
identity separately after creating it; this document does not guess that identity.

## Explicit OPEN scope

Projection runnable remains false. Source execution, oracle, generated
executable/native registration or loader, network, FFI, provider and security
probe evidence are absent. Linux is OPEN after permission denial, full MSVC is
OPEN, normal returned descriptor use after owner release is OPEN, and independent
Core ownership is not implemented. Its lifetime test is authored but not registered
or validated. Plan 07.02 and the complete general/native 47-shape typed
matrix remain planned/unchecked.
