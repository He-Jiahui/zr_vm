# AOT Typed I64 Checked Divide and Modulo

Date: 2026-10-01
Status: Completed bounded slice; 07.02 task 2 and the wider 07-S5/M1.5 matrix remain open.

## Scope

This slice closes the signed `int` typed C-AOT division and modulo boundary
`INT64_MIN / -1` and `INT64_MIN % -1`. It covers two-argument stateful thunks
and source guards for the existing three-argument stateful thunks. It does not
claim the complete checked/wrapping arithmetic, floating-point, or LLVM matrix.

## RED

- The active generator emitted only a zero-denominator check before raw `/` and
  `%`; `INT64_MIN/-1` therefore reached host signed undefined behavior.
- The qualified GCC and Clang child runs recorded `SIGFPE` for the divide
  overflow case under `D:/tmp/zr_vm/ssa-control/*-aot-div-overflow-qualified-red.log`.
- After the first guard-only fix, the generated call returned failure but
  `ZrLibrary_AotRuntime_GetLastError` was null. That exposed the missing
  generated-diagnostic recording path.

## Implementation

- `backend_aot_c_typed_i64_thunks.c` guards the signed overflow pair before
  retaining the raw operator text, records a stable diagnostic through
  `ZrLibrary_AotRuntime_RecordError`, then enters the existing
  `ZrCore_Debug_RunError` path.
- The three-argument divide and modulo writers check both left-associative
  operator steps before their raw return expression.
- The runtime API stores the diagnostic in the per-global AOT runtime state;
  normal successful calls do not change the existing executed-via-AOT result.
- The arithmetic smoke adds a real modulo overflow fixture and isolated child
  execution for both operations. The i64 source contract also requires all four
  overflow diagnostics.

## GREEN

All build and generated artifacts below are under `D:/tmp/zr_vm`.

- GCC target build: `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test`.
- GCC `--overflow-only`: `2 Tests 0 Failures 0 Ignored`.
- GCC full arithmetic smoke: `10 Tests 0 Failures 0 Ignored`.
- GCC typed-call contracts: `4 Tests 0 Failures 0 Ignored`.
- Clang target build succeeded and Clang `--overflow-only` completed with
  `2 Tests 0 Failures 0 Ignored`.
- MSVC 17.14.40 built both focused targets; typed-call contracts ran
  `4 Tests 0 Failures 0 Ignored`. The arithmetic executable ran its Unix-only
  cases as `10 Tests 0 Failures 10 Ignored`, which is the fixture's declared
  Windows behavior.

The direct logs are retained in `D:/tmp/zr_vm/ssa-control` with the `gcc-aot-i64`,
`clang-aot-i64`, `gcc-aot-typed-call-contracts`, `clang-aot-typed-call-contracts`,
and `typed-i64-msvc` prefixes. A second Clang full arithmetic attempt was
bounded at 900 seconds while its default BFD linker was still compiling the
first overflow shared library; that timeout is recorded as a toolchain-I/O
limitation, not as a passing full Clang matrix. The separate GCC three-argument
smoke target reached the same BFD link bottleneck after compiling its objects;
it was interrupted after roughly 28 minutes, left no executable, and is not
claimed as a runtime pass. Its generated guards remain covered by the GCC,
Clang, and MSVC typed-call contract tests.

## Remaining Work

The broader checked/wrapping arithmetic contract, LLVM parity, floating-point
NaN/zero rules, and the complete 07.02/07-S5/M1.5 acceptance matrix remain
unimplemented or unverified.
