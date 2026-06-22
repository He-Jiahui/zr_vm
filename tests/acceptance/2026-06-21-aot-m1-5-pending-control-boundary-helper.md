# AOT M1.5 07-S5 Pending-Control Boundary Helper

## Scope

- `SET_PENDING_RETURN`, `SET_PENDING_BREAK`, and `SET_PENDING_CONTINUE` C lowering now keeps the existing generated markers, initializes `zr_aot_next_instruction`, calls the corresponding `ZrLibrary_AotRuntime_SetPending*` helper, and dispatches when the helper returns a resume instruction.
- Existing runtime helpers own pending-value lookup, pending-control state setup, outer-finally resume, target jump, pending cleanup, frame refresh, and resume-index calculation.
- Affected layers: AOT C control lowering, aggregate source contracts, generated shared-library smoke, semantic docs, and AOT 07 plan records.

## Baseline

- RED: after flipping the source contract to helper-only pending control, `zr_vm_aot_c_source_contracts_test` failed 1/19 while the generator still emitted the old expanded pending-control block. The initial source-level marker expectation was narrowed from generated comment text to source marker text because generated comment shape is covered by the shared-library smoke.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Aggregate AOT source contract: `zr_vm_aot_c_source_contracts_test`.
- Generated C compile smoke: `zr_vm_aot_c_shared_library_smoke_test`, including the pending-control hand-built opcode fixture.
- Broader focused AOT group: source, shared-library aggregate, return, frame setup, typed scalar, value SemIR, call, control, and global contract/smoke binaries.
- Generated-product boundary check: pending-control generated C must contain all three `SetPending*` helpers and resume dispatch, and must not contain the old generated pending-value local, `execution_set_pending_control`, outer-finally resume, direct jump, or pending target-offset template.

## Tooling Evidence

- Toolchain: WSL Ubuntu-22.04, GCC/Ninja build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ```
- GREEN focused commands:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ```
- Broader focused binaries were run directly after building the relevant targets: source, shared-library aggregate, return, frame setup, typed scalar, value SemIR, call, call shared-library smoke, control, control shared-library smoke, global, and global shared-library smoke.

## Results

- RED observed: `Missing source contract text: /* zr_aot_pending_return */` from the first flipped contract. The source contract was tightened to require source-level markers plus `ZrLibrary_AotRuntime_SetPendingReturn/Break/Continue(...)` templates, leaving generated comment shape to the generated-C smoke.
- GREEN focused results: source contracts 19/0 and aggregate shared-library smoke 8/0.
- GREEN broader results: source contracts 19/0, aggregate shared-library smoke 8/0, return contracts 1/0, frame setup contracts 1/0, typed scalar 1/0, value SemIR contracts 4/0, call contracts 4/0, call shared-library smoke 3/0, control contracts 1/0, control shared-library smoke 1/0, global contracts 6/0, global shared-library smoke 7/0.
- Generated check passed: `aot_c_pending_control_direct.c` contains `ZrLibrary_AotRuntime_SetPendingReturn(state, &frame, 0, 3, &zr_aot_next_instruction)`, `ZrLibrary_AotRuntime_SetPendingBreak(state, &frame, 3, &zr_aot_next_instruction)`, `ZrLibrary_AotRuntime_SetPendingContinue(state, &frame, 3, &zr_aot_next_instruction)`, and `goto zr_aot_fn_0_dispatch;`; it does not contain `SZrTypeValue *zr_aot_pending_value = ZR_NULL;`, `execution_set_pending_control(state,`, `execution_resume_pending_via_outer_finally(state, &zr_aot_call_info)`, `execution_jump_to_instruction_offset(state,`, or `state->pendingControl.targetInstructionOffset`.
- A combined build-and-run command for the aggregate shared-library smoke exceeded the command timeout before output was captured; the build and test were rerun as separate commands and passed.
- Large-file note: `tests/parser/test_aot_c_shared_library_smoke.c` is 1087 lines. This slice only flipped assertions in the existing pending-control fixture and did not add a new fixture/helper responsibility, so a structural split was deferred. The smallest follow-up boundary is extracting shared-library smoke scaffolding/control fixtures out of the aggregate smoke file.

## Acceptance Decision

- Accepted for the pending-control boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
