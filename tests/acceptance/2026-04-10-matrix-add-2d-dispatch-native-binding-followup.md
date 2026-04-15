# Matrix Add 2D Dispatch And Native Binding Follow-up

Date: 2026-04-10

## Scope

- dispatch fast-table/static fallback tightening in `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- native binding temp-root/direct-root follow-up in `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c`
- dispatch stack/constant move fast-copy tightening in `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- super-array 4-way append/fill batch commit tightening in `zr_vm_core/src/zr_vm_core/object/object_super_array.c`

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

- split hot stack-to-ret versus stack-to-stack copy handling in `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- keep the fast copy predicate as a single ownership-kind OR check in `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- tighten cached hidden-items lookup and plain int/null writes in `zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h`
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

## Sixth Follow-up: Block-local Plain-Destination Recovery For Super-array Gets

Scope:

- recover a narrow `SUPER_ARRAY_GET_INT_PLAIN_DEST` emission path in `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c`
- keep the proof local to a single basic block and temp-only stack slots, instead of reviving the earlier rejected wider opcode-emission experiment
- reject and revert a late `GET_CONSTANT -> SET_STACK` peephole after measurement showed no static `main.zri` change
- extend the matrix benchmark regression in `tests/parser/test_compiler_regressions.c` so the hot read path must emit at least one `SUPER_ARRAY_GET_INT_PLAIN_DEST`

WSL gcc validation commands:

```bash
cmake --build /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo --target zr_vm_cli_executable zr_vm_container_temp_value_root_test -j8
cd /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin && LD_LIBRARY_PATH=../lib ./zr_vm_container_temp_value_root_test
rm -rf /tmp/matrix_add_2d_opt_plain_gcc2 && mkdir -p /tmp/matrix_add_2d_opt_plain_gcc2
cp -a /mnt/e/Git/zr_vm/tests/benchmarks/cases/matrix_add_2d/zr/. /tmp/matrix_add_2d_opt_plain_gcc2/
cd /tmp/matrix_add_2d_opt_plain_gcc2
LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli --compile benchmark_matrix_add_2d.zrp --intermediate
grep -n 'SUPER_ARRAY_GET_INT' /tmp/matrix_add_2d_opt_plain_gcc2/bin/main.zri
LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib /usr/bin/time -f "ELAPSED %e" /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary
LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib valgrind --tool=callgrind --callgrind-out-file=/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.plain-dest-stable.out /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary
```

WSL gcc results:

- `zr_vm_container_temp_value_root_test`: PASS
- fresh `main.zri` now emits 4 `SUPER_ARRAY_GET_INT_PLAIN_DEST` instructions out of the 7 matrix hot-path `SUPER_ARRAY_GET_INT` family reads
- the fresh matrix entry function now ends at instruction `[141]`, so the static hot function shape dropped from `144` instructions to `142`
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.01s`
- accepted callgrind file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.plain-dest-stable.out`
- accepted callgrind total `Ir`: `38,785,168`
- `execution_dispatch.c:ZrCore_Execute`: `27,409,889 Ir`
- `object_super_array_internal.h:ZrCore_Execute`: `1,953,792 Ir`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,115 Ir`

Comparison against the immediate pre-slice accepted gcc baseline from this session:

- baseline file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.getstack-forward-arith.fillfix.out`
- baseline total `Ir`: `38,932,140`
- baseline `execution_dispatch.c:ZrCore_Execute`: `27,409,889 Ir`
- baseline `object_super_array_internal.h:ZrCore_Execute`: `2,101,248 Ir`
- baseline `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,115 Ir`
- delta: total `Ir` `-146,972`
- delta: `execution_dispatch.c:ZrCore_Execute` flat
- delta: `object_super_array_internal.h:ZrCore_Execute` `-147,456`
- delta: `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast` flat

WSL clang validation:

- build directory: `/home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo`
- `zr_vm_container_temp_value_root_test`: PASS
- fresh clang `main.zri` now matches gcc and emits the same 4 `SUPER_ARRAY_GET_INT_PLAIN_DEST` instructions
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.01s`

Windows MSVC validation:

- build directory: `E:\Git\zr_vm\build\codex-msvc-relwithdebinfo`
- rebuild command: `cmake --build E:\Git\zr_vm\build\codex-msvc-relwithdebinfo --config RelWithDebInfo --target zr_vm_cli_executable --parallel 8`
- `hello_world` smoke output: `hello world`
- fresh Windows verification sandbox: `E:\Git\zr_vm\tmp\matrix_add_2d_msvc_verify`
- Windows fresh compile emitted the same 4 `SUPER_ARRAY_GET_INT_PLAIN_DEST` instructions as WSL
- Windows fresh matrix run checksum: `76802768`
- Windows fresh matrix run wall time: `35.44 ms`

Rejected sub-iteration:

- a late `GET_CONSTANT -> SET_STACK` peephole was implemented after the plain-dest recovery
- fresh matrix `main.zri` showed no change at all, so the peephole added complexity without reducing instruction count
- that sub-iteration was reverted before acceptance

Remaining validation gap:

- `zr_vm_compiler_regressions_test` still aborts immediately on the unrelated long-standing `test_class_member_nested_functions_keep_constant_indices_in_range` stack-smash path, so it was not used as an acceptance gate for this slice

## Seventh Follow-up: Super-array Store-to-Load Forwarding

Scope:

- forward block-local `SUPER_ARRAY_SET_INT` result read-backs in `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c`
- rewrite same-block `SUPER_ARRAY_GET_INT` / `SUPER_ARRAY_GET_INT_PLAIN_DEST` reads back to `GET_STACK valueSlot` when the intervening instructions are narrow pure stack-safe ops that do not touch receiver/index/value slots
- measure and reject another dispatch-body arithmetic fast-label experiment after `callgrind` showed it regressed total `Ir`

Rejected sub-iteration:

- a dispatch experiment temporarily replaced arithmetic fast-label destination preparation with a stack-only variant in `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- measured file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.stack-dest-fastlabels.out`
- measured total `Ir`: `39,879,217`
- because it regressed against the accepted gcc baseline of `38,785,168 Ir`, the experiment was reverted

Accepted implementation summary:

- `compiler_quickening.c`
  - tracks `SUPER_ARRAY_SET_INT` writes across a same-block pure-op window
  - rewrites later matching `SUPER_ARRAY_GET_INT` / `SUPER_ARRAY_GET_INT_PLAIN_DEST` instructions to `GET_STACK`
  - leaves the optimization local and side-effect-safe instead of extending interpreter-side dispatch
- `tests/parser/test_compiler_regressions.c`
  - matrix benchmark regression now also caps the hot `SUPER_ARRAY_GET_INT` family count at `<= 5`

WSL gcc validation:

- build directory: `/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo`
- fresh verification sandbox: `/tmp/matrix_add_2d_store_forward_gcc`
- fresh `main.zri` now forwards the two read-backs at instructions `[94]` and `[103]` to `GET_STACK`
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.01s`
- accepted callgrind file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.store-forward.out`
- accepted callgrind total `Ir`: `38,707,107`
- `execution_dispatch.c:ZrCore_Execute`: `27,680,225 Ir`
- `object_super_array_internal.h:ZrCore_Execute`: `1,609,728 Ir`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,115 Ir`

