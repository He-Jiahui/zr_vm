# Syntax 12 M6: Artifact, Debug, LSP, and Migration Plan

## Goal

Finish Syntax 12 as a single canonical contract across compiled artifacts,
runtime diagnostics, debug tooling, and the language server. Scheduler policy,
Send/Sync requirements, transport schema, and ABI version must originate from
compiler/type-layout facts. Consumers may project those facts but must never
reconstruct them from a type name, source spelling, runtime value class, or
legacy `TaskRunner` field.

## Starting Point

- `SZrArtifactDomainTransferRow` and `ZrCore_CanonicalConsumer_ResolveDomainTransfer`
  already provide a validated artifact row and consumer projection. Current
  tests build these rows as fixtures; no real source-to-artifact producer yet
  connects the row to the scheduler contract.
- M5 consumes `SZrOwnershipTransferEnvelope` and safely rejects a capture when
  the runtime has no canonical provider metadata. M6 must preserve that
  conservative boundary in binary/import/debug/LSP consumers.
- Legacy `Async`, `TaskRunner`, and `autoCoroutine` state is still present in
  parser/library/thread implementation paths. Its removal is not coupled to
  schema work: each migration slice must have an explicit replacement fact and
  compile/runtime regression before deletion.

## Non-Negotiable Contract

1. A scheduler-facing compiled type is identified by canonical `TypeId`, module
   identity, and artifact token. It is not identified by a display name.
2. Artifact rows retain `DomainTransferKind`, schema version/hash, provider
   token/contract hash, DropOnFailure, scheduler policy, and Send/Sync
   requirements. Producer and importer reject malformed, unavailable, or
   mismatched data before work is scheduled.
3. A runtime provider may choose AttachedDomain or IsolatedDomain only through
   its host policy. It cannot reinterpret a missing artifact row as
   ResourceMove, ImmutableHandle, Send, or Sync.
4. Debug and LSP use the same resolved callable/type projection. Text is a
   rendering of that fact, not an input to a second inference path.
5. Public source remains limited to the reference ledger. No job, scheduler,
   thread, coroutine, or transport keyword is introduced.

## M6.1: Artifact Scheduler Contract

M6.1 is deliberately split into three commits. M6.1a establishes the stable
binary schema and canonical importer. M6.1b.1 closes the imported-provider
identity prerequisite. M6.1b.2 is the only stage permitted to connect compiler
facts and a new artifact writer to that schema. This prevents a fixture-only
row or legacy `.zrb` stream from being described as source-produced metadata.

### M6.1a: Schema and Canonical Consumer

| Layer | Paths | Responsibility |
|---|---|---|
| schema | `zr_vm_core/include/zr_vm_core/artifact_schema.h`, `zr_vm_core/src/zr_vm_core/artifact_encoding.c`, `zr_vm_core/src/zr_vm_core/artifact_schema.c`, `zr_vm_core/src/zr_vm_core/artifact_rows.c` | Define, validate, encode, and decode one fixed-width scheduler contract row containing type tokens, supported provider-policy branches, per-policy Send/Sync requirements, ABI version and transport contract hash. |
| canonical consumer | `zr_vm_core/include/zr_vm_core/canonical_consumer.h`, `zr_vm_core/src/zr_vm_core/canonical_consumer.c` | Resolve a scheduler row by canonical scheduler type token and reject missing, duplicate, policy, capability, ABI, or transport-hash mismatch before scheduling. |
| tests | `tests/parser/test_artifact_schema.c`, `tests/parser/test_canonical_consumers.c` | RED/GREEN fixed-width round trip, canonical lookup, malformed/duplicate ordering rejection, and every expectation mismatch. |
| documentation | this plan, `m6-artifact-debug-lsp-migration.md`, `docs/parser-and-semantics/semantic-fact-layer.md` | State row ownership and that it is a contract envelope, not host-selected state. |

M6.1a does not modify parser/compiler/writer or LSP paths. It records the
host-independent policy *support mask* only. The active AttachedDomain or
IsolatedDomain selection remains the M5 host configuration snapshot.

#### M6.1a Acceptance

Completed 2026-07-25 15:44 +08:00. The fixed 48-byte row, input and decoded
artifact validation, canonical lookup, and structured policy/capability/ABI/
transport mismatch diagnostics passed focused `zr_vm_artifact_schema_test`
(15/15) and `zr_vm_canonical_consumers_test` (17/17) on GCC, Clang, and MSVC,
with a true process exit code of zero for every invocation. The source/binary
writer remains intentionally out of scope until M6.1b.

### M6.1b: Compiler and Binary-Writer Integration

#### Preflight Boundary

