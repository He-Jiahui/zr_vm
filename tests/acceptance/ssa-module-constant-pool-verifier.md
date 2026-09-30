# ExecIR module constant pool verifier

## Contract under test

- With `module.constantCount > 0`, each `CONSTANT.layoutId` names an entry in
  `module.constants`; `VerifyModule` must reject an index at the exclusive
  count boundary and `UINT32_MAX`, and reject a pool entry whose type differs
  from the instruction result type.
- A nonzero explicit instruction type must also equal the result type. Type
  tokens are compared as opaque identities, without assuming runtime enums.
- These failures must keep a structured diagnostic: code, function token,
  entry block, instruction ID, source ID, expected value, and actual value.
- A module with no owned constants remains valid when its function's constant
  pool is configured separately by the Oracle input. `VerifyFunction` has no
  module argument and is not expected to prove module-pool references.

## Evidence

- `zr_vm_core/include/zr_vm_core/exec_ir.h:410-414,508-525` declares a module
  owned constant array/count and a constant's type token, flags, and bits.
- `tests/parser/test_ssa_core_model.c:356-385,441-445` constructs a valid
  module pool, writes its range start to `CONSTANT.layoutId`, then accepts the
  module through `VerifyModule`.
- `zr_vm_core/include/zr_vm_core/exec_ir_interpreter.h:157-165` exposes a
  separate optional Oracle constants array/count; `exec_ir_build.c:675-681`
  lowers a SemIR pool index into `layoutId`, while
  `exec_ir_build.c:924-964` builds a module by publishing the function without
  accepting or copying the external pool.
- `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_validate.c:205-234`
  validates constant indexes and result types when a projection pool exists.
- EIS5 persistence tests validate decoded modules with their own serialized
  pool (`tests/library/test_ssa_exec_ir_artifact_v6_eis5.inc:334-339,434-439`).
- The plan's canonical module shape lists `constants` but does not state a
  no-pool `CONSTANT` restriction (`docs/plans/ssa/01-execir-ssa/01-core-model.md:78`).
  The optional external pool behavior is explicit in the Oracle API; no
  stronger no-pool persistence rule was found in the reviewed plan/acceptance
  documents.

## Implementation and validation

- `tests/parser/test_ssa_module_constant_pool_verifier.c` owns a minimal valid
  one-block Core module fixture; its cases are in
  `tests/parser/test_ssa_module_constant_pool_verifier_cases.inc`.
- The root-owned MSVC RED is recorded in
  `D:/tmp/zr_vm/ssa-control/resumed-slices-ctest.log`: a CONSTANT index equal to
  `constantCount` was accepted. The valid fixture passed before that mutation.
- `exec_ir_verify_constants.c` runs after each function passes full verification
  in `VerifyModule`. It checks the owned pool bound before dereferencing a
  constant and then checks both type identities. A zero-sized module pool is
  left to the external consumer. `VerifyFunction` remains module-independent.
- Seven cases cover valid input, exclusive-count and `UINT32_MAX` indexes,
  constant and explicit instruction type mismatches, function-only/no-pool
  verification, and execution with a separately configured Oracle pool.
- CTest `ssa_module_constant_pool_verifier` is registered by
  `tests/cmake/ssa-module-constant-pool-verifier.cmake`. The fresh MSVC run
  passed all seven cases in `canonical-constants-eis6-ctest.log`. The initial
  test selection had two unrelated VM target timeouts; both targets then
  passed under their unchanged CTest limits in
  `canonical-vm-timeout-recheck.log`.
- The current direct-consumer build, including Core model/effects, builder,
  state-map and Oracle targets, linked all 14 targets successfully in
  `current-scalar-place-queries-build.log`. The latest direct execution of the
  constant verifier returned zero and `ssa module constant pool verifier PASS`
  in `current-module-constants-direct.log`.
- The broader 16-test selection passed 13 tests, including this verifier in
  0.81 seconds, and recorded two VM target timeouts. It also exposed a state-map
  fixture failure in `VerifyFunction`: branch instruction successor ranges
  were empty while block successor ranges were nonempty. This occurs before
  the module-only constant check. No complete semantic-matrix or current
  GCC/Clang success is claimed by this record. The full selection is recorded
  in `current-scalar-place-queries-ctest.log`.