Comparison against the immediate accepted sixth follow-up above:

- baseline file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.plain-dest-stable.out`
- baseline total `Ir`: `38,785,168`
- baseline `execution_dispatch.c:ZrCore_Execute`: `27,409,889 Ir`
- baseline `object_super_array_internal.h:ZrCore_Execute`: `1,953,792 Ir`
- baseline `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,115 Ir`
- delta: total `Ir` `-78,061`
- delta: `execution_dispatch.c:ZrCore_Execute` `+270,336`
- delta: `object_super_array_internal.h:ZrCore_Execute` `-344,064`
- delta: `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast` flat

Interpretation:

- this cut is a real net win, but it wins by deleting two hot super-array reads rather than improving dispatch itself
- `execution_dispatch.c` remained the main hotspot and actually rose slightly, which is why later cuts stayed focused on instruction-count reduction instead of more speculative label reshaping

WSL clang validation:

- build directory: `/home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo`
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.03s`

Windows MSVC validation:

- build directory: `E:\Git\zr_vm\build\codex-msvc-relwithdebinfo`
- warm `matrix_add_2d` checksum: `76802768`
- warm wall time: `35.44 ms`

## Eighth Follow-up: Fold Pure Int Result Stores Into Final Slots

Scope:

- add a new quickening peephole in `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c`
- fold a pure int-result producer plus immediate `SET_STACK` copy into a direct final-slot write
- keep the whitelist narrow: `GET_CONSTANT` plain primitives, integer arithmetic, and `LOGICAL_LESS_EQUAL_SIGNED`
- add a matrix regression that checks the hot loop after `SUPER_ARRAY_FILL_INT4_CONST` no longer keeps these producer-plus-copy pairs

Accepted implementation summary:

- `compiler_quickening.c`
  - added `compiler_quickening_try_fold_direct_result_store(...)`
  - added `compiler_quickening_fold_direct_result_stores(...)`
  - compacts the new `NOP`s before the existing store-to-load forwarding pass
- `tests/parser/test_compiler_regressions.c`
  - added `test_matrix_add_2d_compile_folds_direct_result_stores_into_final_slots`
  - the assertion is intentionally scoped to the hot loop after the fill setup; two conservative preheader pairs remain outside the hot loop, but the loop body itself is clean

WSL gcc validation:

- build directory: `/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo`
- fresh verification sandbox: `/tmp/matrix_add_2d_direct_store_fold`
- fresh `main.zri` instruction count: `137`, down from the previous accepted `144`
- hot-loop examples now write directly into the final destination slots:
  - `ADD_INT (extra=35, operand1=38, operand2=29)` at `[82]`
  - `ADD_INT (extra=28, operand1=36, operand2=41)` at `[103]`
  - `MOD_SIGNED_CONST (extra=21, left_slot=42, constant_index=17)` at `[112]`
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.01s`
- accepted callgrind file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.direct-store-fold.sandbox.out`
- accepted callgrind total `Ir`: `37,154,862`
- `execution_dispatch.c:ZrCore_Execute`: `26,939,065 Ir`
- `object_super_array_internal.h:ZrCore_Execute`: `1,609,728 Ir`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,115 Ir`

Comparison against the immediate accepted seventh follow-up above:

- baseline file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.store-forward.out`
- baseline total `Ir`: `38,707,107`
- baseline `execution_dispatch.c:ZrCore_Execute`: `27,680,225 Ir`
- baseline `object_super_array_internal.h:ZrCore_Execute`: `1,609,728 Ir`
- baseline `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,115 Ir`
- delta: total `Ir` `-1,552,245`
- delta: `execution_dispatch.c:ZrCore_Execute` `-741,160`
- delta: `object_super_array_internal.h:ZrCore_Execute` flat
- delta: `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast` flat

WSL regression validation:

- command:

```bash
cmake --build /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo --target zr_vm_compiler_regressions_test -j 8
/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_compiler_regressions_test
```

- matrix-related regressions now all pass, including:
  - `test_matrix_add_2d_compile_avoids_adjacent_temp_reloads_before_super_array_int_ops`
  - `test_matrix_add_2d_compile_folds_right_hand_int_constants_into_const_opcodes`
  - `test_matrix_add_2d_compile_eliminates_temp_self_updates_for_add_int_const`
  - `test_matrix_add_2d_compile_folds_direct_result_stores_into_final_slots`
  - `test_matrix_add_2d_compile_eliminates_generic_array_int_index_opcodes`
- the full binary still has one unrelated existing failure:
  - `test_initializer_bound_local_is_visible_on_next_source_line`

WSL clang validation:

