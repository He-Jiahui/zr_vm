# 2026-06-25 AOT 11-S4G GC Inline-Frame Runtime Layout Resolver

## Scope

- Plan source: `docs/plans/aot/11-metadata.md` §4 token↔cTypeId↔ZrLayout table and `docs/plans/aot/09-memory-management.md` GC inline-frame scanning.
- Slice target: migrate GC inline-frame mark/rewrite layout lookup for AOT-loaded functions to the metadata runtime code-registration layout table, while preserving prototype layout fallback for non-AOT VM/interpreter frames.
- Non-goals: TypeSpec/generic layout materialization, runtime layout construction, ownership offset emission, reflection consumer migration, optional local-address roots, and full token/cTypeId/layout cache.

## Baseline

- Before this slice, generic dictionary TYPE_LAYOUT/SIZEOF and GC descriptor lookup already used metadata-runtime code-registration layout resolution.
- GC mark/rewrite inline-frame traversal still called `ZrCore_Function_ResolvePrototypeFrameTypeLayout()` directly, so an AOT function with a stale prototype layout cache could bypass the code-registration layout registry.
- Ordinary VM/interpreter inline-frame GC traversal also relied on `ZrCore_Function_ResolvePrototypeFrameTypeLayout()`, so that path must remain available when no AOT registry is attached.

## Test Inventory

- `tests/module/test_metadata_runtime_type_layout.c`
  - `test_metadata_runtime_function_layout_resolver_uses_attached_registry_context`
  - `test_metadata_runtime_function_layout_resolver_does_not_fallback_to_prototype_cache`
- `tests/gc/gc_tests.c`
  - `test_gc_minor_collection_rewrites_inline_frame_value_with_layout_visitor`
- Regression set:
  - `zr_vm_metadata_runtime_type_layout_test`
  - `zr_vm_metadata_runtime_query_test`
  - `zr_vm_aot_gc_root_frame_test`
  - `zr_vm_gc_test`
  - `zr_vm_value_type_runtime_test`
  - `zr_vm_aot_c_frame_setup_contracts_test`
  - `zr_vm_aot_c_source_contracts_test`
  - `zr_vm_aot_c_value_type_shared_library_smoke_test`
  - `zr_vm_aot_c_shared_library_smoke_test`
  - `zr_vm_aot_c_descriptor_diagnostics_test`
  - `zr_vm_aot_c_generic_reference_sharing_test`

## RED

Added focused coverage requiring:

- A child function whose `prototypeContextFunction` points to an attached entry function resolves `typeLayoutId` through the entry function's code-registration layout registry.
- A stale `prototypeFrameTypeLayouts[typeLayoutId]` entry with a different byte size does not win over the code-registration registry.
- Detached functions return null from `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout()` even when prototype layout cache data exists.

First RED command:

```text
wsl bash -lc 'cmake --build build/codex-wsl-gcc-debug --target zr_vm_metadata_runtime_type_layout_test -j2 && ./build/codex-wsl-gcc-debug/bin/zr_vm_metadata_runtime_type_layout_test'
```

First RED result: compile/link failed because `ZrCore_MetadataRuntime_AttachFunction()` and `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout()` did not exist.

Second RED command:

```text
wsl bash -lc "cmake --build build/codex-wsl-gcc-debug --target zr_vm_gc_test -j2 && ./build/codex-wsl-gcc-debug/bin/zr_vm_gc_test"
```

Second RED result: `test_gc_minor_collection_rewrites_inline_frame_value_with_layout_visitor` failed with expected work `2` and actual work `1`. This proved the first GC adapter migration had removed the required non-AOT prototype fallback.

## GREEN

Completed implementation:

- `zr_vm_core/include/zr_vm_core/function.h`
  - Added a stable attached `SZrAotCodeRegistration` pointer plus layout/GC descriptor counts to `SZrFunction`.
- `zr_vm_core/src/zr_vm_core/function.c`
  - Initialized and reset attached registry fields so reused/tombstoned functions cannot retain a stale AOT registry.
- `zr_vm_core/include/zr_vm_core/metadata_runtime.h`
  - Added `ZrCore_MetadataRuntime_AttachFunction()`.
  - Added `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout()`.