The existing `writer_binary.c` serializes the legacy `.zrb` VM function
stream, while `ZrCore_Artifact_Write` currently has no production caller.
M6.1b therefore begins by publishing compiler-owned type/provider artifact
identity. It must not claim a fixture row or a legacy `.zrb` write as a
source-produced artifact. The binary writer integration follows only after
that identity can supply stable local TypeDef and imported TypeRef tokens.

#### M6.1b.1: Imported Scheduler Identity

The scheduler contract is keyed by a nominal provider type. A source module
may own that provider locally (`TypeDef`) or import it from a native/module
artifact (`TypeRef`). It must not use a `TypeSpec`, member token, name, source
spelling, or runtime value category as that key.

| Layer | Paths | Responsibility |
|---|---|---|
| schema and consumer | `zr_vm_core/src/zr_vm_core/artifact_schema.c`, `zr_vm_core/src/zr_vm_core/canonical_consumer.c` | Accept only a nonzero `TypeDef` or `TypeRef` scheduler token; resolve the type identity before returning a scheduler contract. |
| tests | `tests/parser/test_artifact_schema.c`, `tests/parser/test_canonical_consumers.c` | RED/GREEN imported `TypeRef` round trip and canonical resolution; reject `TypeSpec` and member-token scheduler identities. |
| documentation | this plan, `m6-artifact-debug-lsp-migration.md`, `m6-1b-1-imported-scheduler-identity.md`, `docs/parser-and-semantics/semantic-fact-layer.md` | Record that this is an identity prerequisite, not a source artifact writer. |

M6.1b.1 is complete only after the fixed v3 row still round trips and the
consumer resolves an imported `TypeRef` through the type-reference table on
GCC, Clang, and MSVC with real process exit zero. It does not publish compiler
facts or write a production `.zro`/`.zri` artifact.

#### M6.1b.1 Acceptance

Completed 2026-07-25 16:23 +08:00. The row now accepts an exact nonzero
`TypeDef` or `TypeRef` scheduler identity and rejects `TypeSpec` and member
tokens. `ZrCore_CanonicalConsumer_ResolveSchedulerContract` resolves that
identity through the corresponding type table before returning a row. GCC,
Clang, and MSVC each passed `zr_vm_artifact_schema_test` (15/15) and
`zr_vm_canonical_consumers_test` (17/17) with true process exit zero. This
commit does not add a compiler fact, a production `ZrCore_Artifact_Write`
caller, or a source-produced artifact.

#### M6.1b.2: Compiler and Artifact-Writer Integration

### Exact Write Set

| Layer | Paths | Responsibility |
|---|---|---|
| schema consumer | `zr_vm_core/include/zr_vm_core/canonical_consumer.h`, `zr_vm_core/src/zr_vm_core/canonical_consumer.c` | Consume the M6.1a/M6.1b.1 scheduler row as the authoritative import contract. |
| parser artifact projection | `zr_vm_parser/include/zr_vm_parser/artifact_projection.h`, `zr_vm_parser/src/zr_vm_parser/artifact_projection.c` | Build a canonical scheduler contract from resolved type/layout/provider facts; no value-class or identifier fallback. |
| compiler task facts | `zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c`, `compiler_task_effects_internal.h`, `compiler_task_effects_declarations.c` | Publish the resolved Task/Job/Scheduler owner/module, Send/Sync and transport requirements used by projection. |
| source/binary integration | `zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c`, `writer_binary_internal.h`, new artifact-writer module if required | Emit the domain-transfer section and stable scheduler contract only from the canonical projection. The legacy `.zrb` VM stream is not relabeled as an artifact. |
| tests | `tests/parser/test_artifact_schema_source_roundtrip.c`, `tests/parser/test_compiler_features.c` | Cover real source compile -> artifact write -> canonical import, source/import hash equality, mismatched ABI/schema/provider rejection, and unavailable provider rejection. |
| documentation | this plan, `m6-artifact-debug-lsp-migration.md`, `docs/parser-and-semantics/semantic-fact-layer.md` | Record schema ownership, policy fields, and rejection behavior. |

### Steps

1. Add RED tests that compile a scheduler-relevant type from source and require
   a real artifact DomainTransfer row rather than a hand-built fixture.
2. Publish a narrow scheduler-artifact fact from canonical type/layout/provider
   facts. `FORBIDDEN` remains the result if a provider, schema, or owner-module
   identity is unavailable.
3. Serialize the fact through the binary writer; bind it to the exact type
   token and canonical public contract hash.
4. Import through `ZrCore_CanonicalConsumer` and reject policy, ABI,
   schema-version, schema-hash, provider-token, provider-contract, Send, or
   Sync mismatches before a scheduler can consume it.
5. Run focused parser/core tests across GCC, Clang, and MSVC from an isolated
   `HEAD + M6.1b.2 overlay` snapshot, update the M6 record, and exact-path
   commit M6.1b.2 before starting legacy deletion.

