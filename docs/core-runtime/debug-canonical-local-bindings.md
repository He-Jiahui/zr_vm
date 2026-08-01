---
related_code:
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/debug_evaluation_closure.c
  - zr_vm_core/src/zr_vm_core/debug_evaluation_context.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_closure_identity.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_formal_evaluation_execute.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_bindings.c
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_closure.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_closure_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/debug_evaluation_closure.c
  - zr_vm_core/src/zr_vm_core/debug_evaluation_context.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_closure_identity.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_formal_evaluation_execute.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_bindings.c
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_closure.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_closure_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
  - user: 2026-08-01 optimize AOT 07-12 and record every completed sub-milestone
tests:
  - tests/debug/test_debug_metadata.c
  - tests/debug/test_debug_introspection.c
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/module/test_reflection_dynamic_generic_instance_interpreter.h
  - tests/module/test_reflection_dynamic_generic_method_context.h
  - tests/parser/test_semir_pipeline.c
  - tests/parser/test_aot_parameter_passing_form_roundtrip_cases.h
  - tests/acceptance/2026-08-01-aot-07-parameter-source-passing-form-projection.md
  - docs/plans/lsp/04-debug-and-repl/2026-07-28-e1a-canonical-local-binding-artifact.md
  - docs/plans/lsp/04-debug-and-repl/2026-07-28-e1b1-paused-frame-canonical-bindings.md
  - docs/plans/lsp/04-debug-and-repl/2026-08-01-e2b3-generation-checked-runtime-root.md
  - docs/plans/lsp/04-debug-and-repl/2026-08-01-e2b5-generation-checked-runtime-root-consumer.md
  - docs/plans/lsp/04-debug-and-repl/2026-08-02-e2b6a-canonical-closure-capture-artifact-identity.md
  - docs/plans/lsp/04-debug-and-repl/2026-08-02-e2b6b-generation-checked-paused-frame-closure-resolver.md
doc_type: module-detail
---

# Debug Canonical Local Bindings And Paused Frames

## Scope

The typed-local metadata row is the artifact boundary for debugger and LSP
frame-context reconstruction. Each source local can carry its compiler-owned
canonical `SymbolId`, `TypeId`, `PlaceId`, and whole declaration range through a
compiled function and its `.zro` representation. Closure captures use the same
identity discipline through a separate typed sidecar. Neither carrier is a
second binding pass.

## Source Projection

`compiler_semantic_ir_register_local` already constructs a local Place from the
canonical type-environment binding. `compiler_semantic_ir_get_slot_identity`
exposes only the corresponding slot identity when all of the TypeId, SymbolId,
and PlaceId are valid. The declaration range comes from that Place. Consumers
must treat a missing identity as unavailable; they must not recover it from a
local name, stack slot, or static display type.

`compiler_build_typed_local_bindings` copies that identity into
`SZrFunctionTypedLocalBinding` alongside the existing display-oriented type
projection. The existing type projection remains useful for legacy metadata but
is not a replacement for canonical `TypeId`.

## Binary Contract

`.zro` source patch 37 appends seven fixed-width identity fields to every typed-local row:

1. `symbolId`
2. `typeId`
3. `placeId`
4. declaration start line and column
5. declaration end line and column

Patch 38 appends `roleFlags` after those fields. Bit zero is
`ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER`; bits one through seven are a one-hot
projection of source parameter passing form: `VALUE`, `IN`, `REF`,
`REF_READONLY`, `SCOPED_REF`, `SCOPED_REF_READONLY`, or `OUT`. The compiler sets
the receiver bit only for the injected instance receiver at stack slot zero. It
sets a passing-form bit only for an explicit parameter in the producer-order
local prefix, adjusted by the receiver offset and checked against the matching
AST parameter name. Later same-name locals remain role-free.

The writer emits the role flags after the identity fields; the IO reader accepts
them only from patch 38 or newer. This is a semantic extension of the existing
fixed-width field, not a new binary field or patch. Older artifacts receive
zeroed identity and role fields, so parameter passing form remains unknown
rather than being guessed. Runtime loading copies the row unchanged into the
executable function. A receiver bit combined with any passing-form bit,
multiple passing-form bits, unknown bits, partial explicit-parameter
availability, or a passing-form bit after the parameter prefix is malformed.

## Closure Capture Artifact Contract

