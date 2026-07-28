---
related_code:
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/debug_evaluation_context.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
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
  - zr_vm_core/src/zr_vm_core/debug_evaluation_context.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
tests:
  - tests/debug/test_debug_metadata.c
  - tests/debug/test_debug_introspection.c
  - tests/module/test_reflection_dynamic_generic_instance_interpreter.h
  - tests/module/test_reflection_dynamic_generic_method_context.h
  - docs/plans/lsp/04-debug-and-repl/2026-07-28-e1a-canonical-local-binding-artifact.md
  - docs/plans/lsp/04-debug-and-repl/2026-07-28-e1b1-paused-frame-canonical-bindings.md
doc_type: module-detail
---

# Debug Canonical Local Bindings And Paused Frames

## Scope

The typed-local metadata row is the artifact boundary for debugger and LSP
frame-context reconstruction. Each source local can carry its compiler-owned
canonical `SymbolId`, `TypeId`, `PlaceId`, and whole declaration range through a
compiled function and its `.zro` representation. This metadata is an identity
projection, not a second binding pass.

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

Patch 38 appends `roleFlags` after those fields. Its currently defined bit is
`ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER`. The compiler sets that bit only for
the injected instance receiver: an active function declaration with a
non-`NONE` receiver effect whose first compiler local occupies stack slot zero.
It does not inspect the local name. The writer emits the role flags after the
identity fields; the IO reader accepts them only from patch 38 or newer. Older
artifacts receive zeroed identity and role fields, which means frame
reconstruction is unavailable rather than guessed. Runtime loading copies the
row unchanged into the executable function.

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
