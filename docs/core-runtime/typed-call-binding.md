---
related_code:
  - zr_vm_core/include/zr_vm_core/typed_call_binding.h
  - zr_vm_core/src/zr_vm_core/typed_call_binding.c
  - zr_vm_core/src/zr_vm_core/call_binding_signature.c
  - zr_vm_core/src/zr_vm_core/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/call_binding_member.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/artifact_call_binding.c
  - zr_vm_core/src/zr_vm_core/io_call_binding.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_callable_member.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_member.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_lambda.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_typed_call.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/aot_typed_call_binding.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/typed_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_call_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_typed_call.c
  - zr_vm_library/src/zr_vm_library/aot_typed_call_binding.c
plan_sources:
  - user: W2 Call Binding M1 approved implementation
  - docs/plans/benchmark/optimize/02-interpreter-hot-path.md
tests:
  - tests/parser/test_typed_call_binding.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_call_binding_artifact.c
  - tests/parser/test_call_binding_aot_projection.c
  - tests/parser/test_aot_c_call_shared_library_smoke.c
  - tests/acceptance/2026-09-06-typed-call-binding.md
doc_type: module-detail
---

# Typed Function Value Call Binding

`TYPED_FUNCTION` describes the signature required by a call to a lexical function
value. A parameter such as `callback: fn(int)->int` may contain a different VM
closure, native callable, or AOT callable on each invocation. Its call site binds
the required signature while retaining the live value as the invocation target.

## Compiler Contract

Function, method, and lambda parameter registration records a callable-value
signature from the function-type AST. The existing callable overload resolver can
therefore check arguments and return values for such parameters. Canonical
function-type nodes provide a fallback for semantic bindings that already carry
a structured type identity. Neither path hashes a `signatureDisplay` label.

After quickening, typed call sites are restored to the generic call opcode with
the same operands. A function signature does not establish whether its live
value uses VM, native, or AOT execution. The linker rejects persisted typed sites
that claim a backend-specific call opcode.

The producer uses the same structural signature hash as VM function metadata:
parameter count, varargs state, return type, parameter type references, and
parameter passing roles. This preserves distinctions between value, `in`, `ref`,
and `out` contracts. Missing structural information is an error.

Each typed cache has zero target and owner tokens, a nonzero signature token and
hash, and its containing module hash. Its relocation kind is `NONE`, with target
index `ZR_CALL_BINDING_SLOT_NONE`. Finalization creates a standalone signature
metadata row marked `ZR_METADATA_TOKEN_RECORD_CALLABLE_SIGNATURE`. No member
definition or constant-pool relocation is invented for the runtime value.

## Linking And Persistence

The linker checks the signature row's token, marker, ownership fields, hash, and
module identity before publishing the instruction-to-cache lookup. Duplicate or
missing signature rows fail linking. A successful initial link has target kind
`NONE` and the containing function's current binding generation.

Binary IO and artifact rows permit `NONE` relocations only for typed contracts.
AOT projection uses the invalid function-index sentinel for these rows, and the
metadata loader validates them without projecting a static thunk. Runtime target
witnesses and callable addresses are never serialized.

## Runtime Validation

The interpreter validates typed bindings in generic, known, zero-argument, spread,
and tail-call instruction lanes. AOT generic calls and direct-call preparation
share the same typed validation helper.

`PrepareTypedCall` validates the containing function's generation, obtains the
actual callable's structural signature, compares it with the required signature,
and records a tagged runtime witness. VM and AOT closures provide their metadata
function. Native registered function descriptors provide a structural signature
through the global registry callback. AOT witnesses also resolve the current
registration's thunk, method metadata, and invoker.

When the same callable appears again, its target generation must still match.
A different live callable is checked independently, even if the previous witness
became stale. The helper never writes to the callable value and never substitutes
the cached witness. Captured values, native context, and ownership state remain
attached to the original closure. Witness pointers use the existing function
cache GC tracing and write barriers.

## Supported Native Signatures

The native adapter currently accepts fixed-arity, nongeneric registered function
descriptors with primitive parameter and return types and explicit passing modes.
Canonical integer widths, floating types, booleans, strings, objects, and null are
recognized. Descriptors needing nominal, array, nested callable, generic, or
varargs type parsing are rejected as missing typed contracts. Extending this
adapter requires a structured descriptor type projection; hashing a display
signature would not establish compatibility.

Static native module members with complete fixed-arity descriptors are projected
to their canonical `fn(...) -> ...` source type when read through an import. The
same structural parameter, return, and passing-mode checks therefore apply to
`provider.add` passed directly to a declared `fn(int) -> int` parameter. Members
whose descriptor omits structural type information remain broad values and are
rejected at a typed boundary; the compiler does not fall back to a display-label
hash or a broad function conversion.

## Validation

The focused suite covers typed parameters, zero-argument calls, repeated closures
with distinct captures, native callbacks, imported native members in source,
runtime signature mismatch, generation invalidation, signature-row tampering, and
binary roundtrip (including an imported native member). The ordinary binding
pipeline and artifact/AOT suites exercise the shared linker and persistence hooks.
