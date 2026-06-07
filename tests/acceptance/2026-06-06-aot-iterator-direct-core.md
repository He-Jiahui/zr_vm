# AOT Iterator Direct Core Lowering

## Summary

Generated AOT C now lowers the iterator opcode family through direct core iterator APIs instead of instruction helper wrappers:

- `ITER_INIT`
- `DYN_ITER_INIT`
- `ITER_MOVE_NEXT`
- `DYN_ITER_MOVE_NEXT`
- `ITER_CURRENT`
- `SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE`

The C backend resolves operands from `frame.slotBase`, validates slot ranges, and calls the iterator semantic APIs declared in `zr_vm_core/object.h`: `ZrCore_Object_IterInit`, `ZrCore_Object_TryIterMoveNextCachedArrayFast`, `ZrCore_Object_IterMoveNext`, `ZrCore_Object_TryIterCurrentCachedMemberFastStackResult`, and `ZrCore_Object_IterCurrent`. The fused move-next false branch validates that the move-next result is bool and emits the generated branch `goto` directly.

This is intentionally a direct core semantic boundary, not raw iterator storage mutation. Unsupported or malformed iterator shapes report `unsupported AOT iterator core path` or `unsupported AOT iterator branch result` and fail through `ZR_AOT_C_FAIL()`.

## Contract

- `backend_aot_c_function_body.c` routes the iterator opcodes to `backend_aot_write_c_direct_iter_*` emitters.
- `backend_aot_c_lowering_iterators.c` emits direct core iterator calls and explicit unsupported failure blocks.
- C backend generated source no longer emits `ZrLibrary_AotRuntime_IterInit`, `ZrLibrary_AotRuntime_IterMoveNext`, or `ZrLibrary_AotRuntime_IterCurrent` for this opcode family.
- The fused iterator false branch no longer emits `ZrLibrary_AotRuntime_IsTruthy`; it checks the bool move-next result directly.
- LLVM parity and broader source-level generated iterator execution fixtures remain future work.

## RED Evidence

The source contract was added first and failed before the lowering module existed:

```text
wsl sh -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_iterator_contracts_test -j 2 && ./build-wsl-gcc/bin/zr_vm_aot_c_iterator_contracts_test'
```

Result:

```text
tests/parser/test_aot_c_iterator_contracts.c:163:test_aot_c_source_lowers_iterator_ops_to_direct_core_calls:FAIL: Expected Non-NULL
1 Tests 1 Failures 0 Ignored
```

## GREEN Evidence

Focused GCC validation:

```text
wsl sh -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_iterator_contracts_test -j 2 && ./build-wsl-gcc/bin/zr_vm_aot_c_iterator_contracts_test'
wsl sh -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_parser_shared -j 1'
wsl sh -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build-wsl-gcc >/tmp/zr_vm_cmake_iter_green.log && cmake --build build-wsl-gcc --target zr_vm_aot_c_iterator_shared_library_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_iterator_shared_library_smoke_test'
```

Result: source contract passed with 1 test and 0 failures; parser shared built and compiled `backend_aot_c_lowering_iterators.c`; generated-C iterator smoke passed with 1 test and 0 failures.

Focused Clang validation:

```text
wsl sh -lc 'cd /mnt/e/Git/zr_vm && CC=clang CXX=clang++ cmake -S . -B build-wsl-clang >/tmp/zr_vm_cmake_iter_clang.log && cmake --build build-wsl-clang --target zr_vm_parser_shared -j 1'
wsl sh -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_iterator_contracts_test -j 2 && ./build-wsl-clang/bin/zr_vm_aot_c_iterator_contracts_test'
wsl sh -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_iterator_shared_library_smoke_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_iterator_shared_library_smoke_test'
```

Result: parser shared built; source contract passed with 1 test and 0 failures; generated-C iterator smoke passed with 1 test and 0 failures.

Focused MSVC validation:

```text
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build-msvc; cmake --build build-msvc --target zr_vm_aot_c_iterator_contracts_test --config Debug; .\build-msvc\bin\Debug\zr_vm_aot_c_iterator_contracts_test.exe; cmake --build build-msvc --target zr_vm_aot_c_iterator_shared_library_smoke_test --config Debug; .\build-msvc\bin\Debug\zr_vm_aot_c_iterator_shared_library_smoke_test.exe
```

Result: source contract passed with 1 test and 0 failures; the generated-C smoke target built under MSVC and reported 1 ignored Unix-only shared-library runtime test, as expected.

Source and generated-C scans found no C backend/generated-C iterator helper emission:

```text
Select-String -Path backend_aot_c_function_body.c,backend_aot_c_lowering_iterators.c -Pattern 'ZrLibrary_AotRuntime_IterInit|ZrLibrary_AotRuntime_IterMoveNext|ZrLibrary_AotRuntime_IterCurrent|ZrLibrary_AotRuntime_IsTruthy'
wsl sh -lc 'cd /mnt/e/Git/zr_vm && find build-wsl-gcc/tests_generated -path "*aot_c_iterator_smoke.c" -print -exec grep -n "ZrLibrary_AotRuntime_Iter\|ZrLibrary_AotRuntime_IsTruthy\|ZrCore_Object_Iter\|ZrCore_Object_TryIter" {} \;'
```

Result: no old runtime helper wrapper emission in the checked C backend path; the generated smoke source contains only direct core iterator calls and cached iterator fast paths.
