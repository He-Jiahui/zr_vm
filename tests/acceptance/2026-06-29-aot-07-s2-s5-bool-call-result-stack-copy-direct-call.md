# AOT 07-S2/S5 Bool Call-Result Stack-Copy Direct Call

## Scope

This record covers one narrow 07-S2/S5 slice in M1.5: a typed bool call result that is copied to another stack slot before a bool branch remains a scalar bool local and can still use typed direct-call lowering.

Affected layer:

- AOT scalar-local declaration and consumer inference in `backend_aot_c_scalar_locals.c`.
- Existing bool typed direct-call shared-library smoke coverage in `tests/parser/test_aot_c_typed_direct_call_bool_shared_library_smoke.c`.

## Baseline

Before this slice, call-result consumer inference saw the immediate stack copy from the call destination, but did not continue into the copy destination's later `JUMP_IF_BOOL_FALSE` consumer. In the `invert(false)` fixture, slot 4 was declared as an i64 local, so the bool typed direct-call gate rejected it and generated:

```text
/* zr_aot_direct_static_function_call */
ZrLibrary_AotRuntime_CallStaticDirect(...)
/* zr_aot_direct_static_function_call_sync_i64_local_boundary */
```

Initial RED:

```text
zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test
28 Tests, 25 Failures
first failure: aot_c_typed_direct_call_bool_smoke_support.h:164
```

## Accepted Shape

The same fixture now keeps the call result as a bool scalar local:

```text
/* zr_aot_static_bool_one_arg_direct_call */
/* zr_aot_static_bool_one_arg_direct_call_metadata_guard */
zr_aot_b4 = zr_aot_typed_bool_fn_1(zr_aot_b5);
```

The existing smoke helper also forbids:

```text
ZrLibrary_AotRuntime_CallStaticDirect(state,
ZrLibrary_AotRuntime_CallStackValue(state,
SZrTypeValue *zr_aot_typed_destination
ZR_VALUE_FAST_SET(zr_aot_typed_destination,
```

## Implementation

`backend_aot_c_scalar_locals_kind_from_call_result_consumers()` now handles call-result stack copies the same way as the existing stack-copy destination analysis:

- Prefer the copy destination SemIR kind when available.
- Otherwise recurse through `backend_aot_c_scalar_locals_kind_from_stack_copy_destination_consumers()`.
- Use the proven callee return kind as the candidate kind when the original call destination does not already have a slot kind.

This keeps the change scoped to consumer inference. It does not alter runtime call helpers or typed thunk emission.

## Validation

WSL gcc:

```text
typed direct-call bool shared-library smoke: 28/0
typed call contracts: 4/0
call contracts: 8/0
source contracts: 22/0
logical shared-library smoke: 6/0
call shared-library smoke: 5/0
```

WSL clang:

```text
typed direct-call bool shared-library smoke: 28/0
typed call contracts: 4/0
call contracts: 8/0
source contracts: 22/0
logical shared-library smoke: 6/0
call shared-library smoke: 5/0
```

Windows MSVC Debug:

```text
typed direct-call bool smoke: 0 failures / 28 ignored
typed call contracts: 4/0
call contracts: 8/0
source contracts: 22/0
```

## Remaining Work

This slice does not close dynamic/string/object truthiness, value-copy migration, GC roots/exports/frame cleanup, broader byte-frame narrowing, field layout resolver migration, performance counters, or the full typed-function zero `SZrValue`/frame-write proof.
