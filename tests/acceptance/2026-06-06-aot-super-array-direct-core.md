# AOT Super-Array Direct Core Lowering

## Summary

Generated AOT C now lowers the integer super-array opcode family through direct core fast-path APIs instead of instruction helper wrappers:

- `SUPER_ARRAY_GET_INT`
- `SUPER_ARRAY_GET_INT_PLAIN_DEST`
- `SUPER_ARRAY_SET_INT`
- `SUPER_ARRAY_ADD_INT`
- `SUPER_ARRAY_ADD_INT4`
- `SUPER_ARRAY_ADD_INT4_CONST`
- `SUPER_ARRAY_FILL_INT4_CONST`

The C backend resolves operands from `frame.slotBase`, validates slot ranges and signed integer operands/constants, then calls `ZrCore_Object_SuperArray*Fast` / `*AssumeFast` APIs declared in `zr_vm_core/object.h`. Unsupported or malformed shapes report `unsupported AOT super-array integer fast path` and fail through `ZR_AOT_C_FAIL()`.

This is intentionally a direct core semantic boundary, not raw array storage mutation: the super-array object owns hash/node storage, length/capacity state, and object invariants. It is also intentionally not the generic `ZrCore_Object_SuperArrayGetInt` / `SetInt` / `AddInt` wrapper path, because those wrappers can fall back to dynamic member/index behavior.

## Contract

- Generated modules include `zr_vm_core/object.h`.
- `backend_aot_c_function_body.c` routes the supported super-array integer opcodes to `backend_aot_write_c_direct_super_array_*` emitters.
- `backend_aot_c_lowering_super_array.c` emits direct fast core calls and an explicit unsupported fast-path failure block.
- C backend generated source no longer emits `ZrLibrary_AotRuntime_SuperArray*` for this opcode family.
- Generated C no longer emits generic `ZrCore_Object_SuperArrayGetInt(state, ...)`, `SetInt(state, ...)`, or `AddInt(state, ...)` calls for this opcode family.
- LLVM parity and wider dynamic super-array shapes remain future work.

## RED Evidence

The source contract was added first and failed before the lowering module existed:

```text
wsl sh -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build-wsl-gcc >/tmp/zr_vm_cmake_super_array_red.log && cmake --build build-wsl-gcc --target zr_vm_aot_c_super_array_contracts_test -j 3 && ./build-wsl-gcc/bin/zr_vm_aot_c_super_array_contracts_test'
```

Result:

```text
tests/parser/test_aot_c_super_array_contracts.c:191: FAIL: Expected Non-NULL
```

## GREEN Evidence

Focused GCC validation:

```text
wsl sh -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_super_array_contracts_test -j 3 && ./build-wsl-gcc/bin/zr_vm_aot_c_super_array_contracts_test'
wsl sh -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_parser_shared -j 1'
wsl sh -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_super_array_shared_library_smoke_test -j 1'
wsl sh -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_super_array_shared_library_smoke_test'
```

Result: source contract passed with 1 test and 0 failures; parser shared built; generated-C super-array smoke passed with 1 test and 0 failures.

Focused Clang validation:

```text
wsl sh -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-clang/bin/zr_vm_aot_c_super_array_contracts_test'
wsl sh -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_super_array_shared_library_smoke_test -j 1'
wsl sh -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-clang/bin/zr_vm_aot_c_super_array_shared_library_smoke_test'
```

Result: source contract passed with 1 test and 0 failures; generated-C super-array smoke passed with 1 test and 0 failures. Existing unrelated Clang warnings remain in broader parser/core files.

Focused MSVC validation:

```text
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; cmake -S . -B build-msvc; cmake --build build-msvc --target zr_vm_aot_c_super_array_contracts_test --config Debug; .\build-msvc\bin\Debug\zr_vm_aot_c_super_array_contracts_test.exe
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; cmake --build build-msvc --target zr_vm_aot_c_super_array_shared_library_smoke_test --config Debug; .\build-msvc\bin\Debug\zr_vm_aot_c_super_array_shared_library_smoke_test.exe
```

Result: source contract passed with 1 test and 0 failures; the generated-C smoke target built under MSVC and reported 1 ignored Unix-only shared-library runtime test, as expected.

Source and generated-C scans found no C backend/generated-C super-array helper emission:

```text
Select-String -Path backend_aot_c_function_body.c,backend_aot_c_lowering_super_array.c -Pattern 'ZrLibrary_AotRuntime_SuperArray|ZrCore_Object_SuperArray(GetInt|SetInt|AddInt)\(state'
wsl sh -lc 'cd /mnt/e/Git/zr_vm && find build-wsl-gcc/tests_generated -path "*aot_c_super_array_smoke.c" -print -exec grep -n "ZrLibrary_AotRuntime_SuperArray\|ZrCore_Object_SuperArray" {} \;'
```

Result: no old runtime helper wrapper emission in the checked C backend path; the generated smoke source contains only the direct fast core super-array calls.
