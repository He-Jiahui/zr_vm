# 2026-06-25 AOT 11-S4F GC Descriptor Runtime Resolver

## Scope

- Plan source: `docs/plans/aot/11-metadata.md` §4 token↔cTypeId↔ZrLayout table and `docs/plans/aot/09-memory-management.md` §1 GC descriptor.
- Slice target: expose a metadata-runtime API for code-registration GC descriptor lookup and require that descriptor lookup reuse the same runtime type-layout resolver introduced in 11-S4D.
- Non-goals: TypeSpec/generic layout materialization, runtime layout construction, ownership offset emission, reflection consumer migration, GC inline-frame scanning migration, optional local-address roots, and full token/cTypeId/layout cache.

## Baseline

- Before this slice, `ZrCore_MetadataRuntime_ResolveTypeLayout()` provided a public code-registration layout registry lookup, and 11-S4E had connected generic dictionary TYPE_LAYOUT/SIZEOF slots to it.
- GC descriptors were already emitted and published through `SZrAotCodeRegistration.gcDescriptors`, but there was no public metadata-runtime resolver that proved descriptor lookup could not drift away from the layout registry.
- Existing AOT root-frame scanning continues to use `SZrAotGcRootMap` frame-byte-offset roots. This slice does not change that scanning path.

## Test Inventory

- `tests/module/test_metadata_runtime_type_layout.c`
  - `test_metadata_runtime_resolves_gc_descriptor_from_code_registration_registry`
  - `test_metadata_runtime_gc_descriptor_does_not_fallback_to_prototype_cache`
- Regression set:
  - `zr_vm_metadata_runtime_type_layout_test`
  - `zr_vm_metadata_runtime_query_test`
  - `zr_vm_aot_gc_root_frame_test`
  - `zr_vm_aot_c_frame_setup_contracts_test`
  - `zr_vm_aot_c_source_contracts_test`
  - `zr_vm_aot_c_value_type_shared_library_smoke_test`
  - `zr_vm_aot_c_shared_library_smoke_test`
  - `zr_vm_aot_c_descriptor_diagnostics_test`
  - `zr_vm_aot_c_generic_reference_sharing_test`

## RED

Added focused coverage requiring:

- A valid descriptor at `gcDescriptors[42]` returns only when `descriptor->typeLayoutId == 42`.
- The same `typeLayoutId` must also resolve through `ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, 42)`.
- Null runtime, `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`, out-of-range ids, sparse descriptor slots, descriptors without layouts, and mismatched descriptor ids all return null.
- A stale `metadataFunction->prototypeFrameTypeLayouts[7]` does not allow descriptor lookup to succeed when code-registration `typeLayouts[7]` is absent.

RED command:

```text
wsl bash -lc 'cmake --build build/codex-wsl-gcc-debug --target zr_vm_metadata_runtime_type_layout_test -j2 && ./build/codex-wsl-gcc-debug/bin/zr_vm_metadata_runtime_type_layout_test'
```

RED result: compile/link failed because `ZrCore_MetadataRuntime_ResolveGcDescriptor()` did not exist. This proved the public metadata-runtime GC descriptor resolver was missing.

## GREEN

Completed implementation:

- `zr_vm_core/include/zr_vm_core/metadata_runtime.h`
  - Added `ZrCore_MetadataRuntime_ResolveGcDescriptor(SZrMetadataRuntime *runtime, TZrUInt32 typeLayoutId)`.
- `zr_vm_core/src/zr_vm_core/metadata_runtime.c`
  - Added bounded lookup through `runtime->codeRegistration->gcDescriptors[typeLayoutId]`.
  - Rejected null runtime, missing code registration, missing descriptor table, `NONE`, and out-of-range ids.
  - Required `descriptor->typeLayoutId == typeLayoutId`.
  - Required `ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, typeLayoutId)` to succeed before returning the descriptor.
- `tests/module/test_metadata_runtime_type_layout.c`
  - Extended the focused resolver test target from 3 tests to 5 tests.
  - Locked both positive registry-backed lookup and no-prototype-fallback behavior.

Focused GREEN command:

