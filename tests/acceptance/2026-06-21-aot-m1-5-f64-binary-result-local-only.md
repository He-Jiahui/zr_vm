# AOT M1.5 07-S2 F64 Binary Result Local-Only

Date: 2026-06-21 06:37:26 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice removes frame destination/result materialization from the focused f64 binary result when both operands are already written f64 scalar locals and every reachable later consumer can use the maintained f64 scalar local.

For the focused typed scalar source, the f64 binary result:

- `dstSlot=32`, `leftSlot=19`, `rightSlot=20`
- generated expression `zr_aot_f32 = zr_aot_f19 * zr_aot_f20;`

now emits only a scalar-local assignment. The generated f64 binary block no longer declares `SZrTypeValue *zr_aot_destination`, creates `zr_aot_f_result`, or writes a double payload through `frame.slotBase[32].value`.

Affected layers:

- AOT C scalar f64 binary emission
- scalar-local liveness/result-skip proof for f64 consumers
- focused generated-product parser/AOT fixture
- AOT semantic contract documentation and plan records

## Baseline

Before this slice, the focused generated C already reused f64 source locals for the multiply, but still materialized the result through the frame:

- `/* zr_aot_scalar_exec_f64_binary semirOpcode=29 dstSlot=32 leftSlot=19 rightSlot=20 */`
- `SZrTypeValue *zr_aot_destination = ZR_NULL;`
- destination frame assignment through `frame.slotBase[32].value`
- assignment through `zr_aot_f_result`
- destination double frame payload materialization

Existing repository baseline:

- The worktree is dirty with unrelated parser/runtime/docs/build-output changes.
- `git diff --check` reports LF/CRLF conversion warnings from the existing dirty worktree but exits 0.
- `backend_aot_c_scalar_locals.c` is above the modularization threshold; this slice keeps extending the existing result-skip proof boundary rather than mixing in a structural split.

## Test Inventory

- Focused subsystem case: `tests/parser/test_aot_c_typed_scalar.c` verifies generated C for the f64 binary result.
- Generated-product boundary: inspected `build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c` for the focused block.
- Source-contract target: `zr_vm_aot_c_source_contracts_test`.
- Registered CTest case: `aot_c_typed_scalar`.
- Negative cases: forbidden generated C tokens for the old f64 binary destination pointer, result temporary, and destination frame payload materialization for `dstSlot=32`.

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

- RED before implementation: failed 1/1 because the generated C still contained the old `dstSlot=32` f64 binary destination block at byte offset 106154.
- GREEN after implementation: direct run reports `1 Tests 0 Failures 0 Ignored OK`.

Generated C evidence:

```c
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

- The RED generated-product assertion first proved the old f64 binary destination/result materialization was still present.
- `backend_aot_write_c_scalar_f64_binary()` now uses f64 written-before proof and `backend_aot_c_scalar_locals_f64_result_can_skip_value_slot()` before skipping frame materialization.
- The result-skip proof now recognizes f64 local consumers for f64 binary operations, f64-to-int/uint conversions, and f64 stack copies, while keeping unknown frame-dependent reads conservative.
- The focused generated C now contains only `zr_aot_f32 = zr_aot_f19 * zr_aot_f20;` for the f64 binary result block.
- The focused typed scalar executable, source-contract target, and registered CTest pass.

## Acceptance Decision

Accepted for the focused 07-S2 f64 binary result local-only slice.

This is not full 07-S2 and not full M1.5/07 completion. Remaining work includes other scalar result materialization, primitive constant frame writes, generic float copy/type checks, direct return/result frame fallbacks, prologue/frame setup, reset-stack-null frame writes, and boundary local restoration.

`backend_aot_c_scalar_locals.c` remains above the modularization threshold. The smallest follow-up split is extracting scalar result-skip/liveness proof helpers into a dedicated module.
