# AOT TYPEOF Direct Reflection Lowering

Date: 2026-06-05

Scope: generated AOT C `TYPEOF` lowering.

## Contract

- Generated AOT C lowers `TYPEOF` to a direct `ZrCore_Reflection_TypeOfValue` call instead of the AOT runtime instruction helper.
- Generated modules include `zr_vm_core/reflection.h`.
- The emitted block resolves the destination and source through `ZrCore_Stack_GetValue(frame.slotBase + slot)`, emits the `zr_aot_value_exec_typeof` marker, and fails through `ZR_AOT_C_FAIL()` if the destination is invalid or reflection fails.
- The C backend no longer emits `ZrLibrary_AotRuntime_TypeOf` from the value/function-body lowering path. LLVM still keeps its existing helper route until LLVM parity is handled.

## RED

The focused contract was added before production code and failed as expected:

```text
Missing source contract text: #include \"zr_vm_core/reflection.h\"\n
/mnt/e/Git/zr_vm/tests/parser/test_aot_c_global_contracts.c:87:test_aot_c_source_lowers_typeof_to_direct_reflection_call:FAIL:missing required source contract text
2 Tests 1 Failures 0 Ignored
```

## Validation

- GCC focused/global set in `build-wsl-gcc`: source contracts 17 tests, constant contracts 3 tests, global contracts 2 tests, global shared-library smoke 2 tests, shared-library smoke 6 tests, all 0 failures.
- Clang focused/global set in `build-wsl-clang`: source contracts 17 tests, constant contracts 3 tests, global contracts 2 tests, global shared-library smoke 2 tests, shared-library smoke 6 tests, all 0 failures.
- MSVC focused set in `build/codex-msvc-aot-constant-debug`: source contracts 17 tests, constant contracts 3 tests, global contracts 2 tests, all 0 failures.
- MSVC global shared-library smoke in `build/codex-msvc-aot-global-debug`: 2 tests, 0 failures, 2 ignored because generated shared-library compilation is Unix-only.
- A C-backend source scan found no `ZrLibrary_AotRuntime_TypeOf` emission in the AOT C value/function-body lowering sources; remaining references are LLVM-only.

## Files

- `tests/parser/test_aot_c_global_contracts.c`
- `tests/parser/test_aot_c_global_shared_library_smoke.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
