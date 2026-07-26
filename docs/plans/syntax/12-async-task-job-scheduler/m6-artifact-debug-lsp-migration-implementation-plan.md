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

M6.1b.2 is split into two commits because source call resolution and artifact
serialization have different ownership and failure boundaries. M6.1b.2a
persists only a resolved source scheduler fact. It does not invent an artifact
token and does not write a file. M6.1b.2b is allowed to emit a `.zri` or
`.zro` document only after that fact has been joined with an exact local
`TypeDef` or imported `TypeRef` row.

#### M6.1b.2a: Compiler Scheduler Source Fact

| Layer | Paths | Responsibility |
|---|---|---|
| compiled-function fact | `zr_vm_core/include/zr_vm_core/function.h`, `zr_vm_core/src/zr_vm_core/function.c` | Retain and release a compact source scheduler fact containing canonical scheduler `TypeId`, resolved schedule member token/signature/hash, protocol mask, and contract role. |
| compiler fact publisher | new `zr_vm_parser/src/zr_vm_parser/compiler/compiler_scheduler_artifact.c`, `compiler_internal.h`, `type_inference_native.c` | Publish a fact only from a resolved receiver call with `TASK_SCHEDULER_SCHEDULE` role and canonical receiver type. Duplicate facts coalesce structurally; missing identity stays unavailable. |
| tests | `tests/parser/test_artifact_schema_source_roundtrip.c`, `tests/CMakeLists.txt` | Compile a real `zr.thread.ThreadScheduler.schedule` call, assert its structured fact, and reject a non-scheduler receiver without a fact. |
| documentation | this plan, `m6-artifact-debug-lsp-migration.md`, `docs/parser-and-semantics/semantic-fact-layer.md`, `m6-1b-2a-compiler-scheduler-source-fact.md` | Record that the fact is a compiler product, not yet a serializable artifact row. |

M6.1b.2a does not use a provider name, source spelling, legacy `.zrb` bytes,
or a runtime value category as an artifact key. It also does not claim that
the scheduler `TypeId` has an artifact `TypeDef`/`TypeRef` token. That join is
the explicit entry condition for M6.1b.2b.

#### M6.1b.2a Acceptance

Completed 2026-07-25 17:19 +08:00. A real
`zr.thread.ThreadScheduler.schedule` source call now retains exactly one
compiler-owned source fact per structural scheduler contract. The fact carries
the canonical scheduler `TypeId`, deterministic native member/signature
tokens, signature hash, TaskScheduler protocol mask, and the resolved
`TASK_SCHEDULER_SCHEDULE` contract role. A source module with no scheduler call
retains no fact. GCC, Clang, and MSVC each passed
`zr_vm_artifact_schema_test` (18/18) and
`zr_vm_canonical_consumers_test` (17/17) with true process exit zero. This
acceptance does not serialize a `.zri`/`.zro` artifact or relax the exact
TypeDef/TypeRef join required by M6.1b.2b.

#### M6.1b.2b: Artifact Writer Integration

### Exact Write Set

| Layer | Paths | Responsibility |
|---|---|---|
| schema consumer | `zr_vm_core/include/zr_vm_core/canonical_consumer.h`, `zr_vm_core/src/zr_vm_core/canonical_consumer.c` | Consume the M6.1a/M6.1b.1 scheduler row as the authoritative import contract. |
| parser artifact projection | `zr_vm_parser/include/zr_vm_parser/artifact_projection.h`, `zr_vm_parser/src/zr_vm_parser/artifact_projection.c` | Build a canonical scheduler contract from resolved type/layout/provider facts; no value-class or identifier fallback. |
| compiler task facts | `zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c`, `compiler_task_effects_internal.h`, `compiler_task_effects_declarations.c`, M6.1b.2a source fact | Join the resolved Task/Job/Scheduler owner/module, Send/Sync and transport requirements to the persisted source fact. |
| source/binary integration | `zr_vm_parser/include/zr_vm_parser/writer.h`, new `zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c` | Emit the domain-transfer section and stable scheduler contract only from the complete compiler-owned provider facts. `writer_binary.c` remains the legacy `.zrb` VM stream and is not relabeled as an artifact. |
| source-fact completion | `zr_vm_core/include/zr_vm_core/function.h`, `compiler_scheduler_artifact.c`, `compiler_internal.h`, `type_inference_native.c`, `type_inference_core.c` | Preserve resolved Task/Job/Scheduler provider TypeDef/signature/layout/module identities, including native-import and closed-generic provenance, before artifact projection. |
| tests | `tests/parser/test_artifact_schema.c`, `tests/parser/test_artifact_schema_source_roundtrip.c` | Cover real source compile -> artifact write -> canonical import, source/import hash equality, mismatched ABI/schema/provider rejection, and unavailable provider rejection. |
| documentation | this plan, `m6-artifact-debug-lsp-migration.md`, `docs/parser-and-semantics/semantic-fact-layer.md` | Record schema ownership, policy fields, and rejection behavior. |

