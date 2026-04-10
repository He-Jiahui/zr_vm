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

## Third Follow-up: Fresh Binary Validation And Rejected `SUPER_ARRAY_GET_INT_PLAIN_DEST`

Scope:

- verify benchmark binary freshness instead of trusting stale checked-in `tests/benchmarks/cases/matrix_add_2d/zr/bin/*`
- measure and reject a new emitted opcode `SUPER_ARRAY_GET_INT_PLAIN_DEST`
- keep the plain-destination store optimization, but fold it back into the existing `SUPER_ARRAY_GET_INT` helper instead of paying a new dispatch label/opcode tax

Fresh compile correction:

- plain source execution of `benchmark_matrix_add_2d.zrp` does not rewrite checked-in `bin/main.zri` or `bin/main.zro`
- a clean sandbox was used at `E:\Git\zr_vm\dumps\matrix_add_2d_perf_sandbox`
- fresh compile command:

```bash
cd /mnt/e/Git/zr_vm/dumps/matrix_add_2d_perf_sandbox
LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli --compile benchmark_matrix_add_2d.zrp --intermediate
```

Rejected sub-iteration:

- emitted `SUPER_ARRAY_GET_INT_PLAIN_DEST` and refreshed sandbox binary
- callgrind file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.superarray-get-plain-dest.out`
- checksum: `76802768`
- callgrind total `Ir`: `44,125,915`
- `execution_dispatch.c:ZrCore_Execute`: `33,543,815 Ir`
- `object_super_array_internal.h:ZrCore_Execute`: `2,273,280 Ir`

Comparison against the immediate accepted pre-experiment baseline:

- baseline file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.superarray-getset-direct-extra.out`
- baseline total `Ir`: `42,971,157`
- baseline `execution_dispatch.c:ZrCore_Execute`: `32,299,530 Ir`
- baseline `object_super_array_internal.h:ZrCore_Execute`: `2,420,736 Ir`
- result: helper-side savings were real, but the new opcode increased dispatch cost more than it saved; this variant was rejected

Accepted salvage cut after the rejection:

- compiler quickening stopped emitting `SUPER_ARRAY_GET_INT_PLAIN_DEST`
- the plain/no-ownership direct-store fast path was folded into the existing `ZrCore_Object_SuperArrayGetIntByValueInlineAssumeFast`
- refreshed sandbox `main.zri` returned to plain `SUPER_ARRAY_GET_INT` only

Validation:

- WSL gcc benchmark checksum: `76802768`
- WSL gcc benchmark wall time: `0.04s`
- sandbox binary benchmark checksum: `76802768`
- sandbox binary benchmark wall time: `0.06s`
- accepted salvage callgrind file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.superarray-get-inline-fastpath.sandbox.out`
- accepted salvage total `Ir`: `42,652,170`
- `execution_dispatch.c:ZrCore_Execute`: `31,930,228 Ir`
- `object_super_array_internal.h:ZrCore_Execute`: `2,420,736 Ir`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,108 Ir`

Accepted salvage delta versus the pre-experiment accepted baseline:

- total `Ir`: `-318,987`
- `execution_dispatch.c:ZrCore_Execute`: `-369,302`
- `object_super_array_internal.h:ZrCore_Execute`: effectively flat

Cross-platform validation for the accepted salvage cut:

- WSL clang `RelWithDebInfo`: checksum `76802768`, wall time `0.04s`
- Windows MSVC `RelWithDebInfo`: checksum `76802768`, wall time `0.06s`
- Windows MSVC `Release`: checksum `76802768`, wall time `0.07s`

## Fourth Follow-up: Reuse Native Result Writes Without Rewriting Ownership Fields

Scope:

- shrink `execution_dispatch.c` hot result stores by reusing the existing normalized no-ownership state of `destination`
- shrink `object_super_array_internal.h` and `object_super_array.c` plain int/null writes by avoiding redundant writes to `ownershipKind`, `ownershipControl`, and `ownershipWeakRef`
- keep the optimization inside hot helpers that already rely on “safe overwrite without release” invariants

Implementation summary:

