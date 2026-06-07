# AOT C Export Publication Direct-Core Acceptance

## Scope

This slice removes the generated-C dependency on `ZrLibrary_AotRuntime_PublishModuleExports(state, &frame)`.

Generated root/export returns now emit `zr_aot_publish_exports_direct`, iterate `frame.function->exportedVariables`, read export values from `frame.slotBase`, publish public/protected exports through `ZrCore_Module_AddPubExport` / `ZrCore_Module_AddProExport`, and set the module-executed flag directly.

Callable export materialization still needs private AOT record context, so generated C calls the narrow `ZrLibrary_AotRuntime_MaterializeModuleExportValue` boundary for that value conversion only. Generated-frame begin setup remains a runtime boundary.

## RED

After extending `zr_vm_aot_c_return_contracts_test`, GCC failed as expected:

```text
Missing source contract text: struct SZrObjectModule *module;
test_aot_c_source_lowers_export_return_to_direct_publication_then_direct_return:FAIL
```

The source contract also forbids generated C from emitting:

```text
/* zr_aot_publish_exports_boundary */
ZrLibrary_AotRuntime_PublishModuleExports(state, &frame)
```

## GREEN

Implementation changed:

- `ZrAotGeneratedFrame` now carries `struct SZrObjectModule *module` and `TZrBool *moduleExecuted`.
- `ZrLibrary_AotRuntime_BeginGeneratedFunction` populates those fields from the loaded AOT record.
- `ZrLibrary_AotRuntime_MaterializeModuleExportValue` exposes callable export value materialization while keeping private thunk binding inside the runtime.
- Generated C includes `zr_vm_core/module.h` and `zr_vm_common/zr_ast_constants.h`.
- `backend_aot_write_c_publish_exports()` now emits the direct generated export loop and no longer emits the full publication helper call.
- The LLVM generated-frame type was extended with the two trailing pointer fields so the shared runtime frame ABI size stays consistent.

## Validation

GCC:

```text
zr_vm_aot_c_source_contracts_test: 17 tests, 0 failures
zr_vm_aot_c_return_contracts_test: 1 test, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 8 tests, 0 failures
```

Clang:

```text
zr_vm_aot_c_source_contracts_test: 17 tests, 0 failures
zr_vm_aot_c_return_contracts_test: 1 test, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 8 tests, 0 failures
```

MSVC:

```text
zr_vm_aot_c_source_contracts_test.exe: 17 tests, 0 failures
zr_vm_aot_c_return_contracts_test.exe: 1 test, 0 failures
zr_vm_aot_c_shared_library_smoke_test.exe: 8 tests ignored as Unix-only runtime checks
```

GCC/Clang still report pre-existing const-discard warnings in `zr_vm_library/src/zr_vm_library/project/project.c`. MSVC still reports pre-existing warnings in `aot_runtime.c` and `project.c`.

## Production Scans

Checked `backend_aot_c*.c` files no longer emit:

```text
ZrLibrary_AotRuntime_PublishModuleExports
zr_aot_publish_exports_boundary
```

The remaining checked C-backend `ZrLibrary_AotRuntime_` emission is:

```text
ZrLibrary_AotRuntime_BeginGeneratedFunction
ZrLibrary_AotRuntime_MaterializeModuleExportValue
```

`git diff --check` over the touched tracked files reports no whitespace errors.

## Remaining Risk

Generated-frame begin setup still depends on `ZrLibrary_AotRuntime_BeginGeneratedFunction`, and callable export materialization still depends on private AOT record/thunk context. LLVM still uses its older helper-oriented return lowering beyond the frame-size ABI update. A real source-level callable-export execution fixture would further strengthen this direct publication path.
