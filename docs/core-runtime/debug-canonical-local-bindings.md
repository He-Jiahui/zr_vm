---
related_code:
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/debug.c
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

E1b1's generic presence flags remain availability-only. Structured generic
type/value substitutions are E1b2b work. Formal parser/binder reuse, effect
policy, result transport, and REPL transport remain E2 through E5. In
particular, `zr_vm_lib_debug/debug_eval.c` must not use these fields to justify
its independent expression parser.

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
