# AOT M1.5 / 07-S5 Static U64 One-Arg Add-Const Typed Thunk

- Timestamp: 2026-06-22 12:47:02 +08:00
- Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started
- Build: WSL Ubuntu-22.04 GCC debug, `build/codex-aot-07-wsl-gcc-debug`

## Scope

- Adds a one-parameter unsigned integer add-constant typed thunk direct-call route.
- Affected layers: AOT C typed thunk recognition/emission, AOT static direct-call generated C contracts, and shared-library AOT smoke execution.
- Covered source shape: `func inc(value: uint): uint { return value + 1; }`.
- Covered SemIR shape: parameter-slot copy, `GET_CONSTANT`, `TO_INT`, `ADD_SIGNED` or `ADD_SIGNED_PLAIN_DEST`, and `FUNCTION_RETURN`.
- Generated thunk ABI remains `static TZrUInt64 zr_aot_typed_u64_fn_N(struct SZrState *state, TZrUInt64 zr_aot_arg0)`.
- Generated thunk body returns `(TZrUInt64)(zr_aot_arg0 + (TZrUInt64)constant)`.
- Out of scope: u64 multi-arg routes, arbitrary u64 expression trees, negative constants, f64 typed routes, inline structs, `in` / `out` writeback, deopt/dynamic bridge boxing, and full 07-S5 completion.

## Baseline

- RED baseline: `zr_vm_aot_c_call_contracts_test` failed because the u64 typed thunk source had no `backend_aot_c_try_get_u64_arg0_add_constant_return(` contract.
- Intermediate baseline: the u64 smoke showed that current `uint + 1` lowering uses `TO_INT` plus signed-add opcodes rather than unsigned-add opcodes, so the recognizer had to cover the emitted SemIR shape.
- Repository baseline: standard target relink was blocked by unrelated `type_inference` unresolved symbols in the shared checkout. This acceptance does not claim whole-repository build health.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_call_contracts.c`.
- Integration smoke: `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`.
- Boundary cases:
  - Direct C call route for one `uint` argument.
  - Non-negative constant addition through current signed-add SemIR lowering.
  - Runtime result 42 from `seed = 36`, `inc(seed)`, then `value + 5`.
  - Direct-call marker present and `CallStaticDirect` / `CallStackValue` markers absent.
  - Typed destination sync marker absent when scalar-local proof can satisfy later consumers.
- Negative cases:
  - Recognizer rejects missing unsigned callable return metadata.
  - Recognizer rejects missing one unsigned parameter metadata.
  - Recognizer rejects varargs.
  - Recognizer rejects negative signed constants.

## Tooling Evidence

Tooling:

- WSL Ubuntu-22.04 GCC debug build.
- `cmake --build` for the focused test targets.
- Direct binary execution for contract and smoke tests.
- Manual `gcc` link with `--allow-shlib-undefined` only for the u64 smoke executable because unrelated `type_inference` symbols blocked normal shared-library relink.

RED command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
```

Focused GREEN commands:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test'
```

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && /usr/bin/gcc -fPIC -g build/codex-aot-07-wsl-gcc-debug/tests/CMakeFiles/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test.dir/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c.o build/codex-aot-07-wsl-gcc-debug/tests/CMakeFiles/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test.dir/harness/runtime_support.c.o build/codex-aot-07-wsl-gcc-debug/tests/CMakeFiles/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test.dir/harness/path_support.c.o build/codex-aot-07-wsl-gcc-debug/tests/CMakeFiles/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test.dir/harness/unity_crash_guard.c.o build/codex-aot-07-wsl-gcc-debug/tests/CMakeFiles/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test.dir/harness/reference_support.c.o -o build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -Wl,--allow-shlib-undefined -Wl,-rpath,/mnt/e/Git/zr_vm/build/codex-aot-07-wsl-gcc-debug/lib build/codex-aot-07-wsl-gcc-debug/lib/libzr_unity.a build/codex-aot-07-wsl-gcc-debug/lib/libzr_vm_parser.so build/codex-aot-07-wsl-gcc-debug/lib/libzr_vm_lib_math.so build/codex-aot-07-wsl-gcc-debug/lib/libzr_vm_lib_system.so build/codex-aot-07-wsl-gcc-debug/lib/libzr_vm_lib_container.so build/codex-aot-07-wsl-gcc-debug/lib/libzr_vm_lib_ffi.so -ldl build/codex-aot-07-wsl-gcc-debug/lib/libzr_vm_library.so build/codex-aot-07-wsl-gcc-debug/lib/libzr_vm_core.so && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
```

Broader focused AOT command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && for t in zr_vm_aot_c_source_contracts_test zr_vm_aot_c_call_contracts_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_call_shared_library_smoke_test zr_vm_aot_c_typed_direct_call_shared_library_smoke_test zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test zr_vm_aot_c_typed_scalar_test zr_vm_aot_c_value_type_shared_library_smoke_test zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_global_shared_library_smoke_test zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_value_semir_contracts_test; do ./build/codex-aot-07-wsl-gcc-debug/bin/$t || exit 1; done'
```

## Results

- `zr_vm_aot_c_call_contracts_test`: 7 tests, 0 failures.
- `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`: 3 tests, 0 failures.
- Broader focused AOT group:
  - source contracts 19/0
  - call contracts 7/0
  - shared-library smoke 8/0
  - call smoke 5/0
  - typed direct-call smoke 5/0
  - bool smoke 2/0
  - u64 smoke 3/0
  - arithmetic smoke 5/0
  - bitwise smoke 6/0
  - typed scalar 1/0
  - value-type smoke 1/0
  - generic numeric smoke 1/0
  - global smoke 9/0
  - logical contracts 4/0
  - power smoke 1/0
  - frame setup contracts 1/0
  - return contracts 1/0
  - value SemIR contracts 4/0

Implementation files after this slice:

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunks.c`: 292 physical / 256 non-empty lines.
- `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`: 454 physical / 420 non-empty lines.
- `tests/parser/test_aot_c_call_contracts.c`: 735 physical / 683 non-empty lines.

## Acceptance Decision

- Accepted for this narrow 07-S5 sub-slice only.
- The accepted behavior is one-argument u64 direct typed calls where the callee returns `arg0 + non_negative_constant`.
- 07-S5 remains partial. 07, M1.5, and AOT stages 08-12 remain incomplete.
- Remaining risk: the standard target relink needs the unrelated `type_inference` unresolved-symbol worktree state to be repaired before this can be used as whole-checkout build evidence.
