# AOT 11-S4N · cTypeId to token resolver

## Scope

11-S4N adds a public runtime resolver for the cTypeId -> TypeDef/TypeSpec token edge of the 11-S4 token/cTypeId/layout mapping.

Affected code:
- `zr_vm_core/include/zr_vm_core/metadata_runtime.h`
- `zr_vm_core/src/zr_vm_core/metadata_runtime_layout_binding.c`
- `tests/module/test_metadata_runtime_typespec_layout.c`
- `docs/plans/aot/11-metadata.md`
- `docs/plans/aot/10-reflection.md`
- `docs/plans/aot/index.md`

## Baseline

11-S4L exposed `ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, typeLayoutId)` and 11-S4M made its cache multi-entry. There was still no public API that names the generated-C `cTypeId` side of the three-way table directly.

The current registry representation keeps `cTypeId == typeLayoutId`; this slice intentionally uses that invariant and does not claim a persistent cTypeId index or future cTypeId/typeLayoutId decoupling.

## RED

Added focused tests:
- `test_metadata_runtime_resolves_ctype_id_tokens_with_multi_entry_cache`
- `test_metadata_runtime_ctype_id_token_does_not_fallback_to_prototype_cache`

RED command:

```sh
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_typespec_layout_test -j2 && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_typespec_layout_test"
```

RED result: build failed with an implicit declaration and undefined references for `ZrCore_MetadataRuntime_ResolveCTypeIdToken`.

## GREEN

Implementation:
- Added `ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, cTypeId)` to `metadata_runtime.h`.
- Implemented it in `metadata_runtime_layout_binding.c` by reusing `ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, cTypeId)` under the current `cTypeId == typeLayoutId` invariant.
- The new API inherits bounded multi-entry cache behavior and no-prototype-fallback semantics from the existing reverse resolver.

Focused GREEN:

```sh
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_typespec_layout_test -j2 && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_typespec_layout_test"
```

Result: `12 Tests 0 Failures 0 Ignored`.

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
- `zr_vm_metadata_runtime_typespec_layout_test`: `12 Tests 0 Failures 0 Ignored`
- `zr_vm_zrp_metadata_format_test`: `11 Tests 0 Failures 0 Ignored`
- `zr_vm_metadata_runtime_query_test`: `22 Tests 0 Failures 0 Ignored`
- `zr_vm_metadata_runtime_type_layout_test`: `10 Tests 0 Failures 0 Ignored`

Existing generated-dispatch warnings, MSVC unreachable-code warnings, and the `metadata_runtime.c` possible-uninitialized warning remain non-fatal.

## Acceptance Decision

Accepted for 11-S4N. The runtime now has a public cTypeId -> TypeDef/TypeSpec token resolver that shares the existing registry-backed reverse path and bounded cache. Full 11-S4 remains open for a persistent cTypeId-to-token index, cTypeId/typeLayoutId decoupling if needed, TypeSpec/generic layout materialization, ownership offset emission, and runtime layout construction.
