---
related_code:
  - zr_vm_core/src/zr_vm_core/call/typed_call_binding.h
  - zr_vm_core/src/zr_vm_core/call/typed_call_binding.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_signature.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/call/call_binding_member.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_call_binding.c
  - zr_vm_core/src/zr_vm_core/io/io_call_binding.c
  - zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_method_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_callable_member.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_class_member.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_lambda.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_typed_metadata.c
  - tests/parser/test_lambda_semantic_ir_isolation.c
  - zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_callable_binding_refinement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_call_binding.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_callable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_callable_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_typed_call.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_typed_call_binding.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c
  - tests/parser/test_typed_call_binding.c
  - tests/parser/test_typed_call_binding_imported_returns.inc
  - tests/parser/test_typed_call_binding_cyclic_returns.inc
  - tests/parser/test_typed_call_binding_member_semantics.inc
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_callable_member.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_callable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_callable_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h
  - zr_vm_core/src/zr_vm_core/call/typed_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/compiler_typed_metadata.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_typed_call.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_typed_call_binding.c
plan_sources:
  - "user: W2 Call Binding M1 approved implementation"
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - docs/plans/benchmark/optimize/02-interpreter-hot-path.md
tests:
  - tests/parser/test_lambda_semantic_ir_isolation.c
  - tests/parser/test_typed_call_binding.c
  - tests/parser/test_typed_call_binding_imported_returns.inc
  - tests/parser/test_typed_call_binding_cyclic_returns.inc
  - tests/parser/test_typed_call_binding_member_semantics.inc
  - tests/parser/test_syntax_hygiene_aliases.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_call_binding_artifact.c
  - tests/parser/test_call_binding_aot_projection.c
  - tests/parser/test_aot_c_call_shared_library_smoke.c
  - tests/acceptance/2026-09-06-typed-call-binding.md
doc_type: module-detail
---

# Typed Function Value Call Binding

## Current Member-Callable Contract (2026-10-15)

The completed typed-call-binding replay passes all 24 cases. The original
eighteen include the natural cyclic summary/live/consumer signature assertions
and result 17. Four public consumer semantic cases cover authentic callable
identity, a noncallable integer member, lexical shadowing and structured arity
rejection. Two block companions check normal statement-block restoration and
actual failed expression-block cleanup. Fixture corrections preserve every
assertion: the real expression-statement BLOCK needs its semicolon and wrapper,
and the incompatible declaration must be the last statement to retain its
structured `type_mismatch` diagnostic.

Immediately after inferred source-variable registration and before IR-local
creation, the three-file shape repair reads the initializer's final MEMBER_ACCESS
fact in the same semantic context. A canonical FUNCTION refines only an
unannotated simple variable inferred as CLOSURE. It preserves the actual local
SymbolId, PlaceId, syntax context and declaration range, and synchronizes that
local symbol's facts. Explicit annotations, other initializers and absent shape
facts retain their existing behavior. Target resolution, external tokens and
signature proof remain independent of this structural type.

The two block compiler entries use the existing child type-environment owner.
Normal and failed inner blocks restore the outer callable binding while
canonical symbols, declaration ranges and IR places remain distinct. Root and
independent peer approve those finite changes and complete RAM inverse proofs.
The historical twelve-target replay completes 285/2/0: all 24 typed cases pass;
only two lambda typed-local primitive references fail.

Those failures belong to metadata serialization after the body's type
environment has been restored, rather than to imported callable identity.
The historical parent-DOUBLE/child-INT64 same-name lambda RED produces
10/3/0. All public/canonical child IDs and parent IR prerequisites pass, but the
new child's legacy base type is DOUBLE. The reviewed metadata projection reads
the already-published TypeId after injected-receiver handling and before name
lookup, accepts only PRIMITIVE NULL/BOOL/NUMBER/STRING, and writes that exact
primitive reference without allocation or interning. That scalar baseline is
37A70BA7. The subsequent bare-NOMINAL hook accepts only a module-free node and
the same context's exact TypeId/name/definition-token record, with OBJECT base
and no generic elements or ownership/reference/readonly/nullable qualifiers.
The existing converter copies the matched reference before the context is freed.
FUNCTION, wrappers, imported or unmatched nominal types and generics retain
their existing paths. Production 475D8D8C has Root/peer SOURCE PASS with complete
RAM inverse/forward proof. Lower-r4 passes all 286 native cases, but its driver exits
1 because two foreign Math source files change. Earlier lower-r5 (2026-10-15) exits 0/build 0:
all twelve native targets pass 286/0/0, including lambda 10/0, pre-semantic IR
121/0 and alias hygiene 15/0. Exact unique rows, stable binaries, all 76 source
pins and HEAD af5f4a7bc7de7d22ba545e91dffd982c2d3454bf close this lower replay.
The pre-475D original127 120/7 and focused support33 30/3 results are historical.
The appended nominal and four-array source cases have review approval; RED-r2
completes 35/5/0 before the nominal projection. Fresh lower-r6 then completes
321/1/0 across 14 targets, build 0/wrapper 1, exact unique rows, stable binaries
and all 100 source pins/HEAD stable. Prior twelve targets retain 286/0/0 and
escape metadata passes 14/0; W2 is 21/1, preserving all old 20 passes.

