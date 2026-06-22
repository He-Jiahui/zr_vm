# AOT M1.5 07-S2 I64 Bit-Not Result Local-Only

Date: 2026-06-21 06:06:05 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice removes frame destination/result materialization from the focused signed i64 bit-not result when the source is already a written signed scalar local and every reachable later consumer can use the maintained signed scalar local.

For the focused typed scalar source, the signed i64 bit-not result:

- `dstSlot=19`
- `sourceSlot=1`
- generated expression `zr_aot_s19 = ~zr_aot_s1;`

now emits only the scalar-local assignment. The generated bit-not block no longer declares `SZrTypeValue *zr_aot_destination`, creates `zr_aot_s_result`, type-checks/reloads `frame.slotBase[1]`, or writes the result payload through `frame.slotBase[19].value`.

Affected layers:

- AOT C scalar bitwise unary emission
- focused generated-product parser/AOT fixture
- AOT semantic contract documentation and plan records

## Baseline

Before this slice, the focused generated C emitted a frame-backed destination block for the signed i64 bit-not result. The RED test failed because the generated product still contained:

- `/* zr_aot_scalar_exec_i64_bit_not semirOpcode=41 dstSlot=19 sourceSlot=1 */`
- `SZrTypeValue *zr_aot_destination = ZR_NULL;`
- source type-check/reload through `frame.slotBase[1]`
- assignment through `zr_aot_s_result`
- destination frame payload materialization

Existing repository baseline:

- The worktree is dirty with unrelated parser/runtime/docs/build-output changes.
- The focused rebuild emitted existing unrelated `-Wunused-function` warnings in `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c`.
- `backend_aot_c_scalar_locals.c` is above the modularization threshold; this slice keeps using the existing result-skip proof boundary rather than mixing in a structural split.

## Test Inventory

- Focused subsystem case: `tests/parser/test_aot_c_typed_scalar.c` verifies generated C for the signed i64 bit-not result.
- Generated-product boundary: inspected `build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c` for the focused block.
- Source-contract target: `zr_vm_aot_c_source_contracts_test`.
- Registered CTest case: `aot_c_typed_scalar`.
- Negative cases: forbidden generated C tokens for the old i64 bit-not destination pointer, source frame type-check/reload, `zr_aot_s_result`, and destination frame payload materialization.

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

- RED before implementation: failed 1/1 because the generated C still contained the old i64 bit-not destination block at byte offset 105371.
- GREEN after implementation: `1 Tests 0 Failures 0 Ignored OK`.
- Build output also reported existing unrelated unused-function warnings in `compile_expression_types.c`.

Generated C evidence:

```c
/* zr_aot_scalar_exec_i64_bit_not semirOpcode=41 dstSlot=19 sourceSlot=1 */
zr_aot_s19 = ~zr_aot_s1;
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

- The RED generated-product assertion first proved the old i64 bit-not destination/result materialization was still present.
- `backend_aot_write_c_scalar_i64_bit_not()` now uses `backend_aot_c_scalar_locals_i64_written_before()` and `backend_aot_c_scalar_locals_i64_result_can_skip_value_slot()` before skipping frame materialization.
- The focused generated C now contains only `zr_aot_s19 = ~zr_aot_s1;` for the i64 bit-not result block.
- The focused typed scalar executable, source-contract target, and registered CTest pass.

## Acceptance Decision

Accepted for the focused 07-S2 i64 bit-not result local-only slice.

This is not full 07-S2 and not full M1.5/07 completion. Remaining work includes other scalar result materialization, primitive constant frame writes, generic float copy/type checks, direct return/result frame fallbacks, prologue/frame setup, reset-stack-null frame writes, and boundary local restoration.

`backend_aot_c_scalar_locals.c` remains above the modularization threshold. The smallest follow-up split is extracting scalar result-skip/liveness proof helpers into a dedicated module.
