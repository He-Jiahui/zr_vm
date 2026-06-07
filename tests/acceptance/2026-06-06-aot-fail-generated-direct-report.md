# AOT C Generated-Fail Direct Report Acceptance

## Scope

This slice removes the generated-C dependency on `ZrLibrary_AotRuntime_FailGeneratedFunction(state, &frame)` for generated function failure exits. `ZR_AOT_C_FAIL()` now reports directly through `ZrCore_Debug_RunError`, records generated function/instruction metadata, and leaves through the existing cleanup-aware `ZR_AOT_C_RETURN(0)` path.

The slice does not replace generated-frame begin setup or module export publication. Those remain explicit runtime boundaries.

## RED

After extending `test_aot_c_source_emits_value_frame_cleanup_exit`, GCC failed as expected:

```text
test_aot_c_source_emits_value_frame_cleanup_exit:FAIL
Expected Non-NULL
```

The missing contract marker was `ZrCore_Debug_RunError(state,` inside the generated fail macro. The test also forbids `ZrLibrary_AotRuntime_FailGeneratedFunction(state, &frame)`.

## GREEN

Implementation changed the generated fail macro in `backend_aot_c_emitter.c` to emit:

- `ZrCore_Debug_RunError(state, "generated AOT function failed: functionIndex=%u instructionIndex=%u", ...)`
- `(unsigned)frame.functionIndex`
- `frame.currentInstructionIndex == ZR_AOT_RUNTIME_RESUME_FALLTHROUGH ? UINT32_MAX : ...`
- `ZR_AOT_C_RETURN(0);`

`backend_aot_c_function_body.c` now initializes the generated frame with `ZrAotGeneratedFrame frame = {0};`, so failure before generated-frame begin setup has defined metadata.

A first generated-library build exposed a host-emitter warning because the generated `%u` format string was not escaped for `fprintf`; the emitter now writes `%%u`, while generated C receives the intended `%u` format string.

## Validation

GCC:

```text
zr_vm_aot_c_source_contracts_test: 17 tests, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 8 tests, 0 failures
```

Clang:

```text
zr_vm_aot_c_source_contracts_test: 17 tests, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 8 tests, 0 failures
```

MSVC:

```text
zr_vm_aot_c_source_contracts_test.exe: 17 tests, 0 failures
zr_vm_aot_c_shared_library_smoke_test.exe: 8 tests ignored as Unix-only runtime checks
```

## Production Scans

Checked `backend_aot_c*.c` files no longer emit:

```text
ZrLibrary_AotRuntime_FailGeneratedFunction
```

The remaining checked C-backend runtime helper boundaries are:

```text
ZrLibrary_AotRuntime_BeginGeneratedFunction
ZrLibrary_AotRuntime_PublishModuleExports
```

`git diff --check` still reports only existing LF-to-CRLF warnings in the dirty worktree, with no whitespace error introduced by this slice.

## Remaining Risk

`BeginGeneratedFunction` still owns generated-frame setup, and `PublishModuleExports` still owns root-module export materialization. LLVM helper parity is also outside this slice.
