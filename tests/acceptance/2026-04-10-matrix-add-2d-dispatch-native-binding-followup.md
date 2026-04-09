# Matrix Add 2D Dispatch And Native Binding Follow-up

Date: 2026-04-10

## Scope

- dispatch fast-table/static fallback tightening in `zr_vm_core/src/zr_vm_core/execution_dispatch.c`
- native binding temp-root/direct-root follow-up in `zr_vm_library/src/zr_vm_library/native_binding_dispatch.c`
- dispatch stack/constant move fast-copy tightening in `zr_vm_core/src/zr_vm_core/execution_dispatch.c`
- super-array 4-way append/fill batch commit tightening in `zr_vm_core/src/zr_vm_core/object_super_array.c`

## WSL GCC Validation

Build directory:

- `/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo`

Build command:

```bash
cmake -S /mnt/e/Git/zr_vm -B ~/codex-builds/zr_vm-gcc-relwithdebinfo -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DBUILD_TESTS=ON -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build ~/codex-builds/zr_vm-gcc-relwithdebinfo --target zr_vm_cli_executable zr_vm_container_temp_value_root_test -j 8
```

Validation commands:

```bash
cd ~/codex-builds/zr_vm-gcc-relwithdebinfo/bin && LD_LIBRARY_PATH=../lib ./zr_vm_container_temp_value_root_test
cd /mnt/e/Git/zr_vm/tests/benchmarks/cases/matrix_add_2d/zr && LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib /usr/bin/time -f "ELAPSED %e" /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary
cd /mnt/e/Git/zr_vm/tests/benchmarks/cases/matrix_add_2d/zr && LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib valgrind --tool=callgrind --callgrind-out-file=/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.dispatch-superarray-next.out /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary
```

## Results

- `zr_vm_container_temp_value_root_test`: PASS
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.05s`
- callgrind total `Ir`: `44,595,814`

Key hotspots from `callgrind_annotate`:

- `execution_dispatch.c:ZrCore_Execute`: `33,643,404 Ir`
- `object_super_array_internal.h:ZrCore_Execute`: `2,691,072 Ir`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,107 Ir`
- `native_binding_temp_root_slot`: `68,123 Ir`
- `ZrLib_TempValueRoot_SetValue`: `54,226 Ir`

## Comparison To Immediate Preceding Iteration

Previous same-build follow-up snapshot:

- wall time: `0.04s`
- callgrind total `Ir`: `45,996,045`
- `execution_dispatch.c:ZrCore_Execute`: `34,999,833 Ir`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `947,081 Ir`
- `native_binding_temp_root_slot`: `68,123 Ir`
- `ZrLib_TempValueRoot_SetValue`: `54,226 Ir`

Observed delta:

- total `Ir`: `-1,400,231`
- `execution_dispatch.c:ZrCore_Execute`: `-1,356,429`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `-42,974`
- `native_binding_temp_root_slot`: unchanged at `68,123`
- `ZrLib_TempValueRoot_SetValue`: unchanged at `54,226`

## WSL Clang Validation

Build directory:

- `/home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo`

Validation commands:

```bash
cd ~/codex-builds/zr_vm-clang-relwithdebinfo/bin && LD_LIBRARY_PATH=../lib ./zr_vm_container_temp_value_root_test
cd /mnt/e/Git/zr_vm/tests/benchmarks/cases/matrix_add_2d/zr && LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo/lib /usr/bin/time -f "ELAPSED %e" /home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary
```

Results:

- `zr_vm_container_temp_value_root_test`: PASS
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.05s`

## Windows MSVC Smoke

Build directory:

- `E:\Git\zr_vm\build\codex-msvc-smoke-debug`

Validation command:

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-msvc-smoke-debug --target zr_vm_cli_executable --parallel 8
E:\Git\zr_vm\build\codex-msvc-smoke-debug\bin\zr_vm_cli.exe E:\Git\zr_vm\tests\fixtures\projects\hello_world\hello_world.zrp
```

Results:

- `zr_vm_cli` build: PASS
- `hello_world` smoke output: `hello world`

## Windows MSVC RelWithDebInfo Benchmark

Build directory:

- `E:\Git\zr_vm\build\codex-msvc-relwithdebinfo`

Validation command:

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake -S . -B build\codex-msvc-relwithdebinfo -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-msvc-relwithdebinfo --target zr_vm_cli_executable --parallel 8
E:\Git\zr_vm\build\codex-msvc-relwithdebinfo\bin\zr_vm_cli.exe E:\Git\zr_vm\tests\benchmarks\cases\matrix_add_2d\zr\benchmark_matrix_add_2d.zrp --execution-mode binary
```

Results:

- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.30s`