- `zr_vm_core/src/zr_vm_core/metadata_runtime.c`
  - Attached function-level registry fields from `SZrMetadataRuntime`.
  - Resolved function layout ids through the function registry or prototype-context entry function registry.
  - Reused `ZrCore_MetadataRuntime_ResolveTypeLayout()` and did not fall back to prototype layout cache inside the public function resolver.
- `zr_vm_core/src/zr_vm_core/module/module.c`
  - Attached the module metadata runtime to the module metadata function.
- `zr_vm_library/src/zr_vm_library/aot_runtime.c`
  - Attached the module metadata runtime to every loaded runtime function in the function table.
- `zr_vm_core/src/zr_vm_core/gc/gc_mark.c`
  - Routed inline-frame mark layout lookup through the metadata runtime resolver when an AOT registry is attached.
  - Preserved prototype resolver fallback only when no AOT registry is attached.
- `zr_vm_core/src/zr_vm_core/gc/gc_cycle.c`
  - Applied the same resolver policy to minor-GC rewrite traversal.
- `tests/module/test_metadata_runtime_type_layout.c`
  - Extended focused type-layout coverage from 5 tests to 7 tests.

Focused GREEN commands:

```text
wsl bash -lc "cmake --build build/codex-wsl-gcc-debug --target zr_vm_metadata_runtime_type_layout_test -j2 && ./build/codex-wsl-gcc-debug/bin/zr_vm_metadata_runtime_type_layout_test"
wsl bash -lc "cmake --build build/codex-wsl-gcc-debug --target zr_vm_gc_test -j2 && ./build/codex-wsl-gcc-debug/bin/zr_vm_gc_test"
```

Focused GREEN results:

```text
zr_vm_metadata_runtime_type_layout_test 7/0
zr_vm_gc_test 66/0
```

## Tooling Evidence

- WSL GCC Debug build directory: `build/codex-wsl-gcc-debug`
- WSL Clang Debug build directory: `build/codex-wsl-clang-debug`
- Windows MSVC Debug build directory: `build\codex-msvc-debug`
- Windows MSVC environment: `VSCMD_VER=17.14.34`, `cl.exe` from `E:\Visual Studio\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64`

## Results

WSL GCC Debug command shape:

```text
for each target:
  cmake --build build/codex-wsl-gcc-debug --target <target> -j2
  build/codex-wsl-gcc-debug/bin/<target>
```

WSL GCC Debug results:

```text
zr_vm_metadata_runtime_type_layout_test 7/0
zr_vm_metadata_runtime_query_test 20/0
zr_vm_aot_gc_root_frame_test 5/0
zr_vm_gc_test 66/0
zr_vm_value_type_runtime_test 14/0
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
zr_vm_metadata_runtime_type_layout_test 7/0
zr_vm_metadata_runtime_query_test 20/0
zr_vm_aot_gc_root_frame_test 5/0
zr_vm_gc_test 66/0
zr_vm_value_type_runtime_test 14/0
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
zr_vm_metadata_runtime_type_layout_test 7/0
zr_vm_metadata_runtime_query_test 20/0
zr_vm_aot_gc_root_frame_test 5/0
zr_vm_gc_test 66/0
zr_vm_value_type_runtime_test 14/0
zr_vm_aot_c_frame_setup_contracts_test 1/0
zr_vm_aot_c_source_contracts_test 19/0
zr_vm_aot_c_value_type_shared_library_smoke_test 2/0, 1 ignored Unix-only branch
zr_vm_aot_c_shared_library_smoke_test 8/0, 8 ignored Unix-only branches
zr_vm_aot_c_descriptor_diagnostics_test 2/0, 2 ignored Unix-only branches
zr_vm_aot_c_generic_reference_sharing_test 4/0
```

Observed existing warnings:

- WSL GCC/Clang and MSVC still report unrelated existing warnings in generated C, debug/reflection/runtime-decorator code, const qualification, or unreachable-code paths.
- No warning was introduced that failed the focused validation set.

## Acceptance Decision

11-S4G passes. GC inline-frame mark/rewrite now uses the metadata runtime layout registry for attached AOT functions, refuses stale prototype fallback under an attached registry, and preserves the original prototype resolver for non-AOT VM/interpreter inline-frame GC traversal.

The full 11-S4 stage remains open for TypeSpec/generic layout materialization, ownership offset emission, runtime layout construction, reflection consumer migration, and complete token/cTypeId/layout caching.