E2b6a keeps the legacy `SZrFunctionClosureVariable` wire row unchanged. Its
compiler-only in-memory tail is never written as part of that row. Instead,
source patch 41 writes a `SZrFunctionTypedClosureBinding` sidecar after typed
local bindings and before typed exported symbols. Each row carries the legacy
capture index, the complete canonical type projection, source `SymbolId`,
canonical `TypeId`, and the whole source declaration range.

`compiler_closure_capture_identity_from_parent` freezes this data from the
exact parent TypeEnvironment binding or matching pre-SemIR slot identity. A
nested capture may reuse only an already frozen parent capture identity. The
typed-sidecar builder accepts a recovered type only when its `SymbolId` and
`TypeId` are exact matches; it does not search a local or capture name. Lambda
and named nested-function compilation both preserve the parent pre-SemIR
snapshot needed for that lookup.

`ZrCore_Function_GetClosureCaptureIdentity` is the public runtime query. It
joins a requested capture slot to exactly one sidecar row, checks nonzero IDs
and a valid range, and returns the canonical type and identity. Missing,
duplicate, incomplete, or inconsistent rows clear all outputs and return
false. Runtime loading zero-initializes the extended legacy capture storage
before filling its serialized fields, so nonserialized compiler-only tail
bytes cannot accidentally look like an identity.

The sidecar TypeRef participates in GC mark, young-reference detection, and
compact relocation. It therefore remains valid when its type projection holds
managed references. E2b6a itself does not publish a paused-frame generation
token, parser reference origin, or Debug evaluator execution path for captures.

## Paused Closure-Capture Resolver

E2b6b adds `ZrCore_Debug_EvaluationContext_GetClosureCapture` and
`ZrCore_Debug_EvaluationContext_ResolveClosureCapture`. Both first reuse the
paused-frame validation for exact active call-info membership, VM-frame kind,
function identity, frame generation, and instruction offset. The resolver then
requires the active callable to be the exact VM closure for that function,
requires its capture-cell count to match the function metadata, and joins the
requested capture index to exactly one E2b6a sidecar identity row.

The returned binding carries only the capture index, canonical TypeRef,
SymbolId, TypeId, whole declaration range, and the current frame generation
token. Resolve clears its output before validation, requires the exact token
and every identity field to match a freshly regenerated binding, then snapshots
the matching closure cell. Resumed, retired, reused, changed-PC, trimmed, or
incomplete frames fail closed; missing cells or metadata are unavailable. The
resolver never calls legacy upvalue name APIs or searches a capture name, stack
slot, AST, display type, or text.

E2b6b is runtime-only. E2b6c must still publish parser closure-origin and
token facts, and E2b6d must make the formal Debug consumer use those facts.
Until then the resolver is not permission for a consumer fallback.

## Consumer Boundary

The E1a metadata carrier and E1b1 paused-frame query form the only current
debug binding boundary. A frame receives a nonzero generation at every normal
or stack-local native initialization and again when a tail-call frame is
reused. `ZrCore_Debug_GetEvaluationContext` captures the exact VM activation,
generation, program counter, and canonical active binding count. Its binding
query revalidates call-info membership before dereferencing the saved pointer,
then requires the same generation, function metadata, and program counter.

Visible bindings use the debug local PC interval for liveness and find their
identity only in the E1a typed-local row for the exact stack slot. A missing or
zero SymbolId, TypeId, or PlaceId is metadata unavailable. A stale generation,
reused frame, changed PC, or retired call-info fails closed. Neither path may
recover identity from a name, stack slot alone, AST, display type, or text.

`ZrCore_Debug_EvaluationContext_GetReceiver` is the E1b2a receiver projection.
It revalidates the paused frame, requires exactly one active typed-local row
with the `RECEIVER` role and a complete canonical identity, and snapshots the
exact frame slot. A free/static frame with complete typed-local metadata returns
`NO_RECEIVER`; missing rows, duplicate receiver rows, inactive receiver rows,
or incomplete identities return `METADATA_UNAVAILABLE`. It never derives a
receiver from a local name, member name, display type, AST, or text.

`ZrCore_Debug_EvaluationContext_GetGenericArgument` is the E1b2b1 reflection
metadata projection. It first revalidates the paused activation, frame
generation, and PC, then accepts only a structured context kind, owner metadata
token, and parameter index. Type and method generic contexts use the existing
public reflection call-info resolvers; missing context, wrong owner, and absent
metadata return `METADATA_UNAVAILABLE`. The returned type object is borrowed and
may be consumed only while the same paused context remains current. The query
does not inspect reflection-private fields or recover generic arguments from a
name, display string, AST, or hidden accessor.

