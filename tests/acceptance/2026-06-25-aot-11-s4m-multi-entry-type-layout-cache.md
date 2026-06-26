# AOT 11-S4M · bounded multi-entry type layout cache

## Scope

11-S4M replaces the 11-S4K/11-S4L latest-hit TypeDef/TypeSpec token-layout cache with a bounded 8-entry runtime cache in `SZrMetadataRuntime`.

Affected code:
- `zr_vm_core/include/zr_vm_core/metadata_runtime.h`
- `zr_vm_core/src/zr_vm_core/metadata_runtime_layout_binding.c`
- `tests/module/test_metadata_runtime_typespec_layout.c`
- `docs/plans/aot/11-metadata.md`
- `docs/plans/aot/10-reflection.md`
- `docs/plans/aot/index.md`

## Baseline

Before this slice, `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` and `ZrCore_MetadataRuntime_ResolveTypeLayoutToken()` shared one latest-hit cache slot. A TypeDef lookup followed by a TypeSpec lookup replaced the TypeDef entry; the same overwrite happened for reverse layout-id lookups.

## RED

Added two focused tests:
- `test_metadata_runtime_type_layout_cache_keeps_multiple_token_entries`
- `test_metadata_runtime_type_layout_cache_keeps_multiple_reverse_entries`

RED command:

```sh
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_typespec_layout_test -j2 && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_typespec_layout_test"
```

RED result: `zr_vm_metadata_runtime_typespec_layout_test` ran 10 tests with 2 failures. The old single-slot cache lost the TypeDef token/layout entry after resolving the TypeSpec entry, and the reverse cache returned `0` after the second layout-id lookup replaced the first.

## GREEN

Implementation:
- `SZrMetadataRuntime` now carries `typeLayoutCacheTokens[]`, `typeLayoutCacheIds[]`, `typeLayoutCacheLayouts[]`, and `typeLayoutCacheNextIndex`.
- Cache capacity is `ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY == 8`.
- Forward token lookup probes by TypeDef/TypeSpec token before reading binding views.
- Reverse lookup probes by layout id before scanning zrp TypeDef/TypeSpec rows.
- Successful binding-view results are stored back into the bounded cache, updating existing tokens, filling empty slots, then round-robin replacing entries when full.

Focused GREEN:

```sh
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_typespec_layout_test -j2 && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_typespec_layout_test"
```

Result: `10 Tests 0 Failures 0 Ignored`.

## Validation

WSL GCC:

```sh
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_typespec_layout_test zr_vm_zrp_metadata_format_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_type_layout_test -j2"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R '^(metadata_runtime_typespec_layout|metadata_runtime_query|metadata_runtime_type_layout|zrp_metadata_format)$' --output-on-failure"
```

Result: CTest `4/4` passed.

WSL clang:

```sh
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_runtime_typespec_layout_test zr_vm_zrp_metadata_format_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_type_layout_test -j2"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-clang -R '^(metadata_runtime_typespec_layout|metadata_runtime_query|metadata_runtime_type_layout|zrp_metadata_format)$' --output-on-failure"
```

Result: CTest `4/4` passed.

Windows MSVC Debug:

```powershell
cmake --build build-msvc --config Debug --target zr_vm_metadata_runtime_typespec_layout_test zr_vm_zrp_metadata_format_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_type_layout_test --parallel 2
.\build-msvc\bin\Debug\zr_vm_metadata_runtime_typespec_layout_test.exe
.\build-msvc\bin\Debug\zr_vm_zrp_metadata_format_test.exe
.\build-msvc\bin\Debug\zr_vm_metadata_runtime_query_test.exe
.\build-msvc\bin\Debug\zr_vm_metadata_runtime_type_layout_test.exe
```

Result:
- `zr_vm_metadata_runtime_typespec_layout_test`: `10 Tests 0 Failures 0 Ignored`
- `zr_vm_zrp_metadata_format_test`: `11 Tests 0 Failures 0 Ignored`
- `zr_vm_metadata_runtime_query_test`: `22 Tests 0 Failures 0 Ignored`
- `zr_vm_metadata_runtime_type_layout_test`: `10 Tests 0 Failures 0 Ignored`

Existing MSVC generated-dispatch/unreachable-code and `metadata_runtime.c` possible-uninitialized warnings remain non-fatal.

## Acceptance Decision

Accepted for 11-S4M. The bounded runtime cache now retains multiple TypeDef/TypeSpec token/layout bindings in both directions. Full 11-S4 remains open for persistent cTypeId-to-token indexing, TypeSpec/generic layout materialization, ownership offset emission, and runtime layout construction.
