# Syntax 12 M6.1b.2b artifact writer acceptance

## Scope

Compile a real `zr.thread.ThreadScheduler.schedule` source program, write a
real `.zro` canonical artifact, and import it through
`ZrCore_CanonicalConsumer`. No hand-built artifact row and no legacy `.zrb`
writer is accepted as producer evidence.

## Required assertions

- Scheduler, Task, and Job source facts include resolved provider TypeDef,
  signature, layout, and module identities.
- The compiler publishes complete source-provider identities; the writer
  projects artifact-local TypeRef/TypeDef rows only from those identities and
  rejects unavailable providers.
- Imported Scheduler contract hash equals the source schedule contract hash.
- Imported Job domain transfer is `RESOURCE_MOVE` with `DROP_ON_FAILURE` and
  preserves its provider contract hash.
- Schema/provider/ABI/policy/requirements/transport mismatches are rejected by
  the existing canonical consumer contract tests.

## Evidence

- RED: before the writer existed, the real source roundtrip test failed to
  link with an undefined `ZrParser_Writer_WriteSchedulerArtifactFile`.
- GREEN: on an isolated `e04719a + M6.1b.2b overlay`, GCC, Clang, and MSVC
  each ran `zr_vm_artifact_schema_test` 21/21 and
  `zr_vm_canonical_consumers_test` 17/17 with real process exit 0. The final
  evidence is recorded in the Syntax 12 M6.1b.2b completion record.
