# 2026-07-19 AOT 08-S6S / 10-S4Z40 Interpreter Generic Value Instance

## Scope

This cross-stage slice materializes an uncollected generic struct request as the runtime's existing boxed dynamic struct
representation. It does not synthesize a typed/AOT layout, resolve cross-module generic identity, resolve a function
from a method token, or expose script-level generic reflection methods.

## Implementation

- `ZrCore_Reflection_NewInterpreterGenericInstanceObject()` accepts an open struct prototype only for a revalidated
  interpreter-deopt request. AOT-routed instances continue to fail closed.
- The created object uses `ZR_OBJECT_INTERNAL_TYPE_STRUCT`, so it participates in the runtime's existing dynamic value
  semantics instead of being represented as a reference-class object.
- The hidden GC-managed generic type object is stored in the struct's ordinary dynamic field map. The existing struct
  clone path therefore preserves the context without a parallel side table.
- The generic instance context getter and instance GenericParam resolver accept both supported internal shapes: ordinary
  reference objects and boxed dynamic structs.
- Resolved VM instance-method invocation reuses the existing object-call struct-receiver copy/synchronization path and
  the S6N type call-info context.
- No typed layout, field offset, GC bitmap, metadata format, or metadata-runtime API is generated or changed.

## RED / GREEN

RED:

- The boxed generic value scenario compiled, linked, and ran under MSVC.
- The focused executable reported 23 tests with 1 failure at the expected non-null struct instance assertion; the
  existing implementation rejected every non-class prototype.

GREEN:

- An interpreter-deopt generic request creates a boxed dynamic struct with the supplied open struct prototype.
- Its hidden generic type context resolves GenericParam[1] to the expected TypeDef argument object.
- `ZrCore_Value_Copy()` uses `ZrCore_Object_CloneStruct()` to produce a different struct object. Mutating the copy's
  payload to 29 leaves the source payload at 17.
- A real resolved VM identity method receives the boxed struct, observes the active generic type context, and returns an
  explicit int64 argument with value 117.
- All preceding dynamic generic route, reference instance, MethodSpec context, full-GC, and VM execution tests remain
  green.
- The focused executable reports 23 tests with 0 failures.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3; shared GC 66/0 and instruction execution 31/0.
- WSL Clang 14.0: focused CTest matrix 3/3; shared GC 66/0 and instruction execution 31/0.
- Windows MSVC 19.44: focused CTest matrix 3/3; shared GC 66/0 and instruction execution 31/0.
- The focused matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- GCC, Clang, and MSVC logs contain no warning attributed to the changed reflection implementation or focused test.
- Scoped `git diff --check` passed with only the repository's existing LF/CRLF conversion notices.

## Acceptance Decision

Accepted as 08-S6S / 10-S4Z40 only. Uncollected generic structs can now materialize and execute on the interpreter's
dynamic value path while preserving generic context and by-value clone behavior. Full 08-S6 remains open for
cross-module generic identity. Full 10-S4 still requires method-token/function resolution and script-level generic
reflection behavior.