```text
wsl bash -lc 'cmake --build build/codex-wsl-gcc-debug --target zr_vm_metadata_runtime_type_layout_test -j2 && ./build/codex-wsl-gcc-debug/bin/zr_vm_metadata_runtime_type_layout_test'
```

Focused GREEN result: `zr_vm_metadata_runtime_type_layout_test` passed 5/0.

## Tooling Evidence

- WSL GCC: `gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0`
- WSL Clang: `Ubuntu clang version 14.0.0-1ubuntu1.1`
- WSL CMake: `cmake version 3.22.1`
- Windows MSVC: `VSCMD_VER=17.14.34`, `cl.exe` from `E:\Visual Studio\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64`
- Windows MSBuild: `.NET Framework MSBuild 17.14.40+3e7442088`

## Results

WSL GCC Debug command shape:

```text
for each target:
  cmake --build build/codex-wsl-gcc-debug --target <target> -j2
  build/codex-wsl-gcc-debug/bin/<target>
```

WSL GCC Debug results:

```text
zr_vm_metadata_runtime_type_layout_test 5/0
zr_vm_metadata_runtime_query_test 20/0
zr_vm_aot_gc_root_frame_test 5/0
zr_vm_aot_c_frame_setup_contracts_test 1/0
zr_vm_aot_c_source_contracts_test 19/0
zr_vm_aot_c_value_type_shared_library_smoke_test 2/0
zr_vm_aot_c_shared_library_smoke_test 8/0
zr_vm_aot_c_descriptor_diagnostics_test 2/0
zr_vm_aot_c_generic_reference_sharing_test 4/0
```

WSL Clang Debug command shape:

```text
for each target:
  cmake --build build/codex-wsl-clang-debug --target <target> -j2
  build/codex-wsl-clang-debug/bin/<target>
```

WSL Clang Debug results:

```text
zr_vm_metadata_runtime_type_layout_test 5/0
zr_vm_metadata_runtime_query_test 20/0
zr_vm_aot_gc_root_frame_test 5/0
zr_vm_aot_c_frame_setup_contracts_test 1/0
zr_vm_aot_c_source_contracts_test 19/0
zr_vm_aot_c_value_type_shared_library_smoke_test 2/0
zr_vm_aot_c_shared_library_smoke_test 8/0
zr_vm_aot_c_descriptor_diagnostics_test 2/0
zr_vm_aot_c_generic_reference_sharing_test 4/0
```

Windows MSVC Debug command shape:

```text
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
for each target:
  cmake --build build\codex-msvc-debug --config Debug --target <target> -j 2
  build\codex-msvc-debug\bin\Debug\<target>.exe
```

Windows MSVC Debug results:

```text
zr_vm_metadata_runtime_type_layout_test 5/0
zr_vm_metadata_runtime_query_test 20/0
zr_vm_aot_gc_root_frame_test 5/0
zr_vm_aot_c_frame_setup_contracts_test 1/0
zr_vm_aot_c_source_contracts_test 19/0
zr_vm_aot_c_value_type_shared_library_smoke_test 2/0, 1 ignored Unix-only branch
zr_vm_aot_c_shared_library_smoke_test 8/0, 8 ignored Unix-only branches
zr_vm_aot_c_descriptor_diagnostics_test 2/0, 2 ignored Unix-only branches
zr_vm_aot_c_generic_reference_sharing_test 4/0
```

Observed existing warnings:

- WSL Clang still reports existing generated-C logical-not parentheses warnings in shared-library smoke output.
- WSL GCC/Clang and MSVC still report existing unrelated unused, const qualifier, missing initializer, and unreachable-code warnings.
- No warning was introduced that failed the focused validation set.

## Acceptance Decision

11-S4F passes. GC descriptor lookup now has a public metadata-runtime resolver, and descriptor exposure is gated by the same code-registration layout registry used by TypeDef binding and generic dictionary TYPE_LAYOUT/SIZEOF lookup.

The full 11-S4 stage remains open for TypeSpec/generic layout materialization, runtime layout construction, ownership offsets, reflection consumer migration, GC inline-frame scanning migration, and complete token/cTypeId/layout caching.
