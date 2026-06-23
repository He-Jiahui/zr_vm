# AOT M1.5 07-S5 static f64 two-arg divide typed thunk

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; stages 08-12 not started.

## Scope

This slice broadens the static f64 two-argument typed-to-typed direct-call route from add/subtract/multiply to ordered division with an explicit runtime zero-denominator failure channel.

## Baseline

- Existing f64 typed thunks covered no-arg constants, one-arg identity/negate/constant arithmetic, and two-arg add/subtract/multiply.
- One-arg f64 divide/modulo accepted only nonzero constants and deliberately left dynamic denominators open.
- Existing generated AOT scalar float division checks zero and raises a runtime error before emitting the C division expression.

## RED

- The typed call contract failed as expected because `backend_aot_c_try_get_f64_arg0_arg1_divide_return(` was absent.
- The new f64 shared-library smoke was added to prove generated C for `ratio(84.0, 2.0)` needs the two-argument f64 thunk and must stay off the VM call/value fallback path.

## Implementation

- `backend_aot_c_typed_f64_thunks.c` now recognizes two-parameter float/double callees whose body is ordered `DIV_FLOAT` followed by `FUNCTION_RETURN`.
- `backend_aot_c_can_emit_typed_f64_two_arg_thunk()` now accepts add, subtract, multiply, and divide.
- The divide thunk emits the existing f64 two-argument native signature, checks `zr_aot_arg1 == 0.0`, calls `ZrCore_Debug_RunError(state, "generated AOT float divide by zero")`, and then returns a defensive `0.0` for compiler reachability before the normal `zr_aot_arg0 / zr_aot_arg1` expression.
- The existing f64 typed direct-call proof and writer are reused, so eligible call sites still assign `zr_aot_fD = zr_aot_typed_f64_fn_N(state, zr_aot_fA, zr_aot_fB)` with scalar-only destination sync elision.

## Tooling Evidence

- WSL GCC Debug build directory: `build/codex-aot-07-wsl-gcc-debug`.
- Focused RED/GREEN command:
  - `cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test -j 2`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test`
- Focused RED result:
  - typed call contracts 3/1, missing `backend_aot_c_try_get_f64_arg0_arg1_divide_return(`
- Focused GREEN result:
  - typed call contracts 4/0
  - f64 typed direct-call smoke 12/0
- Broader focused WSL GCC AOT validation:
  - source contracts 19/0
  - call contracts 4/0
  - typed call contracts 4/0
  - generic numeric contracts 1/0
  - global contracts 7/0
  - logical contracts 4/0
  - power contracts 2/0
  - frame setup contracts 1/0
  - return contracts 1/0
  - value SemIR contracts 4/0
  - shared-library smoke 8/0
  - call smoke 5/0
  - typed direct-call i64 smoke 5/0
  - bool typed direct-call smoke 19/0
  - u64 typed direct-call smoke 14/0
  - f64 typed direct-call smoke 12/0
  - arithmetic typed direct-call smoke 5/0
  - bitwise typed direct-call smoke 6/0
  - typed scalar 1/0
  - value-type smoke 1/0
  - generic numeric smoke 1/0
  - global smoke 9/0
  - logical smoke 4/0
  - power smoke 1/0

## File Scale

- `backend_aot_c_typed_f64_thunks.c`: 843 physical / 736 non-empty lines.
- `test_aot_c_typed_call_contracts.c`: 763 physical / 725 non-empty lines.
- `test_aot_c_typed_direct_call_f64_shared_library_smoke.c`: 297 physical / 272 non-empty lines.
- `aot_c_typed_direct_call_f64_smoke_support.h`: 211 physical / 191 non-empty lines.

## Acceptance Decision

Accepted for the narrow f64 two-parameter divide-return typed direct-call slice with a runtime zero-denominator failure channel. This does not close 07-S5: f64 modulo with runtime failure, inline structs, `in`/`out` writeback, deopt/dynamic bridges, broader ABI coverage, dynamic value access helpers, and stages 08-12 remain open.
