# 2026-06-25 AOT 11-S4H / 10-S4A Reflection Layout Registry Consumer

## Scope

- Plan sources: `docs/plans/aot/11-metadata.md` §4 token↔cTypeId↔ZrLayout table and `docs/plans/aot/10-reflection.md` §3 field offset/layout reflection.
- Slice target: let reflection type/member layout consumers read `SZrTypeLayout` from the attached AOT code-registration layout registry when available.
- Non-goals: generic parameter reflection, DESCRIPTION-level token-driven field entity materialization, TypeSpec/generic layout materialization, ownership offset table emission, runtime layout construction, full token/cTypeId/layout cache, and metadata policy/code stripping completion.

## Baseline

- Before this slice, the metadata runtime could resolve type layouts by `typeLayoutId` and GC inline-frame consumers could use the attached registry.
- Reflection still populated type `layout.fieldCount/size/alignment` from `SZrObjectPrototype` serialized layout data or native fallback data.
- Script field reflection still wrote `offset/size/layout` from `SZrCompiledMemberInfo.member->fieldOffset/fieldSize`, so an attached AOT registry could not be the single source of truth for field offsets.

## Test Inventory

- `tests/module/test_metadata_runtime_type_layout.c`
  - `test_metadata_runtime_resolves_prototype_layout_from_attached_registry_context`
  - `test_metadata_runtime_prototype_layout_resolver_does_not_fallback_to_prototype_cache`
  - `test_reflection_layout_source_consumes_metadata_runtime_registry`
- Regression set:
  - `zr_vm_metadata_runtime_type_layout_test`
  - `zr_vm_metadata_runtime_query_test`
  - `zr_vm_aot_c_type_layout_contracts_test`
  - `zr_vm_aot_c_source_contracts_test`

## RED

Added focused coverage requiring:

- A child function with a `prototypeContextFunction` pointing at an attached entry function resolves a prototype's layout through the entry function's code-registration registry.
- A detached function does not succeed only because a stale `prototypeFrameTypeLayouts[typeLayoutId]` cache exists.
- `reflection.c` includes the metadata runtime, calls the function+prototype registry resolver, applies type layouts through a dedicated helper, and applies field layouts through a dedicated helper.
- Reflection field lookup consumes registry fields by instance-field order instead of using the old serialized field offset as the lookup key.

First RED command:

```text
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_type_layout_test -j2 --verbose"
```

First RED result: link failed because `ZrCore_MetadataRuntime_ResolveFunctionPrototypeTypeLayout()` did not exist.

## GREEN

Completed implementation:

- `zr_vm_core/include/zr_vm_core/metadata_runtime.h`
  - Added `ZrCore_MetadataRuntime_ResolveFunctionPrototypeTypeLayout()`.
- `zr_vm_core/src/zr_vm_core/metadata_runtime.c`
  - Added prototype-context lookup from function/prototype to `typeLayoutId`.
  - Resolved the resulting id through the attached code-registration layout registry.
  - Refused prototype layout cache fallback inside the new resolver.
- `zr_vm_core/src/zr_vm_core/reflection.c`
  - Included `metadata_runtime.h`.
  - Added `reflection_apply_type_layout_to_layout_object()` for type-level layout writes.
  - Added `reflection_apply_field_layout_to_member()` for member `offset/size/layout` writes.
  - Added instance-field-index lookup over `SZrTypeLayout.fields`.
  - Routed normal type reflection and decorator target member reflection through the registry-backed layout when an AOT registry is attached.
  - Preserved existing prototype/member/native fallback behavior when no registry layout is available.
- `tests/module/test_metadata_runtime_type_layout.c`
  - Extended the focused type-layout test from 7 to 10 tests.

Focused GREEN commands:

```text
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_type_layout_test -j2 && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_type_layout_test"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_runtime_type_layout_test -j2 && ./build-wsl-clang/bin/zr_vm_metadata_runtime_type_layout_test"
PowerShell: Import VS dev environment, then build/run build-msvc Debug zr_vm_metadata_runtime_type_layout_test
```

Focused GREEN results:

```text
WSL GCC Debug: zr_vm_metadata_runtime_type_layout_test 10/0
WSL Clang Debug: zr_vm_metadata_runtime_type_layout_test 10/0
Windows MSVC Debug: zr_vm_metadata_runtime_type_layout_test 10/0
```

## Tooling Evidence

- WSL GCC Debug build directory: `build-wsl-gcc`
- WSL Clang Debug build directory: `build-wsl-clang`
- Windows MSVC Debug build directory: `build-msvc`
- Windows MSVC environment: imported from `E:\Visual Studio\Common7\Tools\VsDevCmd.bat`; `cl.exe` from `E:\Visual Studio\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64`

## Results

WSL GCC Debug results:

```text
zr_vm_metadata_runtime_type_layout_test 10/0
zr_vm_metadata_runtime_query_test 20/0
zr_vm_aot_c_type_layout_contracts_test 1/0
zr_vm_aot_c_source_contracts_test 19/0
```

WSL Clang Debug results:

```text
zr_vm_metadata_runtime_type_layout_test 10/0
zr_vm_metadata_runtime_query_test 20/0
zr_vm_aot_c_type_layout_contracts_test 1/0
zr_vm_aot_c_source_contracts_test 19/0
```

Windows MSVC Debug results:

```text
zr_vm_metadata_runtime_type_layout_test 10/0
zr_vm_metadata_runtime_query_test 20/0
zr_vm_aot_c_type_layout_contracts_test 1/0
zr_vm_aot_c_source_contracts_test 19/0
```

Observed validation notes:

- An earlier combined WSL gcc/clang loop that also included `zr_vm_value_type_runtime_test` exceeded the tool timeout while building that unrelated target and was terminated; it is not used as pass evidence.
- Existing compiler warnings remain in generated dispatch/object/reflection/runtime-decorator paths; none failed the focused validation set.

## Acceptance Decision

11-S4H / 10-S4A passes. Reflection now uses the attached metadata runtime code-registration layout registry for type-level layout and script field offset/size layout reflection when a registry layout is available, and it preserves existing non-AOT fallback behavior.

The full 11-S4 stage remains open for TypeSpec/generic layout materialization, ownership offset emission, runtime layout construction, and complete token/cTypeId/layout caching. The full 10-S4 stage remains open for generic parameter reflection, token-driven field entity materialization, complete type-argument reflection, and metadata policy/code stripping integration.
