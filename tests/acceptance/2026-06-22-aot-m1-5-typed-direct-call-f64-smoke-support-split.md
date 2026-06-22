# AOT M1.5 / 07-S5 F64 Typed Direct-Call Smoke Support Split

- Timestamp: 2026-06-22 14:46:01 +08:00
- Status: support slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started
- Build: WSL Ubuntu-22.04 GCC debug, `build/codex-aot-07-wsl-gcc-debug`

## Scope

- Extracts the common f64 typed direct-call shared-library smoke harness into `tests/parser/aot_c_typed_direct_call_f64_smoke_support.h`.
- Keeps the concrete `tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c` focused on case definitions and test registration.
- Adds no new typed thunk behavior.

## Refactor Shape

- New `SZrAotTypedDirectCallF64SmokeCase` captures source text, artifact paths, generated-C needles, sync-marker absence, and expected result.
- New `run_aot_c_typed_direct_call_f64_smoke()` owns source compilation, project/source/binary artifact writing, zro hashing, AOT C generation, generated-C assertions, generated shared-library compilation, AOT runtime execution, and result assertions.
- Concrete smoke file now contains five cases:
  - f64 no-arg constant return
  - f64 one-arg identity return
  - f64 two-arg add return
  - f64 two-arg subtract return
  - f64 two-arg multiply return

## Validation

Focused command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test'
```

Focused result:

- `zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test`: 5 tests, 0 failures.

Broader focused AOT command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_type_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test'
```

Broader focused AOT result:

- source contracts 19/0
- call contracts 8/0
- shared-library smoke 8/0
- call smoke 5/0
- typed direct-call smoke 5/0
- bool smoke 2/0
- u64 smoke 5/0
- f64 smoke 5/0
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

File sizes after split:

- `tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c`: 128 physical / 117 non-empty lines.
- `tests/parser/aot_c_typed_direct_call_f64_smoke_support.h`: 211 physical / 191 non-empty lines.

## Acceptance Decision

- Accepted as a behavior-preserving support refactor.
- 07-S5 remains partial. 07, M1.5, and AOT stages 08-12 remain incomplete.