## M6.2: Runtime Legacy-Surface Migration

### Exact Write Set

| Layer | Paths | Responsibility |
|---|---|---|
| task/library runtime | `zr_vm_library/include/zr_vm_library/task_runtime.h`, `zr_vm_library/src/zr_vm_library/task_runtime.c`, `zr_vm_library/include/zr_vm_library/project.h`, `zr_vm_library/src/zr_vm_library/project/project.c` | Replace public `TaskRunner`/`autoCoroutine` compatibility state with the explicit Task/Job/Scheduler contract. |
| thread provider | `zr_vm_lib_thread/include/zr_vm_lib_thread/runtime.h`, `zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c`, `runtime_internal.h`, `runtime_workers.c`, `runtime_isolated_domain.c` | Consume the M6.1 artifact contract for policy and transport; delete legacy compatibility paths only after equivalent provider behavior is covered. |
| parser/compiler | `zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c`, `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c`, `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c` | Remove concrete Task-name and automatic-coroutine branches in favor of resolved owner/module/type facts. |
| tests/docs | `tests/thread/test_thread_runtime.c`, `tests/parser/test_compiler_features.c`, `docs/library-and-builtins/zr-thread-scheduler.md`, `docs/parser-and-semantics/*task*.md` | Lock the reference ledger, source rejection/migration behavior, sync-complete allocation, task-frame drop, and provider failure outcomes. |

### Steps

1. Add negative API/compile tests for every legacy surface to be removed and
   positive tests for the replacement canonical path.
2. Migrate parser and import resolution first so runtime code never needs to
   branch on a Task spelling or legacy project flag.
3. Migrate library/thread provider state to the M6.1 imported scheduler
   contract, retaining M4/M5 lifecycle and exactly-once drop assertions.
4. Delete each legacy field/function only when its focused test passes on all
   three toolchains; do not combine unrelated deleted surface with debug/LSP.
5. Exact-path commit M6.2 with an acceptance record listing any pre-existing
   target failures separately from the migrated tests.

## M6.3: Debug Projection and Fault Semantics

### Exact Write Set

| Layer | Paths | Responsibility |
|---|---|---|
| debug core | `zr_vm_core/include/zr_vm_core/debug.h`, `zr_vm_core/src/zr_vm_core/debug_traceback.c`, `zr_vm_core/src/zr_vm_core/task_frame_runtime.c` | Project canonical Task/Job/Scheduler identity, policy, transport state, and fault provenance into debug events/frames. |
| debug library | `zr_vm_lib_debug/src/zr_vm_lib_debug/**` | Serialize only structured debug facts and preserve stable logical async stack links. |
| tests/docs | `tests/debug/test_debug_traceback.c`, `tests/debug/test_debug_agent*.c`, `docs/debugging-and-observability/**` | Prove policy mismatch, transport prepare/decode/commit failure, cancellation/shutdown, and Job throw are distinguishable without source-text inference. |

### Steps

1. Add lower-layer frame/event tests for each M4/M5 terminal transport state.
2. Bind logical async frames to canonical callable/type/artifact identity;
   retain the source range as a projection, not a key.
3. Verify source and imported artifact executions yield the same policy and
   fault contract. Commit M6.3 independently.

## M6.4: Language-Server Projection and Workspace Migration

### Exact Write Set

| Layer | Paths | Responsibility |
|---|---|---|
| LSP implementation | `zr_vm_language_server/**` | Consume published artifact/type/callable facts for diagnostics, hover, signature help, definition and rename. |
| LSP tests | `tests/language_server/**` | Freeze source/binary parity, policy/transport diagnostics, owner module identity, and snapshot-safe edits. |
| LSP docs | `docs/cli-and-tooling/lsp-*.md`, `docs/plans/lsp/**` | Record canonical-fact ownership and toolchain acceptance. |

### Steps

1. Establish an LSP-only RED snapshot after M6.1/M6.2 are committed; no LSP
   production code is changed during earlier M6 stages.
2. Add projections for canonical owner module, scheduler policy, Send/Sync,
   transport schema/ABI and fault diagnostics. Do not use member name,
   filename, raw AST, diagnostic message, or display text as a fallback key.
3. Run the independent LSP matrix and stdio/CLI smoke tests, write a separate
   LSP completion record, then exact-path commit M6.4.

## Promotion Gate

M6 is complete only when a real source-produced artifact is the single source
of scheduler policy/transfer requirements for import, runtime, debug, and
LSP; legacy `Async`/`TaskRunner`/`autoCoroutine` public compatibility surface
and concrete Task-name checks are removed; every terminal transport branch
remains faulted/moved/exactly-once as specified by M4/M5; and the final
GCC/Clang/MSVC plus LSP protocol evidence is recorded with true process exits.
