# SSA 01.02: per-instruction semantic operand ranges

## RED and focused fixtures

A one-instruction semantic function advertises one argument for a typed
variadic call but has an empty value-operand side pool. Before the builder
fix, MSVC printed `FAIL: builder silently dropped a variadic call's missing
operand`: lowering skipped the copy and the SSA check accepted the call with
zero operands. The negative fixture now requires `INVALID_RANGE` at entry
block 1, semantic instruction 1, with existing caller output unchanged.

An additional negative case sets the operand start to `UINT32_MAX`, count 1,
and supplies one real side-pool value. The checked subtraction rejects the
wrapped range before a read. The positive fixture defines a constant,
passes its value to a typed call, and returns the call result; it asserts
both copied operands and structural ExecIR verification.

## Observed validation

- MSVC (VSDevCmd, D:-backed focused build): the expanded core/effects/
  dominator/builder/value/oracle/pass-manager CTest selection passed 8/8.
- WSL GCC 11.4 directly compiled the real builder/core sources with
  `-fsanitize=address,undefined`; the D:-backed fixture printed
  `ssa builder CFG PASS` (exit 0), with no sanitizer report.
- WSL Clang 14 directly compiled the same sources without sanitizers; the
  D:-backed fixture printed `ssa builder CFG PASS` (exit 0). No Clang
  sanitizer or CTest result is claimed for this slice; the prior Clang
  sanitizer attempt was intermittent on this WSL host.

## Remaining gates

This covers logical side-pool bounds and copy fidelity, not source-language
argument mapping, dynamic call dispatch, exception-edge availability,
pruned phi construction or end-to-end backend parity. The user-modified
SSA construction tests were left untouched.
