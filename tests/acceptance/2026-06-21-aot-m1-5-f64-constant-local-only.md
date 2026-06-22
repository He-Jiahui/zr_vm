# AOT M1.5 07-S2 F64 Constant Local-Only

Date: 2026-06-21 07:31:11 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice removes frame destination writes from the focused f64 primitive constants when the
constant slot has an f64 scalar local and every reachable later use can consume or kill that local.

For the focused typed scalar source, the float constants now emit only scalar-local assignments:

- `slot 19`: `zr_aot_f19 = (TZrFloat64)1.5;`
- `slot 20`: `zr_aot_f20 = (TZrFloat64)2;`

The generated blocks no longer immediately materialize the same double payload through
`zr_aot_value_exec_primitive_constant` and `frame.slotBase[19/20].value`.

## Baseline

Before this slice, the focused f64 constants were mirrored into scalar locals inside the frame-backed
primitive constant block:

- `/* zr_aot_value_exec_primitive_constant */`
- destination assignment through `frame.slotBase[19].value` / `frame.slotBase[20].value`
- double payload write
- trailing `zr_aot_f19 = (TZrFloat64)1.5;` / `zr_aot_f20 = (TZrFloat64)2;`

Existing repository baseline:

- The worktree is dirty with unrelated parser/runtime/docs/build-output changes.
- `git diff --check` reports LF/CRLF conversion warnings from the existing dirty worktree but exits 0.
- `backend_aot_c_scalar_locals.c` is above the modularization threshold; this slice reuses the
  existing constant/result liveness boundary rather than mixing in a structural split.

## Test Inventory

- Focused subsystem case: `tests/parser/test_aot_c_typed_scalar.c` verifies generated C for the
  two focused f64 primitive constants.
- Generated-product boundary: inspected
  `build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c`.
- Source-contract target: `zr_vm_aot_c_source_contracts_test`.
- Registered CTest case: `aot_c_typed_scalar`.
- Negative cases: forbidden generated C tokens for the old primitive-constant double frame writes
  for slots 19 and 20.

## Tooling Evidence

Tooling:

- WSL GCC Debug build directory: `build/codex-aot-07-wsl-gcc-debug`
- CMake/Ninja target rebuild for focused typed scalar and source contracts
- Direct generated C inspection
- CTest focused filter
- `git diff --check`

Commands and key outputs:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"
```

- RED before implementation: failed 1/1 because the generated C lacked
  `/* zr_aot_scalar_constant_f64_local */` for `zr_aot_f19 = (TZrFloat64)1.5;`.
- GREEN after implementation: direct run reports `1 Tests 0 Failures 0 Ignored OK`.

Generated C evidence:

```c
/* zr_aot_scalar_constant_f64_local */
zr_aot_f19 = (TZrFloat64)1.5;
/* zr_aot_scalar_constant_f64_local */
zr_aot_f20 = (TZrFloat64)2;
/* zr_aot_scalar_exec_f64_binary semirOpcode=29 dstSlot=32 leftSlot=19 rightSlot=20 */
zr_aot_f32 = zr_aot_f19 * zr_aot_f20;
```

Source-contract target:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test"
```

- GREEN: target linked successfully and the test binary reported 19 tests, 0 failures.

Registered CTest:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm/build/codex-aot-07-wsl-gcc-debug && ctest -R 'aot_c_typed_scalar' --output-on-failure"
```

- GREEN: 1/1 tests passed.

Formatting check:

```powershell
git diff --check
```

- GREEN: exit code 0; output only reported existing LF/CRLF conversion warnings.

## Results

- The RED generated-product assertion first proved f64 constants did not have a standalone
  scalar-local constant block.
- `backend_aot_write_c_direct_primitive_constant()` now emits `zr_aot_scalar_constant_f64_local`
  before the frame fallback, and uses `backend_aot_c_scalar_locals_f64_constant_can_skip_value_slot()`
  to skip the frame write when reachable consumers can use or kill the f64 local.
- The focused generated C now contains only scalar-local f64 assignments for slots 19 and 20, and the
  later f64 binary consumes those locals directly.
- The focused typed scalar executable, source-contract target, and registered CTest pass.

## Acceptance Decision

Accepted for the focused 07-S2 f64 primitive constant local-only slice.

This is not full 07-S2 and not full M1.5/07 completion. Remaining work includes remaining primitive
constants outside the focused proof, generic float copy/type checks, direct return/result frame
fallback removal, prologue/frame setup, reset-stack-null frame writes, and boundary local restoration.

`backend_aot_c_scalar_locals.c` remains above the modularization threshold. The smallest follow-up
split is extracting scalar result-skip/liveness proof helpers into a dedicated module.
