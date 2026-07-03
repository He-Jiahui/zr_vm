# AOT 10-S2I / 10-S3M Method.Invoke Int64 No-Arg Return Boxing

## Scope

- Generated AOT C reflection invokers now include a narrow int64/no-arg return-boxing bucket.
- Affected layers: AOT C generator, generated shared-library descriptor/smoke, MethodInfo signature consumers, and reflection invoke metadata carrier tests.

## Baseline

- 10-S2H left `Method.Invoke` with dispatcher guards and void/no-return output canonicalization, but no generated return boxing.
- A naive implementation that captured the full entry thunk return failed because `FZrAotEntryThunk` returns an execution success flag; the business return is available through the generated typed helper.
- Existing build warning baselines remain unchanged; MSVC shared-library smoke still ignores Unix-only dynamic-library cases.

## Test Inventory

- `tests/parser/test_aot_c_frame_setup_contracts.c` asserts the generated include, helper bucket, function-table-fed invoker emitter API, typed i64 helper cases, boxed return call, and fallback.
- `tests/parser/test_aot_c_shared_library_smoke.c` compiles generated C, loads the module descriptor, invokes `answer(): int` through its generated reflection invoker, and verifies boxed int64 value `42`.
- Reflection/metadata focused coverage still runs `test_reflection_method_invoke.c`, `test_reflection_token_resolve.c`, `test_metadata_runtime_method_binding.c`, and `test_metadata_runtime_query.c`.
- Boundary covered: int64 return, zero fixed parameters, valid return slot, and `method->functionIndex` dispatch. Unsupported signatures still execute fallback without writing `outReturn`.
- Negative coverage remains from earlier return-slot and return-base-type guards.

## Tooling Evidence

- WSL gcc focused build/run passed: frame setup contracts 1/0, source contracts 22/0, shared-library smoke 13/0, reflection method invoke 5/0, reflection token resolve 7/0, method binding 2/0, metadata runtime query 24/0.
- WSL clang focused build/run passed with the same counts.
- Windows MSVC Debug focused build/run passed with the same counts, with shared-library smoke reporting 13 ignored Unix-only cases.
- CTest passed 7/7 on WSL gcc, WSL clang, and Windows MSVC Debug for `metadata_runtime_query|metadata_runtime_method_binding|reflection_token_resolve|reflection_method_invoke|aot_runtime_typed_direct_call_compatibility|aot_c_metadata_binding_loader|aot_c_method_info_signature`.
- `git diff --check` exited 0 and only reported existing LF/CRLF normalization warnings.
- Existing warning baselines observed during builds: MSVC conversion/unreachable-code warnings and clang unused/const-discard warnings outside this slice.

## Results

- RED 1: generated source contract failed because `zr_vm_core/value.h` was not emitted.
- RED 2: naive raw entry-thunk return capture failed at `Expected 42 Was 1`, proving full entry thunk return is not a business return.
- RED 3: corrected source contract failed until the reflection invoker emitter accepted the function table.
- GREEN: generated `zr_aot_try_invoke_i64_no_arg(...)` dispatches by `functionIndex`, calls `zr_aot_typed_i64_fn_<index>()`, and boxes the result with `ZrCore_Value_InitAsInt(...)`.

## Acceptance Decision

Accepted for the narrow generated int64/no-arg reflection return-boxing bucket.

Remaining risks and non-goals: argument unboxing, bool/u64/f64/object/inline returns, numeric widening, complete signature buckets, typed target ABI carrier, MethodSpec specialized code slot, public `MethodInfo` object materialization, cross-module token rewrite, and full trim analyzer.
