# 2026-07-18 AOT 08-S6O / 10-S4Z36 Interpreter Generic Instance VM Method Execution

## Scope

This cross-stage slice executes a resolved VM instance method on an S6L interpreter-deoptimized generic reference
instance while making the S6N context available through the active call info.

It does not materialize or execute a MethodSpec-owned generic method, dynamically instantiate a value type, resolve a
cross-module generic identity, or perform script-level member-name lookup.

## Implementation

- `ZrCore_Reflection_InvokeInterpreterGenericInstanceResolvedMethod()` validates the instance metadata runtime and open
  generic owner token before execution.
- The callee must be a non-native, non-vararg VM function whose parameter count exactly equals receiver plus explicit
  arguments. Wrong runtime, owner, or arity fails closed and clears the output result.
- The invocation reuses the ordinary object-call path for callable, receiver, and argument pinning; stack growth and
  anchors; argument staging; nested-call restoration; and result copying.
- `ZrCore_Function_CallWithoutYieldKnownValueAndRestoreWithInterpreterGenericContext()` performs the only specialized
  function action: it copies a pinned reflection type-object context after `PreCall` and before `Execute`.
- The active call info therefore owns the context during real bytecode execution and remains covered by S6N GC
  marking/forwarding rules.
- No metadata format or metadata-runtime API changed.

## RED / GREEN

RED:

- The resolved VM method scenario compiled and reached the final MSVC link.
- The link failed with one unresolved symbol for the missing public invoke API.

GREEN:

- A real VM identity method receives the generic instance receiver and one explicit int64 argument.
- A line observer inside the active VM frame resolves GenericParam[1] to the expected TypeDef argument object.
- The method returns the explicit argument value 73 and leaves the thread in `FINE` state.
- Wrong metadata runtime, wrong open owner token, and wrong fixed arity return false and clear a stale result.
- All preceding dynamic generic route, object, substitution, call-info, and full-GC tests remain green.
- The focused executable reports 19 tests with 0 failures.

An intermediate 19/1 result proved method execution and context resolution but returned null because the hand-written
test instruction used raw call-window offset 1 for the receiver. VM logical frame base is `functionBase + 1`; after the
test method was strengthened to return its explicit argument from logical slot 1, the final target passed 19/0. No
production workaround was added for the incorrect test encoding.

## Validation

- WSL GCC 11.4: final focused CTest matrix 3/3; shared GC 66/0 and instruction execution 31/0.
- WSL Clang 14.0: final focused CTest matrix 3/3; shared GC 66/0 and instruction execution 31/0.
- Windows MSVC 19.44: final focused CTest matrix 3/3; shared GC 66/0 and instruction execution 31/0.
- The focused matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- Changed implementation files introduce no GCC or Clang warning. `object_call.c` retains two Clang unused-helper
  warnings already present in the S6N baseline.
- Scoped `git diff --check` passed with only the repository's existing LF/CRLF conversion notices.

## Acceptance Decision

Accepted as 08-S6O / 10-S4Z36 only. A deoptimized generic reference-type instance can now execute a resolved VM method
with arguments, results, and an active generic context on the ordinary interpreter call path. Full 08-S6 remains open
for MethodSpec-owned generic methods, dynamic value-type instances, and cross-module identity. Full 10-S4 still requires
generic method reflection objects and script-level member lookup.