### Steps

1. M6.1b.2a adds RED tests that compile a scheduler-relevant source call and
   require a resolved compiler fact rather than a name or runtime-value guess.
2. M6.1b.2b adds RED tests requiring a real artifact DomainTransfer row rather
   than a hand-built fixture. `FORBIDDEN` remains the result if a provider,
   schema, or owner-module identity is unavailable.
3. Serialize the joined fact through the new artifact writer; bind it to the exact type
   token and canonical public contract hash.
4. Import through `ZrCore_CanonicalConsumer` and reject policy, ABI,
   schema-version, schema-hash, provider-token, provider-contract, Send, or
   Sync mismatches before a scheduler can consume it.
5. Run focused parser/core tests across GCC, Clang, and MSVC from an isolated
   `HEAD + M6.1b.2 overlay` snapshot, update the M6 record, and exact-path
   commit M6.1b.2 before starting legacy deletion.

#### M6.1b.2b Acceptance

Completed 2026-07-25 21:55 +08:00. The source compiler retains complete
Scheduler, Task, and Job provider TypeDef/signature/layout/module identities,
including native-import and closed-generic provenance. The new canonical
writer projects only those facts into a real `.zro` document through
`ZrCore_Artifact_Write`; it neither invokes nor relabels the legacy `.zrb`
stream. The real source roundtrip proves scheduler contract hash equality and
rejects ABI, policy, requirements, transport, scheduler-contract, provider,
and unavailable-provider failures. On the isolated `e04719a + M6.1b.2b`
overlay, GCC, Clang, and MSVC each passed `zr_vm_artifact_schema_test` 21/21
and `zr_vm_canonical_consumers_test` 17/17 with real process exit zero.

## M6.2: Runtime Legacy-Surface Migration

### Exact Write Set

| Layer | Paths | Responsibility |
|---|---|---|
| task/library runtime | `zr_vm_library/include/zr_vm_library/task_runtime.h`, `zr_vm_library/src/zr_vm_library/task_runtime.c`, `zr_vm_library/include/zr_vm_library/project.h`, `zr_vm_library/src/zr_vm_library/project/project.c` | Replace public `TaskRunner`/`autoCoroutine` compatibility state with the explicit Task/Job/Scheduler contract. |
| thread provider | `zr_vm_lib_thread/include/zr_vm_lib_thread/runtime.h`, `zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c`, `runtime_internal.h`, `runtime_workers.c`, `runtime_isolated_domain.c` | Consume the M6.1 artifact contract for policy and transport; delete legacy compatibility paths only after equivalent provider behavior is covered. |
| parser/compiler | `zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c`, `parser_types.c`, `type_inference/type_inference_native.c`, `compiler/compile_expression_types.c`, `compiler_task_effects.c`, `compiler_reference_escape.c` | Reject legacy task parser forms and remove hidden-await compatibility recognition; derive async boundaries only from explicit AST Task/await facts. |
| tests/docs | `tests/thread/test_thread_runtime.c`, `tests/parser/test_compiler_features.c`, `docs/library-and-builtins/zr-thread-scheduler.md`, `docs/parser-and-semantics/*task*.md` | Lock the reference ledger, source rejection/migration behavior, sync-complete allocation, task-frame drop, and provider failure outcomes. |

#### M6.2 Discovery Additions

The first M6.2 baseline run found that the legacy bridge is also reachable
through parser pseudo-type sugar, dedicated task-runtime regression fixtures,
and documentation that still teaches the removed public surface. They are
therefore part of this milestone's exact write set rather than deferred
cleanup:

| Layer | Paths | Responsibility |
|---|---|---|
| parser compatibility removal | `zr_vm_parser/src/zr_vm_parser/parser/parser_types.c`, `tests/parser/test_parser.c` | Reject `%async T` pseudo-type sugar and remove its `TaskRunner<T>` AST expectation; preserve explicit `async fn ...: Task<T>` parsing. |
| task runtime regressions | `tests/task/test_task_runtime.c` | Replace legacy TaskRunner/start/pump/default-scheduler coverage with negative public-surface tests and canonical `Job`/`Scheduler.schedule` execution coverage. |
| thread runtime regressions | `tests/thread/test_thread_runtime.c` | Replace Thread/Scheduler/start wrapper assertions with `ThreadScheduler.schedule(Job)` provider and lifecycle coverage; delete legacy `%async` runner test positives. |
| source ledger and module docs | `docs/zr_language_specification.md`, `docs/library-and-builtins/index.md`, `docs/library-and-builtins/zr-task-runtime.md`, `docs/library-and-builtins/zr-coroutine-runtime.md`, `docs/library-and-builtins/zr-thread-runtime.md`, `docs/library-and-builtins/zr-task-job-scheduler.md`, `docs/parser-and-semantics/async-task-syntax-and-effect.md`, `docs/parser-and-semantics/index.md` | Remove obsolete `%async`/`%await`, `zr.coroutine`, `TaskRunner`, `autoCoroutine`, `start`, and source `pump/step` guidance; retain the explicit Task/Job/Scheduler ledger. |

`zr_vm_lib_task` is not added: the root CMake project does not build or
register that historical module, and the production built-in registration path
is `ZrCore_TaskRuntime_RegisterBuiltins`. Its obsolete source is not a public
surface in this configured product graph.

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

#### M6.2 Acceptance

Completed 2026-07-25 23:20 +08:00. The configured product graph now exposes
only the canonical `zr.task` `Task`/`Job`/`Scheduler` contract and the
`zr.thread.ThreadScheduler.schedule(Job)` provider path. `TaskRunner`,
`autoCoroutine`, `zr.coroutine`, source `pump`/`step`, `%async`, `%await`,
and `%async T` are rejected or unavailable; compiler task and reference-escape
analysis derives await boundaries only from direct AST facts. The private
runtime scheduler remains an implementation detail used to complete canonical
Task results, not a script-visible public API. Task and thread regressions
retain the M4/M5 lifecycle, transport, cancellation, fault, and exactly-once
drop coverage. In isolated GCC 11.4, Clang 14.0, and MSVC 17.14 builds,
`zr_vm_parser_test` passed 75/75, `zr_vm_task_runtime_test` passed 16/16, and
`zr_vm_thread_runtime_test` passed 25/25; every test process exited zero.
`zr_vm_lib_task` remains intentionally excluded because the root CMake product
graph neither builds nor registers that historical module.

## M6.3: Debug Projection and Fault Semantics

### Exact Write Set

| Layer | Paths | Responsibility |
|---|---|---|
| debug core | `zr_vm_core/include/zr_vm_core/debug.h`, `zr_vm_core/src/zr_vm_core/debug_traceback.c`, `zr_vm_core/include/zr_vm_core/task_frame_runtime.h`, `zr_vm_core/src/zr_vm_core/task_frame_runtime.c` | Project canonical Task/Job/Scheduler identity, policy, transport state, and fault provenance into debug events/frames. |
| debug library | `zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h`, `zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c`, `zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c` | Serialize only structured debug facts and preserve stable logical async stack links. |
| tests/docs | `tests/debug/test_debug_traceback.c`, `tests/debug/test_debug_agent_protocol.c`, `docs/cli-and-tooling/zr-debugger-v1-launch-workflow.md`, this plan, M6 status/record | Prove policy mismatch, transport prepare/decode/commit failure, cancellation/shutdown, and Job throw are distinguishable without source-text inference. The planned `docs/debugging-and-observability/` directory does not exist in this repository; the maintained debugger protocol document is the replacement documentation location. |

### Steps

1. Add lower-layer frame/event tests for each M4/M5 terminal transport state.
2. Bind logical async frames to canonical callable/type/artifact identity;
   retain the source range as a projection, not a key.
3. Verify source and imported artifact executions yield the same policy and
   fault contract. Commit M6.3 independently.

#### M6.3 Acceptance

Completed 2026-07-26 00:15 +08:00. Core now projects a scheduler source fact
and an imported scheduler artifact row into one structured debug contract keyed
by Scheduler/Task/Job TypeDef-or-TypeRef tokens, resolved schedule member and
signature identities, ABI, policy, requirements, transport hash and contract
hash. Source range remains solely the ordinary stack-frame presentation field.
The debug agent serializes the source projection as `stackTrace[].asyncContract`;
all 64-bit hashes use fixed-width hexadecimal strings, so JSON number precision
cannot alter contract identity. TaskFrame terminal projection distinguishes
AttachedDomain/IsolatedDomain completion and fault terminals and accepts only a
structured fault provenance: policy rejection, transport prepare/decode/commit,
cancellation, shutdown, or Job throw. It never parses runtime error text.