E1b2b1 is deliberately limited to reflection type-object metadata. Source
`TypeId` substitutions and const-generic values are not retained by a stable
call-frame or artifact fact, so the runtime must not fabricate a carrier for
them. E1 is complete with the plan's required generic context; E4 may transport
a TypeId only after a canonical fact publishes it. Formal parser/binder reuse,
effect policy, result transport, and REPL transport remain E2 through E5. In
particular, `zr_vm_lib_debug/debug_eval.c` must not use these fields to justify
its independent expression parser. E2a now provides the formal parser fragment
entry that E2b must consume.

E2b3 adds the runtime-root carrier needed by formal evaluation without turning
the global root into a synthetic local. `ZrCore_Debug_EvaluationContext_GetRuntimeRoot`
accepts the structured `ZR_DEBUG_RUNTIME_ROOT_ZR` role and returns an opaque,
nonzero token only after the paused activation, frame generation, function,
and program counter are revalidated. `ZrCore_Debug_EvaluationContext_ResolveRuntimeRoot`
repeats that validation, requires the exact token for the same context, and
returns a borrowed debug snapshot of `global->zrObject`. A modified token or a
retired/reused frame returns `STALE_FRAME`; a missing runtime root returns
`METADATA_UNAVAILABLE`.

The token is an identity carrier, not an address or semantic name. Consumers
must select the root by the structured runtime-root kind and must not inspect
the token, compare `zr` text, access `global->zrObject` directly, or manufacture
a local Place.

E2b5 connects that carrier to formal Debug evaluation. Binding injection asks
the paused evaluation context for `ZR_DEBUG_RUNTIME_ROOT_ZR`, resolves the
current borrowed value only to construct its inferred type, and registers the
parser binding through `ZrParser_TypeEnvironment_RegisterRuntimeRoot`. The
reference receives query-local `SymbolId` and `TypeId`, structured root kind,
and the opaque generation token. It has no source declaration range and no
function-local Place. An active source binding with the same surface spelling
remains authoritative and prevents root injection.

Formal execution dispatches from `SZrSemanticReferenceFact.originKind`.
`SOURCE_DECLARATION` requires exact SymbolId, TypeId, and PlaceId equality with
the paused frame row. `RUNTIME_ROOT` requires the structured `ZR` role, nonzero
token, empty Place/declaration/definition identity, and successful
`ZrCore_Debug_EvaluationContext_ResolveRuntimeRoot` validation against the
same paused frame generation. `CLOSURE_CAPTURE` remains unavailable until its
artifact identity and generation-checked resolver are published. None of these
branches resolves by identifier spelling, stack-slot scan alone, AST shape, or
raw global pointer.

E2b1 consumes this boundary for Debug semantic inference. The internal binder
asks `ZrCore_Debug_GetEvaluationContext` for the selected frame, enumerates
only its active bindings through the generation-validated query, and matches
each row to its exact typed-local definition by stack slot and active local
name. It registers the structured type together with the published `SymbolId`,
`TypeId`, and declaration range through
`ZrParser_TypeEnvironment_RegisterCanonicalVariable`. A stale, trimmed,
missing, or mismatched row stops semantic inference for that frame; raw value
slots, names, ASTs, display types, and text cannot supply a replacement
identity. The no-active-frame test harness path remains separate from a paused
frame request and does not make unavailable frame metadata appear valid.

## Validation

`test_binary_roundtrip_preserves_canonical_local_binding_identity` compiles a
function, writes it to `.zro`, reads it, loads runtime metadata, and compares the
canonical IDs and declaration range at both boundaries.

`test_getlocal_and_setlocal_walk_active_locals_by_index` captures the target
frame, verifies its exact typed-local canonical identity, excludes a caller
local that is inactive at the paused PC, and verifies that a context retained
across tail-frame reuse is rejected as stale. It first verifies `NO_RECEIVER`
for the free-function frame, then uses a test-only role bit on that exact active
canonical row to exercise receiver projection and frame-value snapshotting. The
test is not a compiler provenance substitute: the artifact roundtrip test owns
that coverage.