## Windows MSVC Release Benchmark

Build directory:

- `E:\Git\zr_vm\build\codex-msvc-release`

Validation command:

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake -S . -B build\codex-msvc-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-msvc-release --target zr_vm_cli_executable --parallel 8
E:\Git\zr_vm\build\codex-msvc-release\bin\zr_vm_cli.exe E:\Git\zr_vm\tests\benchmarks\cases\matrix_add_2d\zr\benchmark_matrix_add_2d.zrp --execution-mode binary
```

Results:

- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.29s`

## Known Remaining Issues

- `execution_dispatch.c:ZrCore_Execute` remains the dominant hotspot.
- `object_super_array_internal.h:ZrCore_Execute` remains flat, so the next cut should stay on interpreter-side super-array get/set dispatch or instruction-count reduction rather than native-binding follow-up.
- Touched files remain oversized (`execution_dispatch.c`, `object_super_array.c`), but this round stayed within the existing hot-path responsibility instead of adding a new subsystem; no semantic split was introduced here.
- `zr_vm_container_runtime_test` was not accepted in this round because the current working tree still shows an unrelated stack-smash failure in that target.

## Second Follow-up: Dispatch Copy And Super-array Get/Set Tightening

Scope:

- split hot stack-to-ret versus stack-to-stack copy handling in `zr_vm_core/src/zr_vm_core/execution_dispatch.c`
- keep the fast copy predicate as a single ownership-kind OR check in `zr_vm_core/src/zr_vm_core/execution_dispatch.c`
- tighten cached hidden-items lookup and plain int/null writes in `zr_vm_core/src/zr_vm_core/object_super_array_internal.h`
- explicitly reject a fetch-stage destination prefetch experiment after `callgrind` proved it regressed total `Ir`

WSL gcc validation commands:

```bash
cmake --build ~/codex-builds/zr_vm-gcc-relwithdebinfo --target zr_vm_cli_executable zr_vm_container_temp_value_root_test -j 8
cd ~/codex-builds/zr_vm-gcc-relwithdebinfo/bin && LD_LIBRARY_PATH=../lib ./zr_vm_container_temp_value_root_test
cd /mnt/e/Git/zr_vm/tests/benchmarks/cases/matrix_add_2d/zr && LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib /usr/bin/time -f "ELAPSED %e" /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary
cd /mnt/e/Git/zr_vm/tests/benchmarks/cases/matrix_add_2d/zr && LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib valgrind --tool=callgrind --callgrind-out-file=/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.dispatch-getset-final.out /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary
```

WSL gcc results:

- `zr_vm_container_temp_value_root_test`: PASS
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.05s`
- callgrind total `Ir`: `43,425,391`
- `execution_dispatch.c:ZrCore_Execute`: `32,754,181 Ir`
- `object_super_array_internal.h:ZrCore_Execute`: `2,420,736 Ir`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,108 Ir`

Comparison to the immediately preceding accepted snapshot in this document:

- total `Ir`: `-1,170,423` from `44,595,814`
- `execution_dispatch.c:ZrCore_Execute`: `-889,223` from `33,643,404`
- `object_super_array_internal.h:ZrCore_Execute`: `-270,336` from `2,691,072`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: effectively flat (`+1 Ir`)

Rejected sub-iteration:

- A fetch-stage destination prefetch prototype was measured with `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.dispatch-getset-next.out`.
- That prototype increased total `Ir` to `46,667,855`, increased `execution_dispatch.c:ZrCore_Execute` to `35,994,530 Ir`, and therefore was reverted.

WSL clang validation:

- build directory: `/home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo`
- `zr_vm_container_temp_value_root_test`: PASS
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.06s`

Windows validation:

- `build\\codex-msvc-relwithdebinfo`: `matrix_add_2d` checksum `76802768`, wall time `0.05s`
- `build\\codex-msvc-release`: `matrix_add_2d` checksum `76802768`, wall time `0.04s`
- `build\\codex-msvc-smoke-debug`: `hello_world` still runs correctly, but the incremental rebuild hit transient `LNK1168` on `zr_vm_core.dll`; this is a Windows file-lock issue rather than a runtime semantic regression, because the updated `RelWithDebInfo` and `Release` binaries both rebuilt and ran successfully.

Updated next cut after this round:

- `execution_dispatch.c:ZrCore_Execute` is still the main hotspot, but the fetch-stage destination prefetch variant is now disproven.
- The next execution-side cuts should stay on label-local density or new fast labels for existing thin super-array helpers, not on speculative prefetch tables.
- `object_super_array.c:ZrCore_Object_SuperArrayFillInt4ConstAssumeFast` is unchanged and remains the next super-array boundary after the interpreter get/set path.