In isolated GCC 11.4, Clang 14.0 and MSVC 17.14 builds,
`zr_vm_debug_traceback_test` passed 5/5,
`zr_vm_debug_agent_protocol_test` passed 5/5, and
`zr_vm_thread_runtime_test` passed 25/25. All nine test process exit codes were
zero. The thread test deliberately prints an expected compile-time Unique-use
rejection while asserting it, so its Unity result remains 25/25 with exit zero.

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

#### M6.4 Acceptance

Completed 2026-07-26 03:16 +08:00 with known pre-existing LSP baseline
failures recorded separately. `ZrLanguageServer_LspSchedulerContract_ResolveArtifact`
now resolves a real compiler-owned scheduler source fact and its `.zro` bytes
only through `ZrCore_CanonicalConsumer`: canonical receiver TypeId, serialized
TypeRef, resolved schedule signature, ABI, full policy requirements, owner
layout/module identity, transport hash, and scheduler contract hash must all
agree. It returns canonical structured diagnostics for module, policy, and
transport mismatch; it does not read source text or synthesize a fallback from
member name, filename, AST, display text, or diagnostic message.

The source/binary regression passed on GCC 11.4, Clang 14.0, and MSVC 17.14.
Each toolchain's independent 18-target LSP matrix produced 16 real exit-zero
processes and the same two nonzero existing targets:
`zr_vm_language_server_local_semantic_hover_test` and
`zr_vm_language_server_reachability_semantic_query_test`. All nine stdio/CLI
smokes (main, position encoding, and diagnostic fix on each toolchain) exited
zero. The nonzero targets remain outside this projection's write set, so M6's
promotion gate remains `in_progress` until their baseline failures are closed.

#### M6.4a Reachability Fact Span Follow-up

Completed 2026-07-26 03:45 +08:00. The two nonzero reachability targets were
root-caused to a projection range mismatch: the variable-declaration AST node
covered only the `var` keyword while the LSP query position was the declared
identifier. The semantic analyzer now derives the reachability fact range by
merging the declaration, pattern, available type-name, and initializer AST
ranges. The query remains fact-based; it does not search source text or infer
a range from a variable name. On the same isolated snapshot, GCC 11.4, Clang
14.0, and MSVC 17.14 each ran all 18 LSP test processes with true exit zero.
All nine stdio/CLI smokes also exited zero. The identical six Unity assertion
markers in receiver completion/hover, foreach shadowing, container matrix,
native constructor, and interface-variance tests remain recorded separately;
their test processes exit zero but they keep the overall M6 promotion gate
`in_progress`.

#### M6.4b Canonical Generic Projection Convergence

Completed 2026-07-26 08:13 +08:00. M6.4a's six remaining Unity markers are
closed through canonical facts: source-import array TypeRefs, native generic
iterables and closed generic prototypes retain their `Iterable` protocol;
const generic parameter references and literal values retain distinct
`CONST_PARAMETER`/`CONST_INT` kinds all the way through canonical-name
prototype materialization. The LSP semantic type projection consumes those
facts rather than guessing from member names or display text. The shared
semantic-analyzer entry points are also exported for MSVC consumers.

On an isolated final snapshot, GCC 11.4, Clang 14.0 and MSVC 17.14 each
completed all 18 LSP test processes with true exit zero and no Unity
`Fail - Cost Time` marker. Each toolchain also passed the main stdio/CLI,
position-encoding and diagnostic-fix smokes. This closes M6.4 and the M6
promotion gate; see `m6-4b-canonical-generic-projection-convergence.md` for
the scoped parser-test residual boundary.

## Promotion Gate

M6 is complete only when a real source-produced artifact is the single source
of scheduler policy/transfer requirements for import, runtime, debug, and
LSP; legacy `Async`/`TaskRunner`/`autoCoroutine` public compatibility surface
and concrete Task-name checks are removed; every terminal transport branch
remains faulted/moved/exactly-once as specified by M4/M5; and the final
GCC/Clang/MSVC plus LSP protocol evidence is recorded with true process exits.

#### Promotion Gate Result

Completed 2026-07-26 08:13 +08:00. M6.1-M6.4 now meet the gate: real
source-produced scheduler artifacts are the only scheduler contract source for
import, runtime, debug and LSP; legacy task compatibility is removed; transport
terminal facts remain structured and exactly-once; and the final three-toolchain
LSP matrix plus protocol evidence is recorded above.