- build directory: `/home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo`
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.01s`

Windows MSVC validation:

- environment bootstrap: `C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1`
- build directory: `E:\Git\zr_vm\build\codex-msvc-relwithdebinfo`
- build directory: `E:\Git\zr_vm\build\codex-msvc-release`
- `RelWithDebInfo` warm `matrix_add_2d` checksum: `76802768`
- `RelWithDebInfo` warm wall time: `48 ms`
- `Release` warm `matrix_add_2d` checksum: `76802768`
- `Release` warm wall time: `38 ms`

Updated next cut after this round:

- `execution_dispatch.c:ZrCore_Execute` is still the dominant hotspot even after dropping to `26,939,065 Ir`
- `object_super_array.c:ZrCore_Object_SuperArrayFillInt4ConstAssumeFast` remains flat at `904,115 Ir`, so the next super-array-specific work should stay on pair init / allocation-path tightening instead of interpreter churn
- because the compiler-side instruction-count cuts are still paying off, the next natural continuation is another hot-loop instruction deletion or a proven runtime-side specialization, not a speculative dispatch-table rewrite

## Ninth Follow-up: Narrow Successor-Aware `SUB_INT_CONST` Fold

Scope:

- keep chasing compiler-side hot-loop instruction deletion inside `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c`
- specifically fold the matrix loop-bound pattern `GET_CONSTANT 1 -> SUB_INT` into `SUB_INT_CONST` even when the current basic block ends at `JUMP_IF`
- avoid regressing the already-accepted `ADD_INT_CONST` self-update and direct-result-store folds

Rejected sub-iteration:

- the first attempt widened successor-aware dead-temp analysis to every right-constant arithmetic fold
- fresh sandbox `main.zri` grew from `137` to `139` instructions because three `ADD_INT_CONST` self-update sites regressed back into generic `GET_STACK -> GET_CONSTANT -> ADD_INT`
- measured file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.sub-int-const-fold.sandbox.out`
- measured total `Ir`: `37,754,324`
- measured `execution_dispatch.c:ZrCore_Execute`: `27,542,356 Ir`
- because it regressed against the accepted eighth follow-up baseline of `37,154,862 Ir`, that wider variant was narrowed before acceptance

Accepted implementation summary:

- `compiler_quickening.c`
  - added a small successor-aware dead-temp walker that follows basic-block successors until a temp slot is overwritten, read, or proven dead at exit
  - only uses that stronger CFG walk as a fallback for `SUB_INT` right-constant folds when the older block-local rule cannot prove the temp dead
  - leaves `ADD_INT`, `MUL_SIGNED`, `DIV_SIGNED`, and `MOD_SIGNED` on the previous narrower rule so existing self-update peepholes keep firing
- `tests/parser/test_compiler_regressions.c`
  - added `count_sub_int_right_constant_pairs_recursive(...)`
  - added `test_matrix_add_2d_compile_folds_loop_bounds_into_sub_int_const`

WSL gcc validation:

- build directory: `/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo`
- fresh verification sandbox: `/tmp/matrix_add_2d_sub_int_const_fold`
- fresh `main.zri` instruction count: `133`, down from the previous accepted `137`
- loop-header bound checks now compact into `SUB_INT_CONST`:
  - `[47] SUB_INT_CONST (extra=12, left_slot=12, constant_index=9)`
  - `[67] SUB_INT_CONST (extra=12, left_slot=12, constant_index=9)`
  - `[75] SUB_INT_CONST (extra=12, left_slot=12, constant_index=9)`
  - `[116] SUB_INT_CONST (extra=12, left_slot=12, constant_index=9)`
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.01s`
- accepted callgrind file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.sub-int-const-fold.sandbox.out`
- accepted callgrind total `Ir`: `36,295,792`
- `execution_dispatch.c:ZrCore_Execute`: `26,086,708 Ir`
- `object_super_array_internal.h:ZrCore_Execute`: `1,609,728 Ir`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,115 Ir`

Comparison against the immediate accepted eighth follow-up above:

- baseline file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.direct-store-fold.sandbox.out`
- baseline total `Ir`: `37,154,862`
- baseline `execution_dispatch.c:ZrCore_Execute`: `26,939,065 Ir`
- baseline `object_super_array_internal.h:ZrCore_Execute`: `1,609,728 Ir`
- baseline `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,115 Ir`
- delta: total `Ir` `-859,070`
- delta: `execution_dispatch.c:ZrCore_Execute` `-852,357`
- delta: `object_super_array_internal.h:ZrCore_Execute` flat
- delta: `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast` flat

WSL regression validation:

- command:

```bash
cmake --build /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo --target zr_vm_compiler_regressions_test -j 8
/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_compiler_regressions_test
```

- matrix-related regressions pass, including the new:
  - `test_matrix_add_2d_compile_folds_loop_bounds_into_sub_int_const`
- the full binary still retains one unrelated existing failure:
  - `test_initializer_bound_local_is_visible_on_next_source_line`

WSL clang validation:

- build directory: `/home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo`
- fresh verification sandbox: `/tmp/matrix_add_2d_sub_int_const_fold_clang`
- `matrix_add_2d` checksum: `76802768`
- `matrix_add_2d` wall time: `0.01s`

Windows MSVC validation:

- environment bootstrap: `C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1`
- build directory: `E:\Git\zr_vm\build\codex-msvc-relwithdebinfo`
- build directory: `E:\Git\zr_vm\build\codex-msvc-release`
- fresh verification sandbox: `%TEMP%\codex-matrix-add-2d-msvc-relwithdebinfo`
- fresh verification sandbox: `%TEMP%\codex-matrix-add-2d-msvc-release`
- `RelWithDebInfo` fresh `matrix_add_2d` checksum: `76802768`
- `RelWithDebInfo` fresh wall time: `42 ms`
- `Release` fresh `matrix_add_2d` checksum: `76802768`
- `Release` fresh wall time: `29 ms`

## Regression Harness Correction: Fresh Sandbox Matrix Compile Evidence

