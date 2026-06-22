# AOT M1.5 / 07-S5 U64 Typed Direct-Call Smoke Support Split

- Timestamp: 2026-06-22 16:18:35 +08:00
- Status: support sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started
- Build: WSL Ubuntu-22.04 GCC debug, `build/codex-aot-07-wsl-gcc-debug`

## Scope

- Refactors the u64 typed direct-call shared-library smoke harness without adding typed thunk behavior.
- Adds `tests/parser/aot_c_typed_direct_call_u64_smoke_support.h`.
- Keeps `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c` focused on six case definitions plus `main()`.
- The support header owns project/source/binary path setup, source compilation, binary hash embedding, AOT C generation, generated-C needle checks, generated shared-library compilation, AOT runtime execution, and result assertions.
- Out of scope: new u64 thunk routes, generated C behavior changes, route-selection changes, and full 07-S5 completion.

## Baseline

- Before this slice, `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c` had grown to 909 physical / 842 non-empty lines.
- Continuing u64 route work in that shape would push the focused smoke further into large-file territory.
- The already accepted behavior cases were:
  - no-arg u64 constant typed thunk
  - one-arg u64 identity typed thunk
  - one-arg u64 add-constant typed thunk
  - one-arg u64 subtract-constant typed thunk
  - two-arg u64 add typed thunk
  - two-arg u64 subtract typed thunk

## Test Inventory

- Focused smoke: `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`.
- Shared helper: `tests/parser/aot_c_typed_direct_call_u64_smoke_support.h`.
- Preserved checks:
  - Generated thunk forward declaration needle.
  - Generated thunk definition needle.
  - Generated return-expression needle.
  - Direct-call marker needle.
  - Direct call expression needle.
  - Absence of typed destination sync marker.
  - Absence of `CallStaticDirect` and `CallStackValue`.
  - Runtime result 42 for each case.

## Tooling Evidence

Focused command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test'
```

Broader focused AOT command:

```powershell
wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_type_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test'
```

## Results

- Focused u64 typed direct-call smoke: 6 tests, 0 failures.
- Broader focused AOT group:
  - source contracts 19/0
  - call contracts 8/0
  - shared-library smoke 8/0
  - call smoke 5/0
  - typed direct-call smoke 5/0
  - bool smoke 2/0
  - u64 smoke 6/0
  - f64 smoke 11/0
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

File sizes after this slice:

- `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`: 151 physical / 138 non-empty lines.
- `tests/parser/aot_c_typed_direct_call_u64_smoke_support.h`: 211 physical / 191 non-empty lines.

## Acceptance Decision

- Accepted for this support sub-slice only.
- The six u64 typed direct-call smoke behaviors remain behavior-equivalent after the split.
- 07-S5 remains partial. 07, M1.5, and AOT stages 08-12 remain incomplete.
