# AOT M1.5 07-S2 I64 Shift Result Local-Only

Date: 2026-06-21 05:57:20 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice removes frame destination/result materialization from the focused signed i64 shift result when every reachable later consumer can use the maintained signed scalar local.

For the focused typed scalar source, the first signed i64 shift result:

- `dstSlot=19`
- `leftSlot=15`
- `rightSlot=1`
- generated expression `zr_aot_s19 = (TZrInt64)((TZrUInt64)zr_aot_s15 << zr_aot_s1);`

now keeps the signed shift-count range guard and emits only the scalar-local assignment. The generated shift block no longer declares `SZrTypeValue *zr_aot_destination`, creates `zr_aot_s_result`, or writes the result payload through `frame.slotBase[19].value`.

The shift emitter reuses the signed i64 result-skip proof introduced for i64 binary/bitwise results. It only skips frame materialization when both signed sources are proven written C locals, the destination has a signed scalar local, and reachable later consumers can read the signed scalar local.

Affected layers:

- AOT C scalar shift emission
- focused generated-product parser/AOT fixture
- AOT semantic contract documentation and plan records

## Baseline

Before this slice, the focused generated C emitted a frame-backed destination block for the first signed i64 shift result. The RED test failed because the generated product still contained:

- `/* zr_aot_scalar_exec_i64_shift semirOpcode=45 dstSlot=19 leftSlot=15 rightSlot=1 */`
- `SZrTypeValue *zr_aot_destination = ZR_NULL;`
- assignment through `zr_aot_s_result`
- destination frame payload materialization

Existing repository baseline:

- The worktree is dirty with unrelated parser/runtime/docs/build-output changes.
- `backend_aot_c_scalar_locals.c` is above the modularization threshold; this slice keeps using the existing result-skip proof boundary rather than mixing in a structural split.

## Test Inventory

- Focused subsystem case: `tests/parser/test_aot_c_typed_scalar.c` verifies generated C for the signed i64 shift result.
- Generated-product boundary: inspected `build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c` for the focused block.
- Source-contract target: `zr_vm_aot_c_source_contracts_test`.
- Registered CTest case: `aot_c_typed_scalar`.
- Negative case: forbidden generated C tokens for the old i64 shift destination/result materialization.
- Boundary case: signed shift-count range guard remains in the local-only block.

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

- RED before implementation: failed 1/1 because the generated C still contained the old i64 shift destination block.
- GREEN after implementation: `1 Tests 0 Failures 0 Ignored OK`.

Generated C evidence:

```c
/* zr_aot_scalar_exec_i64_shift semirOpcode=45 dstSlot=19 leftSlot=15 rightSlot=1 */
if (ZR_UNLIKELY(zr_aot_s1 < 0 || zr_aot_s1 >= 64)) {
    ZrCore_Debug_RunError(state, "generated AOT scalar shift count out of range");
    ZR_AOT_C_FAIL();
}
zr_aot_s19 = (TZrInt64)((TZrUInt64)zr_aot_s15 << zr_aot_s1);
```

Source-contract target:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test"
```

- GREEN: target was up to date and the test binary reported 19 tests, 0 failures.

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

- The RED generated-product assertion first proved the old i64 shift destination/result materialization was still present.
- `backend_aot_write_c_scalar_i64_shift()` now skips frame result materialization only after source locals are proven written and `backend_aot_c_scalar_locals_i64_result_can_skip_value_slot()` proves reachable consumers can use the signed scalar local.
- The focused generated C now keeps the range guard and contains only `zr_aot_s19 = (TZrInt64)((TZrUInt64)zr_aot_s15 << zr_aot_s1);` for the first i64 shift result assignment.
- The focused typed scalar executable, source-contract target, and registered CTest pass.

## Acceptance Decision

Accepted for the focused 07-S2 i64 shift result local-only slice.

This is not full 07-S2 and not full M1.5/07 completion. Remaining work includes other scalar result materialization, primitive constant frame writes, generic float copy/type checks, direct return/result frame fallbacks, prologue/frame setup, reset-stack-null frame writes, and boundary local restoration.

`backend_aot_c_scalar_locals.c` remains above the modularization threshold. The smallest follow-up split is extracting scalar result-skip/liveness proof helpers into a dedicated module.
