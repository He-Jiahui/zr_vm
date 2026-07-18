# 2026-07-18 AOT 08-S6M / 10-S4Z34 Interpreter Generic Parameter Substitution

## Scope

This cross-stage slice connects the S6L interpreter instance context to the existing 11-S5 GenericParam metadata view,
providing a concrete type-object lookup for an open generic type parameter.

Call-frame context, uncollected generic method execution, dynamic value-type instances, cross-module identity, and
script-level methods are outside this slice.

## Implementation

- `ZrCore_Reflection_ResolveInterpreterGenericParameterTypeObject()` resolves the requested owner and parameter index
  through `ZrCore_Reflection_ResolveGenericParameter()`; no parallel generic parameter table is introduced.
- The instance must be an ordinary S6L interpreter generic object with a reflection type context.
- The context's `metadataRuntime` must be the supplied runtime, and `genericBaseToken` must equal the metadata parameter
  owner token.
- `genericArgumentCount` and the actual `genericArguments` array length must agree.
- The validated metadata `parameterIndex` selects an existing concrete argument reflection object.
- Wrong owners, out-of-range parameters, malformed context fields, and prototype objects fail closed.
- S6L/S6M interpreter scenarios moved to a focused 122-line test header, reducing the main scenario from 980 to 944
  lines.

## RED / GREEN

RED:

- The new test compiled the extended TypeDef/GenericParam fixture and reached the final link.
- MSVC failed with one unresolved symbol for the missing substitution API.

GREEN:

- Type parameter 0 resolves to the concrete primitive bool argument object.
- Type parameter 1 resolves to the concrete TypeDef token argument object.
- A TypeSpec owner, parameter index 2, and a prototype object are rejected.
- All preceding dynamic generic route, object materialization, and interpreter context tests remain green.
- The focused executable reports 17 tests with 0 failures.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3 passed.
- WSL Clang 14.0: focused CTest matrix 3/3 passed.
- Windows MSVC 19.44: focused CTest matrix 3/3 passed.
- The matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- GCC and Clang logs contain no warnings attributed to `reflection_interpreter_generic_instance.c`.

## Acceptance Decision

Accepted as 08-S6M / 10-S4Z34 only. The interpreter can now map a metadata generic type parameter to its concrete
instance argument type object without duplicating identity rules. Full 08-S6 remains open until that context reaches a VM
call frame and an uncollected generic method executes. Full 10-S4 still requires script-object methods, generic method
reflection, and cross-module identity.
