---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_scheduler_contract.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_scheduler_contract.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/include/zr_vm_core/canonical_consumer.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scheduler_artifact.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_scheduler_contract.c
plan_sources:
  - docs/plans/syntax/12-async-task-job-scheduler/m6-artifact-debug-lsp-migration-implementation-plan.md
tests:
  - tests/language_server/test_lsp_interface.c
doc_type: module-detail
---

# LSP Scheduler Artifact Projection

## Scope

`ZrLanguageServer_LspSchedulerContract_ResolveArtifact` is the LSP-side
projection boundary for a scheduler call that was compiled into a real `.zro`
artifact. It accepts the compiler-owned
`SZrFunctionSchedulerSourceFact`, artifact bytes, and no source text.

The result exposes the canonical scheduler receiver `TypeId`, serialized
TypeRef token, Task/Job tokens, schedule signature token, ABI, policy,
per-policy requirements, owner layout/module identity, transport hash, and
scheduler contract hash. It is a structured semantic result; display text is
not an input to resolution.

## Resolution Contract

The projection performs these checks in order:

1. The source fact must describe an exact
   `TASK_SCHEDULER_SCHEDULE` receiver call and carry nonzero receiver type,
   schedule signature, ABI, transport, scheduler contract, and provider
   layout/module identities.
2. Artifact bytes are decoded as a `.zro` document. The LSP constructs the
   expected public identity from the source fact's canonical scheduler type,
   schedule signature, provider layout, scheduler contract, and provider module
   hash, then opens the artifact through `ZrCore_CanonicalConsumer`.
3. The source `TypeId` is resolved first. The serialized scheduler row is then
   selected by the artifact header's TypeRef token, not by the native provider
   TypeDef token. Both projections must identify the same canonical type and
   schedule signature.
4. `ZrCore_CanonicalConsumer_ValidateSchedulerContract` validates ABI, active
   policy requirements, transport hash, and scheduler contract hash. The
   projection also compares the complete policy mask and both AttachedDomain and
   IsolatedDomain requirement fields before returning a result.

This makes owner-module, policy, requirement, transport, and callable contract
mismatches explicit artifact diagnostics. For example, a source provider module
mismatch returns `ZR_ARTIFACT_STATUS_MODULE_HASH_MISMATCH`; policy and transport
mismatches return `ZR_ARTIFACT_STATUS_SCHEDULER_POLICY_MISMATCH` and
`ZR_ARTIFACT_STATUS_TRANSPORT_CONTRACT_MISMATCH` respectively.

## Non-Fallback Boundary

The projection never selects a scheduler by member name, source spelling,
filename, raw AST, runtime value category, provider display text, or diagnostic
message. The native provider TypeDef token remains compiler provenance and is
not treated as the artifact TypeRef token. Unavailable or mismatched facts fail
closed with the canonical artifact diagnostic instead of constructing a partial
hover, signature, definition, rename, or workspace edit result.

Workspace edits retain their existing document snapshot capture/revalidation
contract. This artifact projection does not mint document versions, source
ranges, or edits, so it cannot legitimize ranges from a later document
generation.

## Validation

The LSP interface regression compiles a real
`zr.thread.ThreadScheduler.schedule` source call, writes its `.zro` artifact,
and resolves the pair through this projection. It verifies canonical source and
binary parity, then independently changes the owner module hash, policy, and
transport hash and requires the corresponding structured rejection. Toolchain
acceptance on 2026-07-26 used one byte-exact `9096792 + M6.4` snapshot. GCC
11.4, Clang 14.0, and MSVC 17.14 each passed this regression and all three
stdio smoke scripts (main protocol plus position-encoding and diagnostic-fix)
with real process exit zero. The wider 18-target LSP matrix was 16/18 on every
toolchain; the two nonzero reachability query/hover targets and their existing
Unity markers are recorded as baseline failures, not as M6.4 passing evidence,
in the Syntax 12 completion record.
