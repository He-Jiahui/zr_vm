# 2026-07-19 AOT 08-S6Z / 10-S4Z47 / 11-S5E Generic Method Argument Object Decoding

## Scope

This slice decodes existing reflection objects into bounded generic argument descriptors and dispatches them through the
exact MethodSpec make boundary. It does not add a native stack entry or publish a `zr.reflection` module export.

## Contract

- `ZrCore_Reflection_MakeGenericMethodFromObjects()` requires a trusted explicit metadata runtime, an open generic method
  definition reflection object, and a reflection argument array.
- The object's runtime native pointer is compared with the trusted runtime but is never used as the dereference source.
- Kind string/value pairs, definition flags, declared arity, array continuity, scalar ranges, and nested object types are
  validated before resolution.
- Primitive, direct token, array, tuple, ownership, nullable, and union shapes share the existing descriptor validator.
- A two-pass decoder caps recursion at 64 and total nodes at 1024, allocates one exact scratch arena, and frees it on all
  success and failure paths.
- Definition and argument roots remain pinned while field-name strings and result objects are allocated.

## TDD And Regression Evidence

- RED: MSVC compiled all changed sources and linked with exactly one missing symbol,
  `ZrCore_Reflection_MakeGenericMethodFromObjects`.
- GREEN: dynamic generic reflection reports 31/0 on MSVC 19.44, GCC 11.4, and Clang 14.0.
- Negative coverage includes runtime mismatch, constructed-object substitution, non-array input, mismatched kind fields,
  invalid primitive type, and null inputs.
- Focused metadata/reflection CTest passes 6/6 on all three compilers.
- Final-source shared regression passes GC 66/0, instruction execution 31/0, and instruction table 95/0 on all three
  compilers. Clang source identity was compared before execution.
- The decoder emits no GCC/Clang diagnostics. The pre-existing `HEAD` profile-enum gap remains outside this slice.

## Acceptance Decision

Accepted as 08-S6Z / 10-S4Z47 / 11-S5E. Bounded argument-object decoding and object-level dispatch are closed. Native
stack dispatch, trusted module registration, `zr.reflection` export, cross-module method binding, and full-AOT closure
remain open.
