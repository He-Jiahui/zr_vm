# AOT 07-S6 reference-local safepoint GC pressure

Timestamp: 2026-07-06 07:29:17 +08:00

Plan slice: M1.5 / 07-S6 reference-local GC pressure/root correctness

Status: focused safepoint GC-pressure sub-slice completed; 07-S6 remains partial and the broader 07~12 goal continues.

## Scope

- Exercise the new `LOCAL_ADDRESS` reference-local root frame under a real safepoint after `TO_STRING` / `TO_OBJECT`
  writes `zr_aot_ref_locals.oN` and before immediate string/object truthiness consumes the raw object.
- Keep the ordinary VM-stack `FRAME_BYTE_OFFSET` root map separate from the reference-local root map.
- Add smoke assertions that force pending GC debt and require `gcLastStepWork > 0` after generated AOT execution.

## RED

- WSL GCC `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test` failed 8 tests / 2 failures after the dynamic
  string/object branches required `zr_aot_gc_safepoint_reference_local` after the `zr_aot_ref_locals.o2` assignment.
- WSL GCC `zr_vm_aot_c_source_contracts_test` failed 24 tests / 1 failure after the source contract required
  `backend_aot_write_c_gc_safepoint(file, "        ", "zr_aot_gc_safepoint_reference_local");`.

## GREEN

- `backend_aot_write_c_direct_to_string()` and `backend_aot_write_c_direct_to_object()` now emit
  `/* zr_aot_gc_safepoint_reference_local */` plus `ZrCore_Gc_SafePoint(state)` immediately after the generated
  reference-local writeback.
- Logical-not and jump-if dynamic string/object smoke cases assert that the safepoint marker appears after
  `zr_aot_ref_locals.o2 = ...` and before `ZR_CAST_STRING(...)` or object truthiness consumption.
- The same smoke cases run with generational GC debt pending and assert that the safepoint performs GC work through
  `gcLastStepWork > 0`.

## Verification

- WSL GCC focused direct run: logical-not 8/0, jump-if 9/0, source contracts 24/0, logical contracts 4/0, frame setup
  contracts 1/0.
- WSL Clang focused build and direct run: logical-not 8/0, jump-if 9/0, source contracts 24/0, logical contracts 4/0,
  frame setup contracts 1/0. The only diagnostics were the existing smoke-helper `const char *` to `TZrNativeString`
  qualifier warnings.
- Windows MSVC Debug focused build passed. Direct run reports the Unix-only smoke bodies as 8/9 expected ignores with
  zero failures, and passes source/logical/frame setup contracts at 24/0, 4/0, and 1/0.
- Generated GCC C inspection confirmed `ZrCore_Gc_SafePoint(state)` appears after `zr_aot_ref_locals.o2` writeback in
  dynamic string/object logical-not and jump-if projects.

## Open Items

- Longer-running GC stress, exports/frame cleanup, in/out writeback, performance counters, complete 07-S6 acceptance,
  and broader 07~12 completion remain open.