Scope:

- remove temporary `fprintf(stderr, ...)` tracing from `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c`
- stop `tests/parser/test_compiler_regressions.c` from compiling `matrix_add_2d` against stale checked-in `tests/benchmarks/cases/matrix_add_2d/zr/bin/*`
- extract a dedicated fresh-sandbox helper into:
  - `tests/parser/matrix_add_2d_compile_fixture.c`
  - `tests/parser/matrix_add_2d_compile_fixture.h`
- wire every `matrix_add_2d` compile regression through a copied sandbox project under `tests_generated/compiler_regressions/...`

Why this correction was necessary:

- the new zero-init regression was proving against stale cached benchmark artifacts instead of a freshly compiled project tree
- fresh CLI compilation in an isolated sandbox already showed the optimization was live; the harness was the stale component
- this section records the corrected evidence and the current cross-toolchain regression behavior

WSL gcc validation:

- build directory: `/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo`
- regression build command:

```bash
cmake --build /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo --target zr_vm_compiler_regressions_test -j 8
```

- regression run command:

```bash
cd /mnt/e/Git/zr_vm
LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_compiler_regressions_test
```

- fresh sandbox compile/run/callgrind command:

```bash
rm -rf /tmp/matrix_add_2d_verify_fresh_callgrind
mkdir -p /tmp/matrix_add_2d_verify_fresh_callgrind/src
cp /mnt/e/Git/zr_vm/tests/benchmarks/cases/matrix_add_2d/zr/benchmark_matrix_add_2d.zrp /tmp/matrix_add_2d_verify_fresh_callgrind/
cp /mnt/e/Git/zr_vm/tests/benchmarks/cases/matrix_add_2d/zr/src/main.zr /tmp/matrix_add_2d_verify_fresh_callgrind/src/
cp /mnt/e/Git/zr_vm/tests/benchmarks/cases/matrix_add_2d/zr/src/bench_config.zr /tmp/matrix_add_2d_verify_fresh_callgrind/src/
cd /tmp/matrix_add_2d_verify_fresh_callgrind
LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli --compile benchmark_matrix_add_2d.zrp --intermediate
LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary
valgrind --tool=callgrind --callgrind-out-file=/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.fresh-sandbox-regression-fix.out /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary
```

WSL gcc results:

- `zr_vm_compiler_regressions_test`: `25` tests, only `1` existing unrelated failure
- all `matrix_add_2d` compile regressions now pass, including:
  - `test_matrix_add_2d_compile_avoids_adjacent_temp_reloads_before_super_array_int_ops`
  - `test_matrix_add_2d_compile_folds_right_hand_int_constants_into_const_opcodes`
  - `test_matrix_add_2d_compile_eliminates_temp_self_updates_for_add_int_const`
  - `test_matrix_add_2d_compile_folds_loop_bounds_into_sub_int_const`
  - `test_matrix_add_2d_compile_eliminates_forwardable_get_stack_copy_reads`
  - `test_matrix_add_2d_compile_fuses_less_equal_signed_jump_if_loop_guards`
  - `test_matrix_add_2d_compile_folds_direct_result_stores_into_final_slots`
  - `test_matrix_add_2d_compile_eliminates_zero_init_constant_copy_pairs`
  - `test_matrix_add_2d_compile_eliminates_generic_array_int_index_opcodes`
- the remaining full-binary failure is still the pre-existing:
  - `test_initializer_bound_local_is_visible_on_next_source_line`
- fresh sandbox `main.zri` no longer shows a zero-init `GET_CONSTANT -> SET_STACK` pair after `SUPER_ARRAY_FILL_INT4_CONST`
- first hot-region proof from fresh sandbox:
  - `[42] SUPER_ARRAY_FILL_INT4_CONST`
  - `[43] GET_CONSTANT (extra=10, operand=8)`
  - `[44] SUB_INT_CONST`
  - `[45] JUMP_IF_GREATER_SIGNED`
- counted zero-init copy pairs after fill in the fresh sandbox artifact: `0`
- fresh sandbox benchmark checksum: `76802768`
- fresh sandbox benchmark wall time: `0.01s`
- callgrind file: `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.fresh-sandbox-regression-fix.out`
- callgrind total `Ir`: `30,516,783`
- `execution_dispatch.c:ZrCore_Execute`: `20,160,417 Ir`
- `object_super_array_internal.h:ZrCore_Execute`: `1,609,728 Ir`
- `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `904,115 Ir`
- current top native-binding cleanup sites in that same fresh-sandbox sample:
  - `ZrLib_TempValueRoot_End`: `252,610 Ir`
  - `native_binding_begin_rooted_value`: `151,755 Ir`
  - `native_binding_begin_rooted_object`: `141,934 Ir`

WSL clang validation:

- build directory: `/home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo`
- build command:

```bash
cmake --build /home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo --target zr_vm_compiler_regressions_test -j 8
```

- run command:

```bash
cd /mnt/e/Git/zr_vm
LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo/lib /home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo/bin/zr_vm_compiler_regressions_test
```

- results:
  - `zr_vm_compiler_regressions_test`: `25` tests, only the same existing failure `test_initializer_bound_local_is_visible_on_next_source_line`
  - all `matrix_add_2d` compile regressions pass under clang as well
  - build emitted unrelated existing warnings in `zr_vm_lib_system/src/zr_vm_lib_system/gc/gc_registry.c` and `module.c`, but no new error or regression in the touched matrix harness path

Windows MSVC validation:

- environment bootstrap:

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
```

- build directory: `E:\Git\zr_vm\build\codex-msvc-release-tests`
- build command:

```powershell
cmake --build E:\Git\zr_vm\build\codex-msvc-release-tests --target zr_vm_compiler_regressions_test --parallel 8
```

- run command:

```powershell
E:\Git\zr_vm\build\codex-msvc-release-tests\bin\zr_vm_compiler_regressions_test.exe
```

