# 12-S7ZZQ / 11-S7 Runtime Export Member Token Publication

## Scope
- Change: AOT-loaded module metadata runtime now mirrors `memberTokenRemapCount` from `SZrAotCodeRegistration` and applies retained `sourceToken -> targetToken` member-token remaps to the loaded entry function's `typedExportedSymbols`.
- Affected layers: core module metadata runtime attach, typed module export metadata, AOT generated member-token remap ABI consumption, cross-module import signature provider token visibility.

## Baseline
- Before this slice, generated C already published `memberTokenRemaps/memberTokenRemapCount` and runtime descriptor validation checked table shape and duplicates.
- `ZrCore_Module_AttachMetadataRuntime()` did not mirror `memberTokenRemapCount`, and the loaded provider entry function still exposed source `MEMBER_DEF` tokens in `typedExportedSymbols`.
- RED evidence: after adding `test_metadata_runtime_rewrites_typed_export_member_tokens_from_registration_remap`, WSL GCC build failed because `SZrMetadataRuntime` had no `memberTokenRemapCount` member.

## Test Inventory
- Unit/focused subsystem:
  - `tests/module/test_metadata_runtime_query.c`
    - `test_metadata_runtime_rewrites_typed_export_member_tokens_from_registration_remap`
    - Checks remapped source `MEMBER_DEF` token, retained already-compacted `MEMBER_DEF` token, and non-member token preservation.
- Adjacent module/import tests:
  - `metadata_type_ref_binding`
  - `metadata_runtime_binding_compatibility`
- AOT generated metadata/remap tests:
  - `aot_c_code_stripping`
  - `aot_c_zrp_metadata_export_token_remap`
  - `aot_c_descriptor_diagnostics`
- Boundary and failure cases:
  - Null/no export list: attach helper returns without mutation.
  - Non-`MEMBER_DEF` token: preserved.
  - Missing remap table/count: original token preserved.
  - Invalid target token shape: ignored by the write-back helper; descriptor validation already rejects invalid published entries before AOT execution.

## Tooling Evidence
- WSL tools:
  - `cmake version 3.22.1`
  - `ctest version 3.22.1`
  - `gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0`
  - `Ubuntu clang version 14.0.0-1ubuntu1.1`
- Windows tools:
  - `cmake version 3.23.0-rc2`
  - `ctest version 3.23.0-rc2`
  - MSVC Debug build output reported MSBuild `17.14.40+3e7442088`.

Commands:

```powershell
wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_query_test -j2
wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test
wsl.exe --cd /mnt/e/Git/zr_vm ctest --test-dir build-wsl-gcc -R "metadata_runtime_query" --output-on-failure
wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-clang --target zr_vm_metadata_runtime_query_test -j2
wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test
wsl.exe --cd /mnt/e/Git/zr_vm ctest --test-dir build-wsl-clang -R "metadata_runtime_query" --output-on-failure
cmake --build build-msvc --target zr_vm_metadata_runtime_query_test --config Debug --parallel 2
.\build-msvc\bin\Debug\zr_vm_metadata_runtime_query_test.exe
ctest --test-dir build-msvc -C Debug -R "metadata_runtime_query" --output-on-failure
wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_binding_compatibility_test -j2
wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-clang --target zr_vm_metadata_runtime_binding_compatibility_test -j2
wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'ctest --test-dir build-wsl-gcc -R "(metadata_runtime_query|metadata_runtime_binding_compatibility|metadata_type_ref_binding|project_import_canonicalization)" --output-on-failure'
wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'ctest --test-dir build-wsl-clang -R "(metadata_runtime_query|metadata_runtime_binding_compatibility|metadata_type_ref_binding|project_import_canonicalization)" --output-on-failure'
ctest --test-dir build-msvc -C Debug -R "(metadata_runtime_query|metadata_runtime_binding_compatibility|metadata_type_ref_binding|project_import_canonicalization)" --output-on-failure
wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'ctest --test-dir build-wsl-gcc -R "(aot_c_code_stripping|aot_c_zrp_metadata_export_token_remap|aot_c_descriptor_diagnostics)" --output-on-failure'
wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'ctest --test-dir build-wsl-clang -R "(aot_c_code_stripping|aot_c_zrp_metadata_export_token_remap|aot_c_descriptor_diagnostics)" --output-on-failure'
ctest --test-dir build-msvc -C Debug -R "(aot_c_code_stripping|aot_c_zrp_metadata_export_token_remap|aot_c_descriptor_diagnostics)" --output-on-failure
```

## Results
- RED:
  - WSL GCC `zr_vm_metadata_runtime_query_test` build failed at the new assertion because `SZrMetadataRuntime` lacked `memberTokenRemapCount`.
- GREEN:
  - WSL GCC direct `zr_vm_metadata_runtime_query_test`: 25 tests, 0 failures.
  - WSL GCC CTest `metadata_runtime_query`: 1/1 passed.
  - WSL clang direct `zr_vm_metadata_runtime_query_test`: 25 tests, 0 failures.
  - WSL clang CTest `metadata_runtime_query`: 1/1 passed.
  - Windows MSVC Debug direct `zr_vm_metadata_runtime_query_test`: 25 tests, 0 failures.
  - Windows MSVC Debug CTest `metadata_runtime_query`: 1/1 passed.
  - WSL GCC CTest `metadata_type_ref_binding|metadata_runtime_query|metadata_runtime_binding_compatibility`: 3/3 passed.
  - WSL clang CTest `metadata_type_ref_binding|metadata_runtime_query|metadata_runtime_binding_compatibility`: 3/3 passed.
  - Windows MSVC Debug CTest same metadata set: 3/3 passed.
  - WSL GCC CTest `aot_c_code_stripping|aot_c_zrp_metadata_export_token_remap|aot_c_descriptor_diagnostics`: 3/3 passed.
  - WSL clang CTest same AOT set: 3/3 passed.
  - Windows MSVC Debug CTest same AOT set: 3/3 passed.
- Non-regression note:
  - The attempted `project_import_canonicalization` selector did not match a registered CTest name in the current build output; the relevant metadata/import registered tests above were used.
  - The first WSL metadata binding CTest attempt failed because the executable had not been built yet; after building `zr_vm_metadata_runtime_binding_compatibility_test`, the CTest set passed on both WSL toolchains.

## Acceptance Decision
- Accepted for this slice.
- The runtime now publishes compacted retained member tokens to the provider entry function table consumed by existing cross-module import signature matching.
- Remaining risks:
  - This does not create a persistent external export manifest/table writer.
  - Cross-module provider loading/version binding beyond the existing import signature path remains incomplete.
  - Full metadata sweep/pruning, annotation policy, full trim analyzer, and broader ABI drift/deopt closure remain open.