The new W2 case executes with FINE status and integer checksum zero, then fails
only its FILL requirement. Actual SUPER_ARRAY_ADD_INT sites at 42/44/46/48 retain
operandExtra 12 while the burst guard requires USE_RET 65535. This is a bounded
diagnostic of the remaining fusion boundary; no repair or contract waiver is credited.

Original127 has not run after 475D. Original51 and platform validation remain
open; the latest WSL true probe exits -1/E_ACCESSDENIED. M1 is unaccepted and
M2-M6 have not started. Full lower-r6 receipts are in the foundation ledger. See
[typed local scalar metadata](../parser-and-semantics/lambda-semantic-ir-isolation.md#typed-local-scalar-metadata-2026-10-15).

Historical owned LLDB observations explain the earlier 18/1 cyclic failure.
At natural import depth 2, the static zero-argument summary has a structured
INT64 return, no declaration AST and a known empty parameter array. The guard
passes and interns FUNCTION TypeId 3. The member fact carries that same-context
shape while its external target remains unresolved with token/signature/hash 0.
The external alias gate correctly declines target publication, but old ordinary
variable registration projects the legacy CLOSURE/display value into PRIMITIVE
CLOSURE TypeId 4. The later resolver then sees local callback SymbolId 5 with the
wrong kind. Refining the real local shape before IR publication repairs this
loss without treating zero external tokens as proof of an actual target.
Logs `m1-msvc-cyclic-callable-r3-lldb-canonical.log` (C5AF54F5), r4 binding
(D178CD05) and r5 binding-type (6E172C2D) remain under the E reports root with full
hashes in the task manifest. Their owned debugger targets exited. Earlier
pending-validation statements below record historical checkpoints; none
substitutes for original integration, foundation or platform acceptance.

`TYPED_FUNCTION` describes the signature required by a call to a lexical function
value. A parameter such as `callback: fn(int)->int` may contain a different VM
closure, native callable, or AOT callable on each invocation. Its call site binds
the required signature while retaining the live value as the invocation target.

## Compiler Contract

Function, method, and lambda parameters and annotated local variables register
a callable-value signature from the function-type AST. A local initialized by
a call returning a function value retains that signature. The existing callable
overload resolver can therefore check arguments and return values. Canonical
function-type nodes provide a fallback for semantic bindings that already carry
a structured type identity. Neither path hashes a `signatureDisplay` label.

Registration and fallback lookup use the local identifier's syntax context.
An alias reads its initializer's context and registers its destination pattern's
context independently. Equivalent existing metadata is reused only when the
context, signature, and parameter passing modes agree. Named arguments use the
declaration selected by context-aware overload resolution. See
[Macro Token Foundation](../parser-and-semantics/macro-token-foundation.md)
for the compiler hygiene boundary.

Annotation reuse also checks context-comparison status separately from equality.
Failed namespace normalization takes the existing cleanup path and returns
false; it must not fall through to fresh callable registration. Cleanup releases
the temporary parameter types, parameter/mode arrays and return type. This
candidate is statically reviewed, not native GREEN for this failure branch;
the four no-snapshot budget cases do not directly exercise typed annotation
reuse. Public pointer lookups do not gain an error/miss distinction, and no
global allocator-OOM transaction is claimed. The checked comparison contract
is detailed in [LSP canonical bindings](../parser-and-semantics/lsp-typecheck-canonical-binding.md).

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
The macro alias suite additionally checks marked function parameters and
function-valued locals, required typed cache publication, and compile-time
rejection of incompatible arguments in both root and definition contexts.

## Imported Primitive Return Regression (2026-10-14)

The fresh imported-return regression reports 16 tests, one failure and zero
ignored on MSVC. The original twelve cases and the nominal `Token`, closed
generic `Box<u8>` and explicit `u8` controls pass. The primitive provider's
actual return reference has base `INT64` and no type name. The imported member
displays `int`; re-inferring that display name adds a type name to the alias.
The actual typed call therefore requires hash `16702598370128743575`, while
the live provider has hash `12842983158373519980`, and execution reports
`ZR_CALL_BINDING_SIGNATURE_MISMATCH`. This is the same pair observed in the
decorator project failure.

The regression compares the real provider reference, alias canonical return,
typed cache and signature record, then executes the public compiled consumer.
It captures observations before freeing its compiler, AST, roots and VM.
The controls protect nominal spelling, closed-generic arguments and explicit
integer width. They do not claim generic runtime execution.

Evidence is `m1-msvc-imported-return-red-r1-zr_vm_typed_call_binding_test.log`
under `E:/cargo-targets/zr_vm/reports`, SHA256
`369E871145DD19BFC8BAB2999763E220E45A6B2BC7CBFDECDE08601E0AA6051D`,
2524 bytes. The repair projects non-array `null`, boolean, numeric and string
return references directly into the member's owned structured return type in
runtime, binary IO and cyclic-summary constructors. It preserves the original
display name and the provider's actual name, nullability, ownership and width.
Complex, nominal, generic and array inference retains its existing path. These
27 lines extend the existing import metadata assembly; they add no new helper
or parsing responsibility to the large file.

Fresh MSVC validation reports 61 tests passing across typed calls (16), return
inference (14), binding pipeline (18), artifact binding (8) and AOT projection
(5), with zero failures or ignored cases. The actual imported primitive call
now returns 17 and all three type-preservation controls pass. Evidence is
`m1-msvc-imported-return-green-r1-*`; the typed log has SHA256
`C08D63F55233F22B146878D4552F0C4DBA654B7A0875330C31F318C1D1908334`,
2377 bytes. The original127 project replay then reports twelve failures; the
decorator import changes from FAIL to PASS while every other standard outcome
and failure message and the custom generator failure remain unchanged. This
proves the source regression and companion contracts.

A subsequent seventeen-case typed suite also passes with a new binary
primitive provider. A separate builder VM writes its binary bytes and is
destroyed before a fresh consumer VM starts. The consumer loader offers only
binary bytes. The successful binary-load counter must increase during manual
compilation while the natural import compile-info stack names that provider;
the compiled member, canonical return, signature record, live signature and
runtime result 17 must all agree. Thus the test exercises the IO import branch,
rather than crediting only the earlier runtime module load. The generated file
is removed and all borrowed contexts and VM roots are cleaned before assertions.
Evidence is `m1-msvc-imported-binary-return-r1-zr_vm_typed_call_binding_test.log`,
SHA256 `ACDE7EED39E9FFBF677D2ADE00A950028A48AC8588227F1FFBFF29C9A96EE041`,
2501 bytes.

The natural cyclic source fixture then reports 18 tests, one failure and zero
ignored. Its original seventeen cases still pass. A scalar diagnostic captures
the real full-compiled summary function: it has no CALL cache or typed
cache, while the runtime and consumer each have one of both and execute to 17.
The log is `m1-msvc-imported-cyclic-return-diagnostic-r2-zr_vm_typed_call_binding_test.log`,
SHA256 `5D9E0E10E758E50EEB9DAA23B6380C265D2E8B5C1C60700AAE62152F19889E7F`,
2782 bytes. Source backedges now continue into the existing summary path while
registered native providers retain their normal priority and phase checks.
The native suite still reports 18/1/0; its original seventeen cases pass.

A fresh process with the existing startup trace proves summary registration
returns success inside the natural B compilation. The same function completes
body compilation, quickening and summary finalization. This establishes real
summary-producer coverage while preserving the observed missing CALL cache.
The trace is `m1-msvc-cyclic-source-routing-trace-r2-zr_vm_typed_call_binding_test.log`,
SHA256 `10FCD600878346072C45FA469AEDC9F206FBD2F1C2B78522FCD98DFEB441B050`,
1519904 bytes. The static callable projection now accepts a known zero-parameter
metadata record whose parameter array has zero length, including a constructed
empty array without allocated storage. Unknown counts, nonzero invalid arrays,
length mismatches and generic records retain their existing rejection rules.
The canonical signature adapter already supports this empty contract. Native
validation is pending; wider M1 acceptance is not claimed.

The later bounded lower-contract replay still reports 18 tests, one failure
and zero ignored. `test_typed_zero_argument_value_is_guarded` passes; the sole
failure remains the cyclic summary whose returned function has no CALL or
typed-call cache, while the runtime and consumer each have both and execute to
17. This does not establish that the summary producer has been repaired. The
current log is `m1-msvc-m1-lower-contracts-r1-zr_vm_typed_call_binding_test.log`,
SHA256 `5D9E0E10E758E50EEB9DAA23B6380C265D2E8B5C1C60700AAE62152F19889E7F`,
2782 bytes. The typed-call result remains focused evidence only, not M1
acceptance.
