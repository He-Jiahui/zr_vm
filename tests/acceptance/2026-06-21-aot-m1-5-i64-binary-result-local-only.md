# AOT M1.5 07-S2 I64 Binary Result Local-Only

Date: 2026-06-21 05:43:39 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice removes frame destination/result materialization from the focused signed i64 binary result when every reachable later consumer can use the maintained signed scalar local.

For the focused typed scalar source, the first signed i64 binary result:

- `dstSlot=2`
- `leftSlot=0`
- `rightSlot=1`
- generated expression `zr_aot_s2 = zr_aot_s0 * zr_aot_s1;`

now emits only the scalar-local assignment. The generated block no longer declares `SZrTypeValue *zr_aot_destination`, creates `zr_aot_s_result`, or writes the result payload through `frame.slotBase[2].value`.

The i64 result skip proof extends the reachable-consumer scan to signed scalar locals. It recognizes signed local consumers, typed signed conversions, direct signed branches, scalar stack-copy consumers, and unconditional `JUMP` instructions while preserving conservative rejection for unknown frame-dependent slot reads.

Affected layers:

- AOT C scalar binary emission
- AOT C scalar-local liveness/result-skip proof
- focused generated-product parser/AOT fixture
- AOT semantic contract documentation and plan records

## Baseline

Before this slice, the focused generated C still emitted a frame-backed destination block for the first signed i64 binary result. The RED test failed because the generated product still contained:

- `/* zr_aot_scalar_exec_i64_binary semirOpcode=29 dstSlot=2 leftSlot=0 rightSlot=1 */`
- `SZrTypeValue *zr_aot_destination = ZR_NULL;`
- assignment through `zr_aot_s_result`
- destination frame payload materialization

Existing repository baseline for this run:

- The worktree is dirty with unrelated parser/runtime/docs/build-output changes.
- A previous source-contract rebuild attempt reported an unrelated dirty core compile error in `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`, but the fresh rerun at 2026-06-21 05:47 +08:00 rebuilt and linked the target successfully.

## Test Inventory

- Focused subsystem case: `tests/parser/test_aot_c_typed_scalar.c` verifies generated C for the signed i64 binary result.
- Generated-product boundary: inspected `build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c` for the focused block.
- Registered CTest case: `aot_c_typed_scalar`.
- Source-contract evidence: fresh target rebuild plus direct execution of `zr_vm_aot_c_source_contracts_test`.
- Negative case: forbidden generated C tokens for the old i64 binary destination/result materialization.
- Boundary behavior: i64 result-skip proof allows only known local consumers and remains conservative for unknown slot mentions; unconditional `JUMP` no longer counts as a value-slot read.

## Tooling Evidence

Tooling:

- WSL GCC Debug build directory: `build/codex-aot-07-wsl-gcc-debug`
- CMake/Ninja target rebuild for focused typed scalar test
- Direct generated C inspection
- CTest focused filter
- `git diff --check`

Commands and key outputs:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"
```

- RED before implementation: failed 1/1 because the generated C still contained the old i64 binary destination block.
- GREEN after implementation: `1 Tests 0 Failures 0 Ignored OK`.

Generated C evidence:

```c
/* zr_aot_scalar_exec_i64_binary semirOpcode=29 dstSlot=2 leftSlot=0 rightSlot=1 */
zr_aot_s2 = zr_aot_s0 * zr_aot_s1;
```

Source-contract rebuild:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j2 2>&1 | tail -n 80"
```

- GREEN: target rebuilt and linked successfully.

Source-contract binary:

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test 2>&1 | tail -n 120"
```

- GREEN: 19 tests passed, 0 failures.

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

- The RED generated-product assertion first proved the old i64 binary destination/result materialization was still present.
- `backend_aot_write_c_scalar_i64_binary()` now skips frame result materialization only after source locals are proven written and `backend_aot_c_scalar_locals_i64_result_can_skip_value_slot()` proves reachable consumers can use the signed scalar local.
- The focused generated C now contains only `zr_aot_s2 = zr_aot_s0 * zr_aot_s1;` for the first i64 binary block.
- The focused typed scalar executable and registered CTest pass.
- The source-contract target rebuilds and the freshly executed source-contract binary passes.

## Acceptance Decision

Accepted for the focused 07-S2 i64 binary result local-only slice.

This is not full 07-S2 and not full M1.5/07 completion. Remaining work includes other scalar result materialization, primitive constant frame writes, generic float copy/type checks, direct return/result frame fallbacks, prologue/frame setup, reset-stack-null frame writes, and boundary local restoration.

`backend_aot_c_scalar_locals.c` is now above the modularization threshold. This slice kept the proof in place to avoid mixing a structural split into the behavior change; the smallest follow-up split is extracting scalar result-skip/liveness proof helpers into a dedicated module.