- `execution_dispatch.c`
  - `ALGORITHM_1`
  - `ALGORITHM_2`
  - `ALGORITHM_CVT_2`
  - `ALGORITHM_CONST_2`
  - `ALGORITHM_FUNC_2`
  now store native results with a “reuse normalized no-ownership destination” helper instead of `ZR_VALUE_FAST_SET(...)`
- `object_super_array_internal.h`
  - `zr_super_array_store_plain_int_reuse`
  - `zr_super_array_store_plain_null_reuse`
  - plain-destination branches inside `ZrCore_Object_SuperArrayGetIntByValueInlineAssumeFast`
  now only rewrite the fields that actually change
- `object_super_array.c`
  - `object_store_plain_int_reuse`
  - `object_store_plain_null_reuse`
  - `object_try_super_array_set_cached_int_field`
  now avoid the same redundant ownership-field rewrites

WSL gcc validation:

- build directory: `/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo`
- `zr_vm_container_temp_value_root_test`: PASS
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.04s`
- callgrind file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.dispatch-reuse-store.out`
- callgrind total `Ir`: `41,113,439`
- `execution_dispatch.c:ZrCore_Execute`: `30,711,097 Ir`
- `object_super_array_internal.h:ZrCore_Execute`: `2,101,248 Ir`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,092 Ir`

Comparison against the immediate accepted salvage cut above:

- total `Ir`: `-1,538,731` from `42,652,170`
- `execution_dispatch.c:ZrCore_Execute`: `-1,219,131` from `31,930,228`
- `object_super_array_internal.h:ZrCore_Execute`: `-319,488` from `2,420,736`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: effectively flat (`-16`)

Relevant line-level confirmation from `callgrind_annotate`:

- fast `ADD_INT` body dropped from `2,066,688 Ir` to `1,463,904 Ir`
- fast `ADD_INT_CONST` body dropped from `2,241,408 Ir` to `1,637,952 Ir`
- the overall `SUPER_ARRAY_GET_INT` / `SUPER_ARRAY_SET_INT` dispatch bodies stayed flat, which matches the intended effect: this cut reduced write-back work inside helpers and native result stores, not opcode count

WSL clang validation:

- build directory: `/home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo`
- `zr_vm_container_temp_value_root_test`: PASS
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.05s`

Windows MSVC validation:

- `build\\codex-msvc-relwithdebinfo`: checksum `76802768`, wall time `0.05s`
- `build\\codex-msvc-release`: checksum `76802768`, wall time `0.06s`

Updated next cut after this round:

- `execution_dispatch.c:ZrCore_Execute` is still the dominant hotspot, but the easy “native result write-back” redundancy is now largely removed
- `GET_STACK` / `GET_CONSTANT` fast copy and `DONE_FAST(1)` dispatch cost now stand out more clearly as the next interpreter-side density boundary
- `object_super_array.c:ZrCore_Object_SuperArrayFillInt4ConstAssumeFast` is now even more clearly isolated as the remaining super-array bulk-allocation boundary

## Fifth Follow-up: Localize `GET_STACK` / `GET_CONSTANT` Destination Selection

Scope:

- shrink `execution_dispatch.c` label-local density for fast `GET_STACK` / `GET_CONSTANT`
- move stack-vs-ret destination selection into a dedicated no-profile helper so the hot label no longer pays both `FAST_PREPARE_DESTINATION()` and a second `destination == &ret` split
- measure a `object_super_array.c` pair-template batch-init experiment for `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`, then reject and revert it because hotspot totals stayed flat

Accepted implementation summary:

- `execution_dispatch.c`
  - added `execution_copy_value_to_destination_fast_no_profile(...)`
  - `EXECUTE_GET_STACK_BODY_FAST` and `EXECUTE_GET_CONSTANT_BODY_FAST` now call that helper on the non-profiled path
  - `LZrFastInstruction_GET_STACK` / `LZrFastInstruction_GET_CONSTANT` no longer call `FAST_PREPARE_DESTINATION()` before entering the hot body

Rejected sub-iteration:

- `object_super_array.c` was temporarily changed to initialize dense int/int hash pairs from a prebuilt `SZrHashKeyValuePair` template
- measured callgrind file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.dispatch-getconst-pair-template.out`
- measured total `Ir`: `40,370,464`
- measured `execution_dispatch.c:ZrCore_Execute`: `29,970,568 Ir`
- measured `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,092 Ir`
- because the super-array hotspot stayed flat at the same `904,092 Ir` and the total delta versus the accepted variant below was only `-9,242 Ir`, this experiment was treated as noise rather than a proven win and was reverted

WSL gcc validation:

- build directory: `/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo`
- build command:

```bash
cmake --build /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo --target zr_vm_cli_executable zr_vm_container_temp_value_root_test -j8
```

- validation commands:

```bash
cd /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin && LD_LIBRARY_PATH=../lib ./zr_vm_container_temp_value_root_test
cd /mnt/e/Git/zr_vm/tests/benchmarks/cases/matrix_add_2d/zr && LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib /usr/bin/time -f "ELAPSED %e" /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary
cd /mnt/e/Git/zr_vm/tests/benchmarks/cases/matrix_add_2d/zr && LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib valgrind --tool=callgrind --callgrind-out-file=/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.dispatch-getconst-only.out /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary
```

WSL gcc results:

- `zr_vm_container_temp_value_root_test`: PASS
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.04s`
- accepted callgrind file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.dispatch-getconst-only.out`
- accepted callgrind total `Ir`: `40,379,706`
- `execution_dispatch.c:ZrCore_Execute`: `29,970,568 Ir`
- `object_super_array_internal.h:ZrCore_Execute`: `2,101,248 Ir`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,092 Ir`

Comparison against the immediate accepted fourth follow-up above:

- total `Ir`: `-733,733` from `41,113,439`
- `execution_dispatch.c:ZrCore_Execute`: `-740,529` from `30,711,097`
- `object_super_array_internal.h:ZrCore_Execute`: flat at `2,101,248`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: flat at `904,092`

Relevant line-level confirmation from `callgrind_annotate`:

- previous accepted snapshot paid a standalone `FAST_PREPARE_DESTINATION()` cost of `1,257,480 Ir` in `LZrFastInstruction_GET_STACK`
- current accepted snapshot no longer shows a separate `FAST_PREPARE_DESTINATION()` line in that label; the hot label now records `1,257,480 Ir` on `EXECUTE_GET_STACK_BODY_FAST()` directly
- previous accepted snapshot paid `223,584 Ir` for `FAST_PREPARE_DESTINATION()` plus `335,376 Ir` for `EXECUTE_GET_CONSTANT_BODY_FAST()` in `LZrFastInstruction_GET_CONSTANT`
- current accepted snapshot records `260,848 Ir` on `EXECUTE_GET_CONSTANT_BODY_FAST()`

WSL clang validation:

- build directory: `/home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo`
- `zr_vm_container_temp_value_root_test`: PASS
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.04s`

Windows MSVC validation:

- environment bootstrap: `C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1`
- `build\\codex-msvc-smoke-debug`: `zr_vm_cli_executable` rebuilt and `hello_world` still prints `hello world`
- `build\\codex-msvc-relwithdebinfo`: checksum `76802768`, first post-link run `0.13s`, immediate warm reruns `0.05s` and `0.03s`
- `build\\codex-msvc-release`: checksum `76802768`, initial measured run `0.03s`, immediate warm reruns `0.03s` and `0.03s`

Updated next cut after this round:

- `execution_dispatch.c:ZrCore_Execute` is still the dominant hotspot at `29,970,568 Ir`
- after this label-local cut, the next clearly isolated getter overhead is the still-always-false `destinationOffset == ZR_INSTRUCTION_USE_RET_FLAG` branch inside `execution_copy_value_to_destination_fast_no_profile(...)`, which costs `493,688 Ir` on the current benchmark even though `main.zri` contains zero `GET_STACK` / `GET_CONSTANT` ret destinations
- that makes the next natural direction instruction-level no-ret getter specialization or another instruction-count reduction, not more helper churn inside `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`
