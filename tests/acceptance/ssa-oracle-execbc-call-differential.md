# SSA 01.05: oracle/ExecBC ordinary CALL differential

## Scope and baseline

The direct pointer-free ExecIR oracle already executed provider-backed CALL,
but the initial ExecBC projection marked it non-runnable. This slice adds a
separate caller-owned projected CALL provider, full ordered argument delivery,
and a bounded, owned CALL event snapshot. The first new GCC build failed as
expected because `SZrExecBcExecutionInput` had no `call` or `callUserData`
fields. After implementation, the full verifier rejected the test's initial
CALL because the fixture omitted mandatory managed-heap and native-FFI
memory versions. Adding the schema-required input/output tokens made its
STRUCTURE|SSA|EFFECT verification pass.

## Test inventory and failure injection

- `test_oracle_execbc_call_differential` builds one verified five-argument
  CONSTANT/CALL/RETURN function. Independent callbacks receive argument
  values 1..5 in order, return 15, and each emit one CALL event containing
  instruction ID 6, source ID 606, and the first four operand snapshots.
  The differential harness compares CALL operand values/positions and RETURN
  at source 607. Altering the fourth operand yields `EVENT_MISMATCH` at index 3.
- A separate verified nine-argument function sends all nine arguments through
  both providers, returning 45 and storing exactly four bounded snapshots.
  The projected call's temporary operand allocation is freed on all paths.
- Removing the projected callback reports `UNSUPPORTED`; provider rejection
  reports `ORACLE_CALL_ERROR`; an undefined returned value reports
  `INVALID_VALUE`. All three identify instruction 6/source 606 and leave
  previously published event and slot pointers untouched. A successful retry
  replaces the result. Candidate event capacity is reserved before callback
  execution; allocation overflow/OOM is diagnosed but not fault-injected.
- Review found that the runner initially forwarded invalid input value kinds
  to the CALL provider where the oracle rejected them before dispatch. A new
  invalid-constant and invalid-initial-value fixture was RED at the callback;
  shared input preflight now rejects both with `INVALID_VALUE` at instruction
  zero, with no provider invocation and the previously published result intact.

No host pointer is interpreted from operands. Providers own external state and
rollback on rejection; this test only counts invocations. Cancellation,
exceptions/landing pads, leases, allocation/drop balance, production ExecBC
and C/LLVM parity remain outside this phase.
The projected runner reserves event storage before invoking a provider,
whereas the direct oracle allocates when appending after the provider; on an
event-allocation OOM their provider side effects may differ. This known
failure-path parity gap has no fault-injection coverage and is not accepted
as full error-path equivalence.

## Validation

From the repository root, the affected parser library and direct test targets
were rebuilt with GCC 11.4.0 (WSL), Clang 14.0.0 (WSL), and MSVC
19.44.35228.0 (Windows); GCC 11.4.0 ASan/UBSan rebuilt the focused tests.
The test was first RED on missing projected CALL callback input fields, and
the additional invalid-kind regression was RED after review.
After the fixture's required memory tokens were added, the GNU focused test
passed. Clang and MSVC initially reported conditional-range initialization
warnings in the test; zero-initializing those local ranges removed the new
warnings from the final changed-file builds. MSVC still emits its existing
`/W3`-overridden-by-`/W4` command-line warning.

```text
wsl.exe bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/ssa-gcc-debug --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_differential_harness_test zr_vm_parser_shared -j 4'
wsl.exe bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build/ssa-gcc-debug -L ssa --output-on-failure --no-tests=error -j 4'
wsl.exe bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/ssa-clang-debug --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_differential_harness_test zr_vm_parser_shared -j 4'
wsl.exe bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build/ssa-clang-debug -R "^(ssa_oracle_projections|ssa_differential_harness)$" --output-on-failure --no-tests=error'
wsl.exe bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/ssa-gcc-asan-phase80 --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_differential_harness_test -j 4'
wsl.exe bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build/ssa-gcc-asan-phase80 -R "^(ssa_oracle_projections|ssa_differential_harness)$" --output-on-failure --no-tests=error'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build build/ssa-msvc-debug --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_differential_harness_test zr_vm_parser_static --config Debug -j 4
ctest --test-dir build/ssa-msvc-debug -C Debug -R '^(ssa_oracle_projections|ssa_differential_harness)$' --output-on-failure --no-tests=error
```

The final GNU Debug SSA-label sweep passed 80/80, including the newly
rebuilt oracle and harness tests; the other 78 binaries were not all freshly
rebuilt. Clang Debug, Windows MSVC Debug and GCC ASan/UBSan each passed 2/2
focused tests after the range-initialization change. Sanitizers reported no
memory or undefined-behavior errors. No cancellation or fault-injected
allocation failure was exercised. This slice is accepted for successful
pointer-free ordinary CALL projection and its tested diagnostics only; OOM
event-side-effect parity and the complete 01.05/M1 gates are not closed.