- results:
  - target build succeeds with the new `matrix_add_2d_compile_fixture.c`
  - runtime results match WSL: all `matrix_add_2d` compile regressions pass
  - the only observed failure remains `test_initializer_bound_local_is_visible_on_next_source_line`

Updated next cut after this correction:

- this round does not represent a new runtime optimization delta; it fixes the evidence path so future compiler/perf claims are grounded in fresh artifacts
- `execution_dispatch.c:ZrCore_Execute` and `object_super_array.c:ZrCore_Object_SuperArrayFillInt4ConstAssumeFast` remain the dominant runtime boundaries in the fresh-sandbox callgrind sample
- because the matrix compile regressions now rebuild a private project copy every time, the next optimization slice can safely add or tighten compiler peepholes without risking false negatives from stale benchmark `bin/` output

## Runtime Follow-Up: Fast Const Dispatch For `SUB_INT_CONST`

Scope:

- continue shrinking the interpreter-side `matrix_add_2d` hot loop inside `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- add fast-dispatch coverage for:
  - `SUB_INT_CONST`
  - `SUPER_ARRAY_FILL_INT4_CONST`
- convert redundant right-constant runtime type checks on hot `*_CONST` arithmetic bodies to internal `ZR_ASSERT` invariants for:
  - `ADD_INT_CONST`
  - `SUB_INT_CONST`
  - `MUL_SIGNED_CONST`
  - `DIV_SIGNED_CONST`
  - `MOD_SIGNED_CONST`
- leave compiler output unchanged; this slice is runtime-only and intentionally does not change `main.zri`

Immediate baseline for this slice:

- prior isolated-source callgrind file:
  - `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.source-validate.current-cut.out`
- prior isolated-source total `Ir`:
  - `30,414,952`
- prior isolated-source hotspot excerpt:
  - `/mnt/e/Git/zr_vm_source_validate/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:ZrCore_Execute`: `28,343,268`
  - `/mnt/e/Git/zr_vm_source_validate/zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:ZrCore_Execute`: `1,609,728`
  - `/mnt/e/Git/zr_vm_source_validate/zr_vm_core/src/zr_vm_core/object/object_super_array.c:ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `1,437,173`

WSL gcc validation:

- build directory:
  - `/home/hejiahui/codex-builds/zr_vm-gcc-source-validate`
- regression command:

```bash
cd /mnt/e/Git/zr_vm_source_validate
LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-source-validate/lib /home/hejiahui/codex-builds/zr_vm-gcc-source-validate/bin/zr_vm_compiler_regressions_test
```

- fresh sandbox benchmark/callgrind file:
  - `/tmp/matrix_add_2d_validate_current_cut_2`
  - `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.source-validate.current-cut-2.out`
- results:
  - `zr_vm_compiler_regressions_test`: `25` tests, only the same existing unrelated failure `test_initializer_bound_local_is_visible_on_next_source_line`
  - fresh sandbox `main.zri` instruction count: `206`
  - fresh sandbox benchmark checksum: `76802768`
  - fresh sandbox benchmark wall time: `0.01s`
  - callgrind total `Ir`: `29,084,255`
  - `/mnt/e/Git/zr_vm_source_validate/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:ZrCore_Execute`: `27,012,953`
  - `/mnt/e/Git/zr_vm_source_validate/zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:ZrCore_Execute`: `1,609,728`
  - `/mnt/e/Git/zr_vm_source_validate/zr_vm_core/src/zr_vm_core/object/object_super_array.c:ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `1,437,195`
  - `/mnt/e/Git/zr_vm_source_validate/zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:native_binding_begin_rooted_object`: `312,218`
  - `/mnt/e/Git/zr_vm_source_validate/zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:native_binding_begin_rooted_value`: `268,625`

Delta against the immediate isolated-source baseline above:

- total `Ir`: `-1,330,697`
- `execution_dispatch.c:ZrCore_Execute`: `-1,330,315`
- `object_super_array_internal.h:ZrCore_Execute`: flat
- `object_super_array.c:ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`: `+22` noise

Delta against the accepted fresh-sandbox regression-fix baseline:

- accepted reference file:
  - `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.fresh-sandbox-regression-fix.out`
- accepted fresh-sandbox total `Ir`: `30,516,783`
- current delta: `-1,432,528`

Interpretation:

- this cut materially reduced interpreter dispatch/self work without changing compiler output shape
- the win is concentrated in `execution_dispatch.c` itself, which matches the intended target for adding `SUB_INT_CONST` fast dispatch and removing redundant right-constant type checks
- `object_super_array` fill remains structurally flat, so the next meaningful runtime boundary is still the fill/grow path rather than the already-tightened dispatch around it

WSL clang validation:

- build directory:
  - `/home/hejiahui/codex-builds/zr_vm-clang-source-validate`
- regression results:
  - `zr_vm_compiler_regressions_test`: `25` tests, only the same existing unrelated failure `test_initializer_bound_local_is_visible_on_next_source_line`
- fresh sandbox benchmark:
  - checksum: `76802768`
  - wall time: `0.01s`

Windows MSVC validation:

- environment bootstrap:

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
```

- build directory:
  - `E:\Git\zr_vm_source_validate\build\codex-msvc-runtime-opt`
- build command:

```powershell
cmake -S E:\Git\zr_vm_source_validate -B E:\Git\zr_vm_source_validate\build\codex-msvc-runtime-opt -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build E:\Git\zr_vm_source_validate\build\codex-msvc-runtime-opt --config Release --target zr_vm_cli_executable --parallel 8
```

- fresh benchmark sandbox:
  - `%TEMP%\codex-matrix-add-2d-msvc-source-validate`
- results:
  - checksum: `76802768`
  - Release wall-time series: `60,28,36,27,27 ms`
  - Release min wall time: `27 ms`

Acceptance decision for this slice:

- accepted as a runtime-only follow-up cut
- no new correctness regression was observed on WSL gcc, WSL clang, or Windows MSVC Release CLI smoke
- remaining dominant runtime boundaries after this slice:
  - `execution_dispatch.c` main loop, albeit materially smaller than the prior isolated-source baseline
  - `object_super_array.c:ZrCore_Object_SuperArrayFillInt4ConstAssumeFast` and its dense grow path in `zr_vm_core/include/zr_vm_core/hash_set.h`

## Current Repo Follow-Up: Dense Grow Pair-Pool Co-Reserve And Normalized Plain-Destination Stores

Scope:

- continue the current-repo `matrix_add_2d` runtime work on:
  - `zr_vm_core/src/zr_vm_core/object/object_super_array.c`
  - `zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h`
  - `zr_vm_core/include/zr_vm_core/hash_set.h`
  - `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- tighten dense super-array fill/grow by co-reserving pair-pool capacity with dense bucket growth
- cut remaining arithmetic plain-destination store overhead by treating normalized plain stack destinations as an internal invariant and no longer re-writing `isGarbageCollectable` / `isNative` on every hot int store
- validate and reject one additional compiler-side branch:
  - disabling `SUPER_ARRAY_GET_INT_PLAIN_DEST` emission

Kept code changes:

- added `ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacityAndPairPoolExact(...)` so dense append growth can reserve buckets and pair-pool in one exact helper path
- removed the extra `object_super_array_target_pair_pool_capacity(...)` layer from `object_try_super_array_ensure_capacity_for_append(...)`
- added normalized plain-int/null store helpers in `object_super_array.c` / `object_super_array_internal.h` and used them only where the runtime already proves the destination is a plain primitive
- changed `EXECUTION_STORE_PLAIN_DIRECT(...)` in `execution_dispatch.c` to assert normalized plain-destination invariants and only write `type` plus payload, instead of redundantly re-writing the same native/non-GC flags on hot arithmetic plain-destination stores

Rejected branch:

- temporarily stopped emitting `SUPER_ARRAY_GET_INT_PLAIN_DEST` in `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c`
- fresh gcc callgrind regressed from the kept cut's `30,722,722 Ir` to `30,894,429 Ir`
- the branch was reverted; the accepted state keeps `SUPER_ARRAY_GET_INT_PLAIN_DEST`
- a clean rebuild was required after that experiment to restore the expected fresh `main.zri` shape

WSL gcc validation:

- build directory:
  - `/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo`
- build command:

```bash
cmake --build /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo --target zr_vm_cli_executable --clean-first -j 8
```

- fresh sandbox:
  - `/tmp/matrix_add_2d_check`
- compile command:

```bash
cd /tmp/matrix_add_2d_check
LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli --compile benchmark_matrix_add_2d.zrp --intermediate
```

- callgrind command:

```bash
cd /tmp/matrix_add_2d_check
LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib valgrind --tool=callgrind --callgrind-out-file=/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.final-current.out /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary
```

- fresh `main.zri` shape:
  - still emits `SUPER_ARRAY_FILL_INT4_CONST`
  - still emits the 2 hot `SUPER_ARRAY_GET_INT_PLAIN_DEST` reads
- runtime result:
  - checksum: `76802768`
- kept cut callgrind file:
  - `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.final-current.out`
- kept cut results:
  - total `Ir`: `30,722,722`
  - `execution_dispatch.c:ZrCore_Execute`: `19,591,587`
  - `SUPER_ARRAY_GET_INT_PLAIN_DEST` body sample: `221,184`

Delta against the immediate pre-cut baseline for this session:

- baseline file:
  - `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.pre-next.out`
- baseline total `Ir`:
  - `30,994,660`
- baseline `execution_dispatch.c:ZrCore_Execute`:
  - `19,788,195`
- current delta:
  - total `Ir`: `-271,938`
  - `execution_dispatch.c:ZrCore_Execute`: `-196,608`

Delta against the earlier fresh baseline that still emitted `SUPER_ARRAY_GET_INT_PLAIN_DEST` before this cut:

- baseline file:
  - `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.current-cut.out`
- baseline total `Ir`:
  - `30,832,999`
- current delta:
  - total `Ir`: `-110,277`

WSL clang validation:

- build directory:
  - `/home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo`
- build command:

```bash
cmake --build /home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo --target zr_vm_cli_executable -j 8
```

- fresh sandbox:
  - `/tmp/matrix_add_2d_clang`
- results:
  - fresh `main.zri` matches gcc and still emits `SUPER_ARRAY_FILL_INT4_CONST` plus the same 2 `SUPER_ARRAY_GET_INT_PLAIN_DEST` reads
  - checksum: `76802768`

Windows MSVC validation:

- Visual Studio environment bootstrap:

```powershell
& "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" where.exe cl
```

- CLI smoke build directory:
  - `E:\Git\zr_vm\build\codex-msvc-relwithdebinfo`
- CLI smoke command:

```powershell
& "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build E:\Git\zr_vm\build\codex-msvc-relwithdebinfo --config RelWithDebInfo --target zr_vm_cli_executable --parallel 8
E:\Git\zr_vm\build\codex-msvc-relwithdebinfo\bin\zr_vm_cli.exe E:\Git\zr_vm\tests\fixtures\projects\hello_world\hello_world.zrp
```

- result:
  - CLI smoke prints `hello world`

- compiler regression build directory:
  - `E:\Git\zr_vm\build\codex-msvc-release-tests`
- build command:

```powershell
& "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build E:\Git\zr_vm\build\codex-msvc-release-tests --config Release --target zr_vm_compiler_regressions_test --parallel 8
```

- run command:

```powershell
E:\Git\zr_vm\build\codex-msvc-release-tests\bin\zr_vm_compiler_regressions_test.exe
```

- result:
  - the updated `matrix_add_2d` compile regression passes
  - the only observed failure remains the existing unrelated baseline `test_initializer_bound_local_is_visible_on_next_source_line`

Acceptance decision for this slice:

- accepted as a current-repo follow-up cut
- kept changes are:
  - dense grow plus pair-pool co-reserve
  - normalized plain-destination direct stores for arithmetic and super-array internal hot values
- explicitly rejected change:
  - removing `SUPER_ARRAY_GET_INT_PLAIN_DEST` emission
- no new correctness regression was observed in:
  - WSL gcc fresh benchmark run
  - WSL clang fresh benchmark run
  - Windows MSVC CLI smoke
  - Windows `zr_vm_compiler_regressions_test` beyond the pre-existing unrelated baseline failure

## Current Repo Follow-Up: Exact Reserved Pair Spans And Stack-Direct Plain-Destination Arithmetic

Scope:

- continue the current-repo `matrix_add_2d` runtime work on:
  - `zr_vm_core/include/zr_vm_core/hash_set.h`
  - `zr_vm_core/src/zr_vm_core/object/object_super_array.c`
  - `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- cut the remaining super-array fill/grow overhead by preferring one exact reserved pair span, especially the newly grown tail block, instead of always walking cursor/span fragments
