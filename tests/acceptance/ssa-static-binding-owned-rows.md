# SSA 03.02: Core-owned typed binding rows

## Scope

This bounded slice gives each `SZrExecIrFunction` an explicit binding-row schema
and an owned row table. Schema `0` remains the historical mode and owns no table.
Schema `1` owns typed rows, including an empty table. In typed mode,
`bindingRow=0` means no row and references `1..N` resolve to
`bindingRows[reference - 1]`.

Core copies, validates, hashes, clones, and frees the full rows. Parser projection
copies producer facts and publishes row references transactionally. Core Verify
checks table shape and both directions of the row/instruction association. EIS1
through EIS5 do not carry the schema marker or rows, so scalar artifact writers
reject schema 1 instead of silently dropping that information.

This slice does not define a typed call target resolver. A structurally valid
typed CALL/INVOKE with a row still returns `UNSUPPORTED` in the generic Oracle,
without calling the legacy callback. Schema and association validation happens
before interpreter callbacks; schema 0 retains legacy prototype behavior.
Call-graph and optimization consumers either decode complete rows or fail closed;
they must never interpret a row reference as a `FunctionId`.

## Focused assertions

| Test | Evidence |
| --- | --- |
| `test_ssa_static_binding_facts.c` | projection owns rows, uses 1-based references, supports typed-empty, and preserves old state on failed replacement |
| `test_binding_rows_are_owned_typed_and_transactional` | Core copy/clone independence, hash coverage, schema transitions, invalid table shapes, direct accessor rejection of corrupt count/capacity and storage metadata, and known versus unknown opcode diagnostics |
| `test_binding_rows_pass_complete_verifier_gate` | complete constant/CALL/return block passes `VerifyFunction` and `VerifyModule`; a broken reciprocal reference fails both with instruction/source and expected/actual reference diagnostics |
| `test_ssa_oracle_call_differential.c` | valid typed calls do not enter the generic callback; schema 2 and a broken schema-1 reciprocal reference are rejected before callback, preserving the prior Oracle result |
| `test_ssa_pass_manager_scalar.c` | a typed zero-effect CALL is retained by scalar cleanup, and changing only its owned row's signature hash changes `FunctionHash` |
| `test_ssa_value_validation.c` | direct SSA promotion rejects malformed typed rows before its no-memory-access fast return |
| `test_ssa_loops_specialization.c` | LICM preserves an owned CALL row and remaps its instruction reference after hoisting; the fixture synthesizes CFG effects and verifies before and after |
| `test_ssa_binding_rows_artifact.c` | schema-zero EIS1 writing remains available; typed-empty rows are rejected by direct EIS3/EIS4/EIS5 writers and the public scalar router |

The direct-source audit found 21 CMake source lists that compile Core
`exec_ir.c` (20 in `ssa-tests.cmake`, one in `ssa-builder-tests.cmake`); each now
also compiles `exec_ir_binding_rows.c`. Direct Oracle targets include this helper.
The container-specialization target links the Core library for the new Core row
API. The loop-specialization target also compiles `exec_ir_effects.c`,
`exec_ir_effects_linear.c`, and `exec_ir_effect_loops.c` so its fixture uses the
real `ZrParser_ExecIr_SynthesizeCfgEffects` producer.

## Verification record

The initial test-first MSVC run exposed the historical zero-based projection
reference: four positive projection assertions failed with `Expected 1 / Was 0`.
The producer now publishes 1-based row references.

Two later failures were fixture or target-source issues and were corrected
without relaxing Core validation:

- The Oracle call fixture initially could not install its typed row because its
  module/signature identity metadata did not match. The fixture now supplies
  matching nonzero identity metadata. It still asserts that a valid typed CALL
  returns `UNSUPPORTED` before the generic callback, and that unknown schema
  and a broken reciprocal reference are rejected while preserving the prior
  Oracle result.
- The loop-specialization fixture initially used a stale value-definition
  instruction ID after moving the invariant constant. It now records the
  constant's moved definition as instruction 5, synthesizes effect tokens using
  `ZrParser_ExecIr_SynthesizeCfgEffects`, and verifies the fixture before and
  after LICM. The first native relink also exposed three missing producer
  sources in the hand-built test target; adding `exec_ir_effects.c`,
  `exec_ir_effects_linear.c`, and `exec_ir_effect_loops.c` closes that fixture
  dependency. Core validation was not weakened.

Before the direct-accessor metadata guard below was added, the focused MSVC
native build completed 13/13 actions with exit code 0; its log is
`D:/tmp/zr_vm/ssa-control/binding-final-build.log`. The registered binding
CTest set passed 12/12 with exit code 0 in 219.03 seconds; its log is
`D:/tmp/zr_vm/ssa-control/binding-ctest.log`. The 12 tests were:

```text
ssa_core_model
ssa_binding_rows_artifact
ssa_value_validation
ssa_oracle_projections
ssa_pass_manager_scalar
ssa_escape_ownership
ssa_interprocedural_inlining
ssa_loops_specialization
ssa_static_binding_facts
ssa_aot_projection_descriptor
container_specialization
ssa_generated_fusion
```

The focused reciprocal-reference fixture build also completed 31/31; its log is
`D:/tmp/zr_vm/ssa-control/reciprocity-fixtures-build.log`. These builds and tests
predate the current accessor change and do not verify the current source.

The latest review found that `FunctionBindingRowAt` trusted the public
`bindingRowCount`/`bindingRowCapacity` pair before indexing. A corrupt one-row
owned table changed to `count=2, capacity=1` could therefore return the
one-past pointer for reference 2. The accessor now checks the O(1) metadata
shape before indexing, and the existing lifecycle test covers that case and
both pointer/capacity mismatch directions while restoring the owned pointer
before cleanup.

After this accessor guard, the focused MSVC native build completed 13/13
actions with exit code 0; its log is
`D:/tmp/zr_vm/ssa-control/accessor-vm-gc-build.log`. The coupled CTest run
passed 5/5 with exit code 0 in 2.08 seconds; its log is
`D:/tmp/zr_vm/ssa-control/accessor-vm-ctest.log`. It covers
`ssa_core_model`, `ssa_binding_rows_artifact`, `ssa_oracle_projections`,
`ssa_static_binding_facts`, and the VM16 `ssa_exec_ir_execbc_vm` target.
The earlier binding 12/12 CTest and reciprocal-reference 31/31 build were
completed before this accessor guard; they remain historical evidence and
were not rerun after it. The current focused gates verify the accessor repair
and its coupled behavior without claiming a post-guard rerun of all 12 tests.

These focused gates do not close the remaining 03.02 requirements. Typed CALL
target resolution and execution remain unsupported: a well-formed typed CALL
still must not fall through to the generic Oracle callback. EIS1 through EIS5
still cannot persist schema 1 rows, and typed rows are rejected by those scalar
writers rather than serialized. Full typed target resolution, remaining
consumer forms, and artifact persistence remain open.
