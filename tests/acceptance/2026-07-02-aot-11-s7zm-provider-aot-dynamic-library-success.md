# AOT 11-S7ZM / 12-S7 Provider AOT Dynamic-Library Success

## Scope
- Added a focused parser/AOT smoke fixture for standalone `.zrp` provider imports.
- Layers covered: project manifest provider resolution, AOT C code generation, generated shared-library build, strict AOT runtime module import, canonical module cache identity, and exported value publication.
- No production runtime logic changed in this slice; the test verifies that the preceding provider load-request runtime work succeeds for a real generated provider dynamic library.

## Baseline
- Before this slice, coverage stopped at missing-provider diagnostics: `tests/library/test_project_import_aot_provider_runtime.c` proved the runtime asked for `/deps/math/bin/aot_c/lib/zrvm_aot_ops_sum.*`, but no fixture built that library and imported it successfully.
- First build attempt against the stale WSL GCC build tree returned `No rule to make target 'zr_vm_aot_c_provider_shared_library_smoke_test'`; after CMake reconfiguration, the new target built and ran.
- Known residual baseline from the previous provider slice remains unchanged: exploratory WSL GCC `zr_vm_project_import_canonicalization_test` was 35/1, failing at `test_project_compile_applies_dependency_import_version_range_to_assembly_ref` with `assemblyRef` null.

## Test Inventory
- Focused success path: `tests/parser/test_aot_c_provider_shared_library_smoke.c`.
- The fixture writes a root `.zrp` referencing `deps/math/math.zrp`, writes the provider `.zrp`, compiles provider `ops/sum.zr` to `.zro`, emits AOT C with `moduleName = "ops/sum"`, compiles `zrvm_aot_ops_sum.so`, and imports `$mathLocal@2.1.0/ops/sum` under strict AOT C.
- Boundary assertions:
  - provider dynamic-library path is under `/deps/math/bin/aot_c/lib/zrvm_aot_ops_sum.so`;
  - runtime descriptor/module validation accepts provider-local `ops/sum`;
  - module cache remains keyed by canonical `$mathLocal@2.1.0/ops/sum`;
  - executed-via state is `ZR_LIBRARY_EXECUTED_VIA_AOT_C`;
  - exported `seed` value is published as integer `37`;
  - Windows/MSVC compiles the test and records the Unix-only dynamic-library execution branch as ignored.

## Tooling Evidence
- WSL GCC:
  - `cmake -S . -B build-wsl-gcc && cmake --build build-wsl-gcc --target zr_vm_aot_c_provider_shared_library_smoke_test -j 2 && ./build-wsl-gcc/bin/zr_vm_aot_c_provider_shared_library_smoke_test`
  - Result: `1 Tests 0 Failures 0 Ignored`.
- WSL clang:
  - `cmake -S . -B build-wsl-clang && cmake --build build-wsl-clang --target zr_vm_aot_c_provider_shared_library_smoke_test -j 2 && ./build-wsl-clang/bin/zr_vm_aot_c_provider_shared_library_smoke_test`
  - Result: `1 Tests 0 Failures 0 Ignored`.
- Windows MSVC Debug:
  - `cmake -S . -B build-msvc; cmake --build build-msvc --target zr_vm_aot_c_provider_shared_library_smoke_test --config Debug -j 2; .\build-msvc\bin\Debug\zr_vm_aot_c_provider_shared_library_smoke_test.exe`
  - Result: `1 Tests 0 Failures 1 Ignored`.

## Results
- Added `tests/parser/test_aot_c_provider_shared_library_smoke.c`.
- Registered `zr_vm_aot_c_provider_shared_library_smoke_test` in `tests/CMakeLists.txt`.
- WSL GCC and WSL clang validate the generated provider `.so` can be loaded and executed through strict AOT provider import.
- Windows MSVC Debug validates the target compiles and the Unix-only dynamic loader branch is explicitly ignored.

## Acceptance Decision
- Accepted as a provider dynamic-library success fixture for 11-S7ZM / 12-S7 support.
- This closes the success-fixture gap after 11-S7ZL; it does not close multi-version provider selection, export metadata attach, full metadata sweep/pruning, full trim analyzer, or broader ABI drift/deopt coverage.
