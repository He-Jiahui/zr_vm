# 2026-07-18 AOT 08-S6L / 10-S4Z33 Interpreter Generic Reference Instance Context

## Scope

This cross-stage slice gives the interpreter its first concrete consumer of an uncollected constructed-generic carrier:
an ordinary reference-class object with an owned generic type context.

Generic parameter substitution, generic method execution, dynamic struct/union instances, cross-module identity, and
script-level construction methods are outside this slice.

## Implementation

- `ZrCore_Reflection_RevalidateDynamicGenericTypeInstance()` is the shared public same-runtime stale-carrier gate.
- `ZrCore_Reflection_NewInterpreterGenericInstanceObject()` accepts only a carrier that still resolves to
  `INTERPRETER_DEOPT` and an open class prototype.
- The result is an ordinary initialized object whose prototype remains the open generic class prototype.
- The instance owns a deep-copied public generic type object in `__zr_genericTypeInfo`;
  `ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject()` reads and validates that context.
- Existing object prototype lookup supplies open-prototype members without creating permanent per-instance prototypes.
- AOT carriers, struct/union prototypes, and non-instance objects fail closed.
- The type object, instance, and field key are protected with the existing GC ignore-root discipline during allocation,
  and cleanup retains the allocated object pointer even when the function returns null.

## RED / GREEN

RED:

- The new reference-instance context test compiled against two missing public interpreter APIs.
- MSVC linked the existing core and then failed with exactly two unresolved symbols for the constructor and getter.

GREEN:

- The object uses the supplied open class prototype and satisfies `ZrCore_Object_IsInstanceOfPrototype()`.
- Its generic context reports the interpreter-deopt route and exact open generic base token.
- An inherited marker on the open prototype remains readable through ordinary object lookup.
- Struct prototypes and AOT carriers are rejected, and a prototype object is not accepted as an instance context owner.
- The focused executable reports 16 tests with 0 failures.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3 passed.
- WSL Clang 14.0: focused CTest matrix 3/3 passed.
- Windows MSVC 19.44: focused CTest matrix 3/3 passed.
- The matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- GCC and Clang logs contain no warnings attributed to `reflection_interpreter_generic_instance.c`,
  `reflection_generic_instance.c`, or `reflection_generic_type_object.c`.

## Acceptance Decision

Accepted as 08-S6L / 10-S4Z33 only. Reference-class object/context materialization is now a real interpreter consumer of
the deopt carrier, while full 08-S6 remains open until generic parameters are substituted and uncollected generic methods
execute. Full 10-S4 still requires script-object methods, generic method reflection, and cross-module identity.
