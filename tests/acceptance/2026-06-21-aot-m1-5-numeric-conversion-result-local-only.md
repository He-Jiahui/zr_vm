# AOT M1.5 07-S2 Numeric Conversion Result Local-Only

Date: 2026-06-21 06:54:52 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice removes frame destination/result materialization from the focused typed numeric conversion results when the source is already a written scalar local, the destination has the matching scalar local, and every reachable later consumer can use or kill that scalar local.

For the focused typed scalar source, these conversion results now emit only scalar-local assignments:

- `TO_INT_FLOAT`, `dstSlot=31`, `srcSlot=19`: `zr_aot_s31 = (TZrInt64)zr_aot_f19;`
- `TO_INT_UNSIGNED`, `dstSlot=31`, `srcSlot=8`: guarded signed wrap directly into `zr_aot_s31`
- `TO_UINT_SIGNED`, `dstSlot=31`, `srcSlot=2`: `zr_aot_u31 = (TZrUInt64)zr_aot_s2;`
- `TO_UINT_FLOAT`, `dstSlot=31`, `srcSlot=19`: `zr_aot_u31 = (TZrUInt64)zr_aot_f19;`

The generated conversion blocks no longer declare `SZrTypeValue *zr_aot_destination`, create `zr_aot_s_result` / `zr_aot_u_result`, or write integer payloads through `frame.slotBase[31].value`.

Affected layers:

- AOT C scalar conversion emission
- i64/u64 scalar-local liveness/result-skip proof reuse
- focused generated-product parser/AOT fixture
- AOT semantic contract documentation and plan records

## Baseline

Before this slice, the focused generated C already reused f64/u64/i64 source locals for the conversion expressions, but still materialized each result through the frame:

- `/* zr_aot_scalar_exec_to_i64 opcode=32 dstSlot=31 srcSlot=19 */`
- `/* zr_aot_scalar_exec_to_i64 opcode=33 dstSlot=31 srcSlot=8 */`
- `/* zr_aot_scalar_exec_to_u64 opcode=35 dstSlot=31 srcSlot=2 */`
- `/* zr_aot_scalar_exec_to_u64 opcode=34 dstSlot=31 srcSlot=19 */`
- `SZrTypeValue *zr_aot_destination = ZR_NULL;`
- assignment through `zr_aot_s_result` or `zr_aot_u_result`
- destination frame payload materialization

Existing repository baseline:

- The worktree is dirty with unrelated parser/runtime/docs/build-output changes.
- `git diff --check` reports LF/CRLF conversion warnings from the existing dirty worktree but exits 0.
- `backend_aot_c_scalar_locals.c` is above the modularization threshold; this slice reuses the existing result-skip proof boundary rather than mixing in a structural split.

## Test Inventory

- Focused subsystem case: `tests/parser/test_aot_c_typed_scalar.c` verifies generated C for the numeric conversion results.
- Generated-product boundary: inspected `build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c` for the focused blocks.
- Source-contract target: `zr_vm_aot_c_source_contracts_test`.
- Registered CTest case: `aot_c_typed_scalar`.
- Negative cases: forbidden generated C tokens for the old conversion destination pointer, result temporary, and destination frame payload materialization for `dstSlot=31`.

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

- RED before implementation: failed 1/1 because the generated C still contained the old `dstSlot=31` `TO_INT_FLOAT` destination block at byte offset 106770.
- GREEN after implementation: direct run reports `1 Tests 0 Failures 0 Ignored OK`.

Generated C evidence:

```c
/* zr_aot_scalar_exec_to_i64 opcode=32 dstSlot=31 srcSlot=19 */
zr_aot_s31 = (TZrInt64)zr_aot_f19;
/* zr_aot_scalar_exec_to_i64 opcode=33 dstSlot=31 srcSlot=8 */
{
    TZrUInt64 zr_aot_limit = (TZrUInt64)ZR_TYPE_RANGE_INT64_MAX + (TZrUInt64)1u;
    if (zr_aot_u8 >= zr_aot_limit) {
        zr_aot_s31 = ZR_TYPE_RANGE_INT64_MIN + (TZrInt64)(zr_aot_u8 - zr_aot_limit);
    } else {
        zr_aot_s31 = (TZrInt64)zr_aot_u8;
    }
}
/* zr_aot_scalar_exec_to_u64 opcode=35 dstSlot=31 srcSlot=2 */
zr_aot_u31 = (TZrUInt64)zr_aot_s2;
/* zr_aot_scalar_exec_to_u64 opcode=34 dstSlot=31 srcSlot=19 */
zr_aot_u31 = (TZrUInt64)zr_aot_f19;
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

- The RED generated-product assertion first proved the old numeric conversion destination/result materialization was still present.
- `backend_aot_write_c_scalar_to_i64()` now uses f64/u64 written-before proof plus `backend_aot_c_scalar_locals_i64_result_can_skip_value_slot()` before skipping frame materialization.
- `backend_aot_write_c_scalar_to_u64()` now uses i64/f64 written-before proof plus `backend_aot_c_scalar_locals_u64_result_can_skip_value_slot()` before skipping frame materialization.
- The focused generated C now contains only scalar-local conversion assignments for the targeted signed, unsigned, and float source conversions.
- The focused typed scalar executable, source-contract target, and registered CTest pass.

## Acceptance Decision

Accepted for the focused 07-S2 numeric conversion result local-only slice.

This is not full 07-S2 and not full M1.5/07 completion. Remaining work includes remaining scalar result materialization outside the focused conversions, primitive constant frame writes, generic float copy/type checks, direct return/result frame fallbacks, prologue/frame setup, reset-stack-null frame writes, and boundary local restoration.

`backend_aot_c_scalar_locals.c` remains above the modularization threshold. The smallest follow-up split is extracting scalar result-skip/liveness proof helpers into a dedicated module.