- cut more plain-destination arithmetic overhead by letting `ADD_INT_PLAIN_DEST` / `ADD_INT_CONST_PLAIN_DEST` / `SUB_INT_PLAIN_DEST` / `SUB_INT_CONST_PLAIN_DEST` / `MUL_SIGNED_PLAIN_DEST` / `MUL_SIGNED_CONST_PLAIN_DEST` / `DIV_SIGNED_CONST_PLAIN_DEST` / `MOD_SIGNED_CONST_PLAIN_DEST` write the stack slot directly on the hot success path without first routing through the shared `destination` fetch/prep path

Kept code changes:

- added `ZrCore_HashSet_HasReservedPairSpanExactPreferTailAssumeAvailable(...)` and `ZrCore_HashSet_TakeReservedPairSpanExactPreferTailAssumeAvailable(...)`
- used those helpers from `object_super_array.c` so large dense append/fill batches can take one exact reserved span immediately when the tail block already has enough room
- rewrote the hot plain-destination arithmetic bodies in `execution_dispatch.c` to:
  - read operands through local pointers
  - compute/store directly into `&BASE(E(instruction))->value`
  - only assign the shared `destination` variable when the slow numeric fallback is actually needed
- removed the extra fast-label `FAST_PREPARE_STACK_DESTINATION()` fetch from those plain-destination arithmetic opcodes because the successful fast path now materializes its destination locally

Temporary regression found and fixed before acceptance:

- the first draft reused the identifier `plainDestination__` inside `EXECUTION_STORE_PLAIN_DIRECT_TO(...)`
- that macro-local name collided with the outer plain-destination local in `MUL_SIGNED_PLAIN_DEST`, expanding into self-initialization and causing a null-destination crash on gcc
- reproduced with:

```bash
gdb -q -batch \
  -ex "file /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli" \
  -ex "set env LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib" \
  -ex "run /tmp/matrix_add_2d_nextcut/benchmark_matrix_add_2d.zrp --execution-mode binary" \
  -ex "bt" \
  -ex "frame 0" \
  -ex "info locals"
```

- fixed by renaming the macro-local temporary to `executionPlainDestination__`

WSL gcc validation:

- build directory:
  - `/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo`
- build commands:

```bash
cmake --build /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo --target zr_vm_cli_executable --clean-first -j 8
cmake --build /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo --target zr_vm_cli_executable -j 8
```

- fresh sandbox:
  - `/tmp/matrix_add_2d_nextcut`
- compile command:

```bash
cd /tmp/matrix_add_2d_nextcut
LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli --compile benchmark_matrix_add_2d.zrp --intermediate
```

- fresh `main.zri` shape:
  - still emits `SUPER_ARRAY_FILL_INT4_CONST`
  - still emits the 2 hot `SUPER_ARRAY_GET_INT_PLAIN_DEST` reads
  - still emits the arithmetic plain-destination opcodes used by the benchmark, including `MUL_SIGNED_PLAIN_DEST` and `ADD_INT_PLAIN_DEST`
- runtime result:
  - checksum: `76802768`
- callgrind command:

```bash
cd /tmp/matrix_add_2d_nextcut
LD_LIBRARY_PATH=/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/lib valgrind --tool=callgrind --callgrind-out-file=/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.next-cut2.out /home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary
```

- callgrind file:
  - `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.next-cut2.out`
- results:
  - total `Ir`: `30,636,316`
  - `execution_dispatch.c:ZrCore_Execute`: `19,505,550`

Delta against the accepted previous current-repo cut above:

- reference file:
  - `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.final-current.out`
- previous total `Ir`:
  - `30,722,722`
- previous `execution_dispatch.c:ZrCore_Execute`:
  - `19,591,587`
- current delta:
  - total `Ir`: `-86,406`
  - `execution_dispatch.c:ZrCore_Execute`: `-86,037`

Interpretation:

- the direct stack-destination rewrite removed another small but real slice from `execution_dispatch.c` itself
- the exact reserved pair-span preference is safe and active on the benchmark path, but its win is now folded into the already smaller dispatch/runtime total rather than surfacing as a separate top-level symbol in this annotate cut
- this slice is materially smaller than the earlier dispatch-table and plain-destination normalization cuts, but it still moves the accepted gcc fresh-sandbox baseline in the correct direction without changing compiler output shape or benchmark checksum

WSL clang validation:

- build directory:
  - `/home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo`
- build command:

```bash
cmake --build /home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo --target zr_vm_cli_executable -j 8
```

- fresh sandbox:
  - `/tmp/matrix_add_2d_clang_nextcut`
- result:
  - checksum: `76802768`

Windows MSVC validation:

- CLI smoke build directory:
  - `E:\Git\zr_vm\build\codex-msvc-relwithdebinfo`
- build command:

```powershell
& "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build E:\Git\zr_vm\build\codex-msvc-relwithdebinfo --config RelWithDebInfo --target zr_vm_cli_executable --parallel 8
```

- smoke command:

```powershell
E:\Git\zr_vm\build\codex-msvc-relwithdebinfo\bin\zr_vm_cli.exe E:\Git\zr_vm\tests\fixtures\projects\hello_world\hello_world.zrp
```

- smoke result:
  - prints `hello world`

- benchmark sandbox:
  - `%TEMP%\codex-matrix-add-2d-nextcut`
- benchmark compile command:

```powershell
E:\Git\zr_vm\build\codex-msvc-relwithdebinfo\bin\zr_vm_cli.exe --compile %TEMP%\codex-matrix-add-2d-nextcut\benchmark_matrix_add_2d.zrp --intermediate
```

- benchmark run result:
  - RelWithDebInfo wall-time series: `25,28,23,23,24 ms`

Acceptance decision for this slice:

- accepted as a follow-up micro-cut on top of the current-repo runtime baseline
- kept changes are:
  - exact reserved pair-span preference for dense super-array batch fill/append
  - stack-direct plain-destination arithmetic bodies that only materialize shared `destination` on fallback
- the temporary `MUL_SIGNED_PLAIN_DEST` null-destination regression was reproduced and fixed before acceptance
- no new correctness regression was observed in:
  - WSL gcc fresh benchmark run
  - WSL clang fresh benchmark run
  - Windows MSVC CLI smoke
  - Windows MSVC RelWithDebInfo benchmark run

## 2026-04-11 super-array cached-get plus 4-way prepare fast path

Scope:

- continue pushing the remaining `execution_dispatch.c` and `object_super_array` hot boundary without widening semantics
- specifically target:
  - `SUPER_ARRAY_GET_INT_PLAIN_DEST` fetch/self cost
  - `object_super_array_prepare_append_plans4_from_stack_assume_fast`
  - exact-span batch append setup overhead that still ran before the exact path was known to be usable

Kept implementation:

- `zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h`
  - split out `zr_super_array_try_resolve_items_cached_only_assume_fast()`
  - added `zr_super_array_store_plain_get_from_items_object_assume_fast()`
  - rewired `SUPER_ARRAY_GET_INT_PLAIN_DEST` to hit the cached-items direct read path before falling back to the existing slow-resolve helper
- `zr_vm_core/src/zr_vm_core/object/object_super_array.c`
  - added `object_super_array_try_prepare_append_plans4_cached_from_stack_assume_fast()`
  - `object_super_array_prepare_append_plans4_from_stack_assume_fast()` now first does one cached-only 4-way resolve/length/capacity pass and only falls back to the old 4x single-plan path on cache miss
  - `object_add_int_int_pairs4_assuming_absent_dense_ready_assume_fast()` no longer initializes batch cursors before the exact reserved-span path is proven to miss

Explicitly rejected and reverted in the same session because callgrind regressed:

- forcing `FAST_PREPARE_STACK_DESTINATION()` in the `SUPER_ARRAY_GET_INT_PLAIN_DEST` fast label
- a cached-only direct rewrite of `SUPER_ARRAY_SET_INT`
- a 4-way dense pair write helper for the commit path

WSL gcc validation:

- build directory:
  - `/home/hejiahui/codex-builds/zr_vm-gcc-relwithdebinfo`
- fresh sandbox:
  - `/tmp/matrix_add_2d_nextcut3`
- benchmark result:
  - checksum: `76802768`
  - wall-time series: `0.01, 0.01, 0.01 s`
- callgrind output:
  - `/home/hejiahui/codex-builds/callgrind.matrix_add_2d.gcc.next-cut6.out`
- callgrind summary:
  - total `Ir`: `30,529,665`
  - `execution_dispatch.c:ZrCore_Execute`: `19,505,550`
  - `object_super_array_internal.h:ZrCore_Execute`: `1,425,409`

Delta against the accepted current-repo cut above (`next-cut2`):

- previous total `Ir`: `30,636,316`
- previous `execution_dispatch.c:ZrCore_Execute`: `19,591,587`
- previous `object_super_array_internal.h:ZrCore_Execute`: `1,536,001`
- current delta:
  - total `Ir`: `-106,651`
  - `execution_dispatch.c:ZrCore_Execute`: `-86,037`
  - `object_super_array_internal.h:ZrCore_Execute`: `-110,592`

Interpretation:

- the effective win came from shrinking the inlined super-array helper path rather than from increasing dispatch-side fetch machinery
- `SUPER_ARRAY_GET_INT_PLAIN_DEST` now keeps the fast cached-items read entirely on the hot path while preserving the old slow-resolve fallback
- the 4-way prepare cut is safe and retained, but its benefit is folded into the lower helper total instead of surfacing as a standalone top-level symbol
- exact-span append still dominates the fill path; the profitable cut here was to stop paying cursor-setup cost before knowing the exact path would miss

WSL clang validation:

- build directory:
  - `/home/hejiahui/codex-builds/zr_vm-clang-relwithdebinfo`
- fresh sandbox:
  - `/tmp/matrix_add_2d_clang_nextcut6`
- benchmark result:
  - checksum: `76802768`
  - wall-time series: `0.01, 0.01, 0.02 s`

Windows MSVC validation:

- build directory:
  - `E:\Git\zr_vm\build\codex-msvc-relwithdebinfo`
- CLI smoke result:
  - `hello world`
- benchmark result:
  - checksum: `76802768`
  - repo-path RelWithDebInfo wall-time series: `30.391, 32.109, 29.605, 29.297, 36.072 ms`
- note:
  - a temp-directory stopwatch series showed large outliers (`137-143 ms`), so the repo-path series above is the only Windows timing kept as reference evidence for this slice

Acceptance decision for this slice:

- accepted
- kept changes are:
  - cached-only super-array item resolution helper reuse
  - direct cached read path for `SUPER_ARRAY_GET_INT_PLAIN_DEST`
  - cached-only 4-way append-plan preparation fast path
  - exact-span-first append setup that avoids speculative cursor initialization
- reverted experiments are intentionally not kept because they made callgrind worse