The introspection test also resolves the structured `ZR` runtime root, compares
its borrowed snapshot with the core global root, rejects a modified token, and
rejects the captured token after the frame returns. The E2b3 RED failed to
compile because the runtime-root contract did not exist. The final MSVC runner
passes 2/2 with exit 0; GCC and Clang compile the exact core and test objects
with exit 0. The full E3 acceptance matrix remains responsible for executing
the consumer integration on all three toolchains.

`test_debug_formal_evaluation_resolves_generation_checked_runtime_root` pauses
inside a real VM activation with `zr` holding a byte array. It requires a
resolved runtime-root reference with query-local IDs, no source identity or
Place, and the exact token returned by the core binding query. Injecting a
definition range or mutating only the fact token while leaving the `zr`
spelling unchanged must make formal execution unsupported; restoring the
canonical fact evaluates `zr[1]` as `uint 98`.
The final fixed snapshot includes the clean Syntax05 Task4 property bootstrap
support from ancestor commit `3d67352` and the 14 exact Debug code/test
overlays. MSVC 19.44 (Visual Studio 17.14.36), GCC 11.4.0, and Clang 14.0.0
each configure and build a fresh static target, then pass the complete
`zr_vm_debug_expression_diagnostics_test` runner with 52 tests, zero failures,
zero ignored, and process exit 0. The copied WSL snapshot matches all 14
overlay SHA-256 hashes.
The later Task4 completion commit `3c4c172` only rejects null bootstrap
arguments; this Debug path supplies non-null compiler and function inputs, so
the validated integration path is unchanged.

`test_debug_source_binding_shadows_runtime_root_spelling` keeps the runtime root
available while pausing in a parameter named `zr`. The resolved fact remains a
`SOURCE_DECLARATION` with the exact frame Place and cleared root kind/token, and
formal evaluation reads the parameter value. This freezes source shadowing
without turning identifier spelling into runtime-root identity.

`test_binary_roundtrip_preserves_canonical_receiver_binding_role` finds the
receiver row through published `SZrCompiledPrototypeInfo` and
`SZrCompiledMemberInfo.functionConstantIndex` records, once in the source
function and once in a distinct runtime function loaded only from the written
`.zro` in the initialized test state. It compares role, canonical identity, and
declaration range without member names, hidden-accessor names, AST pairing, or
compiler-private trees. On 2026-07-28, GCC, Clang, and a fresh MSVC shared
build each passed `zr_vm_debug_metadata_test` (6 tests) and
`zr_vm_debug_introspection_test` (2 tests) with real exit 0.

`test_interpreter_generic_instance_executes_resolved_vm_method_with_context`
and `test_method_spec_executes_resolved_vm_function_with_context` query type
and method generic arguments from their paused trace hooks. They compare the
returned object against the existing public reflection resolver, reject the
opposite context kind and wrong owner token, and verify that the captured
context is stale after the frame returns. On 2026-07-28, GCC, Clang, and a fresh
MSVC shared build each passed `zr_vm_reflection_dynamic_generic_instance_test`
(35 tests) and `zr_vm_debug_introspection_test` (2 tests) with real exit 0.

`test_debug_semantic_binding_preserves_paused_frame_canonical_identity` pauses
inside a function parameter named `paused`, formally parses the expression
`paused`, and compares the inferred read reference with the active frame row's
exact `SymbolId`, `TypeId`, and declaration start. On 2026-07-29, GCC, Clang,
and MSVC each built and ran `zr_vm_debug_expression_diagnostics_test` with 34
tests, zero failures, and a real zero exit code.

`test_parameter_passing_forms_roundtrip_into_exec_ir` compiles all seven source
passing forms plus a real instance method. It verifies the free-function roles,
the instance receiver at slot zero, explicit VALUE/IN roles at slots one/two,
and the same rows after `.zro` runtime loading. The adjacent prefix regression
proves a nested local that shadows a parameter name remains role-free.

`test_binary_roundtrip_preserves_canonical_closure_capture_identity` compiles
an array capture, requires an exact sidecar identity at source and after a
`.zro` read/runtime load, and verifies invalid capture-index output clearing.
`test_source_preserves_canonical_named_closure_capture_identity` repeats the
source identity check for a named nested function. The E2b6a focused matrix
also runs `zr_vm_gc_concurrent_major_test`, exercising collection and compact
relocation after the sidecar TypeRef becomes part of a function graph.
