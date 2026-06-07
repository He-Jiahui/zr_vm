# AOT Meta Value-Access Boundary

## Scope

- Generated AOT C `META_GET`, `META_SET`, `SUPER_META_GET_CACHED`, `SUPER_META_SET_CACHED`, `SUPER_META_GET_STATIC_CACHED`, and `SUPER_META_SET_STATIC_CACHED` lowering.
- Affected layers: AOT C backend value lowering, function-body routing, source contracts, generated-C compile smoke.

## Baseline

- Before this change, generated C emitted `ZrLibrary_AotRuntime_MetaGet*` and `ZrLibrary_AotRuntime_MetaSet*` instruction-helper calls for meta value access.
- The RED contract failed on the missing `backend_aot_write_c_unsupported_meta_value_access` source contract.
- The repository already had unrelated dirty state; this record claims only the focused AOT validation below.

## Test Inventory

- `test_aot_c_source_makes_meta_value_access_explicit_boundary` in `tests/parser/test_aot_c_global_contracts.c`.
- `test_aot_c_generated_shared_library_compiles_meta_value_access_boundary` in `tests/parser/test_aot_c_global_shared_library_smoke.c`.
- Boundary coverage: all six meta value-access opcodes route to an explicit generated failure block, preserve opcode names, materialize primary/secondary stack operands, record member/cache index, report `unsupported AOT meta value access`, and fail through `ZR_AOT_C_FAIL()`.
- Negative coverage: `ZrLibrary_AotRuntime_MetaGet`, `MetaSet`, cached, and static cached helper calls are absent from the checked AOT C value/function-body lowering path.

## Tooling Evidence

- RED:
  - `wsl sh -lc 'cmake --build /mnt/e/Git/zr_vm/build-wsl-gcc --target zr_vm_aot_c_global_contracts_test zr_vm_aot_c_global_shared_library_smoke_test -j 4 && /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_aot_c_global_contracts_test && /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_aot_c_global_shared_library_smoke_test'`
  - Result: after fixing the test harness enum type, `test_aot_c_source_makes_meta_value_access_explicit_boundary` failed on missing `backend_aot_write_c_unsupported_meta_value_access(FILE *file,`.
- GCC:
  - `wsl sh -lc 'cmake --build /mnt/e/Git/zr_vm/build-wsl-gcc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_constant_contracts_test zr_vm_aot_c_global_contracts_test zr_vm_aot_c_global_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test -j 3 && /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_aot_c_constant_contracts_test && /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_aot_c_global_contracts_test && /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_aot_c_global_shared_library_smoke_test && /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test'`
- Clang:
  - `wsl sh -lc 'cmake --build /mnt/e/Git/zr_vm/build-wsl-clang --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_constant_contracts_test zr_vm_aot_c_global_contracts_test zr_vm_aot_c_global_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test -j 3 && /mnt/e/Git/zr_vm/build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && /mnt/e/Git/zr_vm/build-wsl-clang/bin/zr_vm_aot_c_constant_contracts_test && /mnt/e/Git/zr_vm/build-wsl-clang/bin/zr_vm_aot_c_global_contracts_test && /mnt/e/Git/zr_vm/build-wsl-clang/bin/zr_vm_aot_c_global_shared_library_smoke_test && /mnt/e/Git/zr_vm/build-wsl-clang/bin/zr_vm_aot_c_shared_library_smoke_test'`
- MSVC:
  - `. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; cmake --build build\codex-msvc-aot-constant-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_constant_contracts_test zr_vm_aot_c_global_contracts_test zr_vm_aot_c_global_shared_library_smoke_test --config Debug --parallel 4`
  - Executed `zr_vm_aot_c_source_contracts_test.exe`, `zr_vm_aot_c_constant_contracts_test.exe`, `zr_vm_aot_c_global_contracts_test.exe`, and `zr_vm_aot_c_global_shared_library_smoke_test.exe`.
- Source scan:
  - `Select-String -Path 'zr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot\backend_aot_c_function_body.c','zr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot\backend_aot_c_lowering_values.c' -Pattern 'ZrLibrary_AotRuntime_MetaGet|ZrLibrary_AotRuntime_MetaSet|ZrLibrary_AotRuntime_MetaGetCached|ZrLibrary_AotRuntime_MetaSetCached|ZrLibrary_AotRuntime_MetaGetStaticCached|ZrLibrary_AotRuntime_MetaSetStaticCached|zr_aot_value_unsupported_meta_value_access'`

## Results

- GCC: source contracts 17 tests, constant contracts 4 tests, global contracts 4 tests, global shared-library smoke 5 tests, shared-library smoke 6 tests; all 0 failures.
- Clang: source contracts 17 tests, constant contracts 4 tests, global contracts 4 tests, global shared-library smoke 5 tests, shared-library smoke 6 tests; all 0 failures.
- MSVC: source contracts 17 tests, constant contracts 4 tests, global contracts 4 tests, global shared-library smoke 5 tests; all 0 failures, with 5 Unix-only shared-library smoke tests ignored.
- Source scan found only the new `zr_aot_value_unsupported_meta_value_access` marker in `backend_aot_c_lowering_values.c`; the old meta value-access helper calls are absent from the checked AOT C function-body/value-lowering sources.

## Acceptance Decision

Accepted as an explicit generated AOT C dynamic/meta value-access boundary. This does not implement real generated meta value access or meta dispatch execution; those remain future contracts alongside LLVM parity.
