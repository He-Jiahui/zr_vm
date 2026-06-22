# AOT M1.5 07-S2 I64 Compare Bool Result Local-Only

Date: 2026-06-21 06:23:48 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice removes frame destination/result materialization from the focused signed i64 compare bool results when both operands are already written signed scalar locals and every reachable later consumer can use the maintained bool scalar local.

For the focused typed scalar source, the signed i64 compare bool results:

- `dstSlot=27`, `leftSlot=2`, `rightSlot=4`
- `dstSlot=7`, `leftSlot=2`, `rightSlot=4`
- generated expressions `zr_aot_b27 = (TZrBool)(zr_aot_s2 > zr_aot_s4);` and `zr_aot_b7 = (TZrBool)(zr_aot_s2 > zr_aot_s4);`

now emit only scalar-local assignments. The generated compare blocks no longer declare `SZrTypeValue *zr_aot_destination`, create `zr_aot_s_result`, or write bool payloads through `frame.slotBase[27].value` or `frame.slotBase[7].value`.

Affected layers:

- AOT C scalar signed compare emission
- focused generated-product parser/AOT fixture
- AOT semantic contract documentation and plan records

## Baseline

Before this slice, the focused generated C emitted frame-backed destination blocks for signed i64 compare bool results. The RED test failed because the generated product still contained:

- `/* zr_aot_scalar_exec_i64_compare semirOpcode=36 dstSlot=27 leftSlot=2 rightSlot=4 */`
- `SZrTypeValue *zr_aot_destination = ZR_NULL;`
- destination frame assignment through `frame.slotBase[27].value`
- assignment through `zr_aot_s_result`
- destination bool frame payload materialization

The same old shape also existed for `dstSlot=7`.

Existing repository baseline:

- The worktree is dirty with unrelated parser/runtime/docs/build-output changes.
- `git diff --check` reports LF/CRLF conversion warnings from the existing dirty worktree but exits 0.
- `backend_aot_c_scalar_locals.c` is above the modularization threshold; this slice keeps using the existing result-skip proof boundary rather than mixing in a structural split.

## Test Inventory

- Focused subsystem case: `tests/parser/test_aot_c_typed_scalar.c` verifies generated C for both signed i64 compare bool results.
- Generated-product boundary: inspected `build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c` for the focused blocks.
- Source-contract target: `zr_vm_aot_c_source_contracts_test`.
- Registered CTest case: `aot_c_typed_scalar`.
- Negative cases: forbidden generated C tokens for the old i64 compare destination pointer, bool result temporary, and destination frame payload materialization for `dstSlot=27` and `dstSlot=7`.

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

- RED before implementation: failed 1/1 because the generated C still contained the old `dstSlot=27` i64 compare destination block at byte offset 85035.
- GREEN after implementation: direct run reports `1 Tests 0 Failures 0 Ignored OK`.

Generated C evidence:

```c
/* zr_aot_scalar_exec_i64_compare semirOpcode=36 dstSlot=27 leftSlot=2 rightSlot=4 */
zr_aot_b27 = (TZrBool)(zr_aot_s2 > zr_aot_s4);

/* zr_aot_scalar_exec_i64_compare semirOpcode=36 dstSlot=7 leftSlot=2 rightSlot=4 */
zr_aot_b7 = (TZrBool)(zr_aot_s2 > zr_aot_s4);
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

- The RED generated-product assertion first proved the old i64 compare destination/result materialization was still present.
- `backend_aot_write_c_scalar_i64_compare()` now uses signed written-before proof and `backend_aot_c_scalar_locals_bool_result_can_skip_value_slot()` before skipping frame materialization.
- The focused generated C now contains only `zr_aot_b27 = (TZrBool)(zr_aot_s2 > zr_aot_s4);` and `zr_aot_b7 = (TZrBool)(zr_aot_s2 > zr_aot_s4);` for the two i64 compare bool result blocks.
- The focused typed scalar executable, source-contract target, and registered CTest pass.

## Acceptance Decision

Accepted for the focused 07-S2 i64 compare bool result local-only slice.

This is not full 07-S2 and not full M1.5/07 completion. Remaining work includes other scalar result materialization, primitive constant frame writes, generic float copy/type checks, direct return/result frame fallbacks, prologue/frame setup, reset-stack-null frame writes, and boundary local restoration.

`backend_aot_c_scalar_locals.c` remains above the modularization threshold. The smallest follow-up split is extracting scalar result-skip/liveness proof helpers into a dedicated module.
