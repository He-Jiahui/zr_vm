# AOT 09-S3A Safepoint Insertion Acceptance

## Scope

- Slice: 09-S3A, safepoint API plus generated-C insertion at allocation, call, and loop back-edge boundaries.
- Runtime: `ZrCore_Gc_SafePoint(state)` is the controlled poll used by generated AOT code and currently delegates to `ZrCore_GarbageCollector_CheckGc(state)`.
- AOT C backend: emits `ZrCore_Gc_SafePoint(state);` after object/array allocation, after function calls, and before `goto` only when the branch target is a back-edge.
- Tests: runtime debt progress, source contracts, generated-C marker smoke, and cross-toolchain focused verification.

## Baseline

- RED 1: the new runtime safepoint test could not compile before `ZrCore_Gc_SafePoint()` existed.
- RED 2: the control source contract failed before the emitter exposed `backend_aot_write_c_gc_safepoint()` and before allocation/call/back-edge marker strings existed in the lowering sources.
- RED 3: generated-C smoke initially had no `zr_aot_gc_safepoint_allocation`, `zr_aot_gc_safepoint_call`, or `zr_aot_gc_safepoint_back_edge` markers to assert.

## Implementation Summary

- Added public GC safepoint API in `zr_vm_core/include/zr_vm_core/gc.h` and implementation in `zr_vm_core/src/zr_vm_core/gc/gc.c`.
- Added AOT emitter helper `backend_aot_write_c_gc_safepoint(FILE *, const char *, const char *)`.
- Allocation lowering now emits an allocation safepoint after `ZrLibrary_AotRuntime_CreateObject()` and `ZrLibrary_AotRuntime_CreateArray()`.
- Function-body call dispatch now emits a call safepoint after static typed, direct, dynamic, and fallback call lowering paths.
- Branch lowering receives an `isBackEdge` flag from function-body dispatch; back-edge safepoints are emitted before the generated `goto` for direct, generic truthiness, typed signed, bool-false, and iterator branches.

## Test Inventory

- `tests/core/test_aot_gc_root_frame.c`: root-frame coverage plus `ZrCore_Gc_SafePoint()` pending-debt progress.
- `tests/parser/test_aot_c_control_contracts.c`: source-level API/helper/marker contract.
- `tests/parser/test_aot_c_control_shared_library_smoke.c`: generated-C smoke for allocation, call, and back-edge markers, plus Unix shared-library compile.
- `tests/parser/test_aot_c_value_type_shared_library_smoke.c`: existing generated-C value-type/root-map smoke remains green.
- `tests/parser/test_aot_c_iterator_contracts.c`: iterator back-edge source path remains green.

## Boundary Cases

- Pure straight-line scalar code remains poll-free unless it crosses allocation or call boundaries.
- Forward branches do not emit back-edge safepoints.
- Back-edge detection is local and deterministic: `targetInstructionIndex <= instructionIndex`.
- Generated C still depends on 09-S2B root-frame registration for moving-GC root rewriting at safepoints.
- This slice does not implement 09-S4 write barriers, 09-S5 boxing boundary changes, or FFI pin/unpin.

## Tooling Evidence

- WSL gcc build targets passed: `zr_vm_aot_gc_root_frame_test`, `zr_vm_aot_c_control_contracts_test`, `zr_vm_aot_c_control_shared_library_smoke_test`, `zr_vm_aot_c_value_type_shared_library_smoke_test`, `zr_vm_aot_c_iterator_contracts_test`.
- WSL gcc direct tests passed: root-frame 3/0, control contracts 2/0, value-type smoke 2/0, iterator contracts 1/0, control shared-library smoke 2/0.
- WSL gcc CTest passed: `aot_gc_root_frame` 1/1.
- WSL clang focused build passed for root-frame, control contracts, control shared-library smoke, and value-type smoke.
- WSL clang direct tests passed: root-frame 3/0, control contracts 2/0, control shared-library smoke 2/0, value-type smoke 2/0.
- WSL clang CTest passed: `aot_gc_root_frame` 1/1.
- Windows MSVC Debug focused build passed for root-frame, control contracts, control shared-library smoke, and value-type smoke.
- Windows MSVC Debug direct tests passed: root-frame 3/0, control contracts 2/0, control shared-library smoke 2/0 with Unix shared-library branches ignored, value-type smoke 2/0 with Unix execution branch ignored.
- Windows MSVC Debug CTest passed: `aot_gc_root_frame` 1/1.

## Non-Acceptance Residual

- `zr_vm_aot_c_logical_contracts_test` still has 2 failures in the current tree.
- Investigation showed those failures are stale scalar-local source-contract assertions looking for old explicit text in `backend_aot_c_scalar_locals.c`; the implementation now uses generalized result-opcode inference.
- The generated function-body still contains the logical opcode dispatch text required by the current safepoint work, and the residual failure is unrelated to 09-S3A allocation/call/back-edge insertion.

## Acceptance Decision

Accepted for 09-S3A. Runtime safepoints advance GC debt, generated C exposes the three planned safepoint classes, and focused WSL gcc, WSL clang, and Windows MSVC verification passed. 09-S4 write barriers and 09-S5 boxing/FFI pinning remain open.
