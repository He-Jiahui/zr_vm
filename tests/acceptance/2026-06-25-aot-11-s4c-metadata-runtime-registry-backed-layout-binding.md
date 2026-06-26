# 2026-06-25 AOT 11-S4C Metadata Runtime Registry-Backed Layout Binding

## Scope

- Changed `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()` so the returned `SZrTypeLayout` pointer is sourced from `runtime->codeRegistration->typeLayouts[typeLayoutId]`.
- Affected layers: core metadata runtime, module code-registration consumption, metadata runtime query tests, AOT metadata plan documentation.

## Baseline

- 11-S4A returned a layout pointer from `metadataFunction->prototypeFrameTypeLayouts[typeLayoutId]` when the cached layout's `cTypeId` matched the TypeDef row.
- 11-S4B added the generated-C code-registration layout registry, but the TypeDef binding view did not yet read that registry.

## Test Inventory

- Focused core test: `zr_vm_metadata_runtime_query_test`.
- Regression contracts: `zr_vm_aot_c_frame_setup_contracts_test`, `zr_vm_aot_c_source_contracts_test`.
- Runtime/generated integration: `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_value_type_shared_library_smoke_test`, `zr_vm_aot_c_descriptor_diagnostics_test`.
- Boundary case covered: a TypeDef row with both a stale prototype layout pointer and a code-registration registry pointer must return the registry pointer.

## Tooling Evidence

- RED command:
  `wsl bash -lc 'cmake --build build/codex-wsl-gcc-debug --target zr_vm_metadata_runtime_query_test -j2 && ./build/codex-wsl-gcc-debug/bin/zr_vm_metadata_runtime_query_test'`
- RED output: `test_metadata_runtime_typedef_layout_binding_uses_code_registration_registry` failed because the expected registry pointer differed from the returned prototype layout pointer.
- GREEN WSL gcc/clang commands rebuilt and ran metadata runtime query, frame setup, source contracts, shared-library smoke, value-type shared-library smoke, and descriptor diagnostics in `build/codex-wsl-gcc-debug` and `build/codex-wsl-clang-debug`.
- GREEN Windows command rebuilt and ran the same targets from `build/codex-msvc-debug` using Visual Studio 2022 Developer Command Prompt.

## Results

- WSL gcc: metadata runtime query 20/0, frame setup 1/0, source contracts 19/0, shared-library smoke 8/0, value-type shared-library smoke 2/0, descriptor diagnostics 2/0.
- WSL clang: metadata runtime query 20/0, frame setup 1/0, source contracts 19/0, shared-library smoke 8/0, value-type shared-library smoke 2/0, descriptor diagnostics 2/0.
- Windows MSVC Debug: metadata runtime query 20/0, frame setup 1/0, source contracts 19/0, shared-library smoke 8/0 with 8 Unix-only branches ignored, value-type shared-library smoke 2/0 with 1 Unix-only branch ignored, descriptor diagnostics 2/0 with 2 Unix-only branches ignored.
- Existing warnings observed but not introduced as failures: clang generated-C logical-not parentheses warning, MSVC `argumentNode` possible-uninitialized warning.

## Acceptance Decision

Accepted for 11-S4C. TypeDef layout binding now reads the code-registration layout registry. Remaining 11-S4 work includes TypeSpec/generic layout materialization, ownership offset emission, runtime layout construction, reflection/generic/GC unified consumers, and the complete token/cTypeId/layout cache.

Size note: `tests/module/test_metadata_runtime_query.c` is already above 1000 lines. This slice added one tightly scoped metadata-runtime query case to the existing shared-fixture test target. If 11-S4 adds more layout-binding cases, the next cleanup boundary should extract the S4 layout binding coverage into a dedicated metadata-runtime layout-binding test file.
