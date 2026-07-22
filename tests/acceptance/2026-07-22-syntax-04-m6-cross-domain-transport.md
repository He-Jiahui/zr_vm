# Syntax 04 M6 cross-domain transport acceptance

## Scope

This acceptance record covers the canonical cross-domain transport contract shared
by artifact schema v2, TypeLayout v2, the VM TransferEnvelope runtime, and the AOT C
TypeLayout descriptor.

## Required behavior

- Artifact rows reproduce `Forbidden`, `ValueCopy`, `StructuredClone`,
  `ImmutableHandle`, and `ResourceMove` with exact schema/provider identity; an
  absent table or row remains valid and exposes no cross-domain capability.
- Cross-domain payloads contain no ordinary source-domain GC/ref/Span/PoolRef/native
  pointer edge.
- ValueCopy supports canonical scalars and closed GcFree inline layouts and rejects a
  stale target layout identity.
- StructuredClone preserves alias and cycle identity, rejects foreign edges, and
  enforces object, byte, and depth quotas.
- Provider prepare/commit/abort exposes structured failures and cleans a zero or
  partial token exactly once.
- ResourceMove success, allocation/decode/provider failure, cancellation, and the
  shutdown abort path leave exactly one owner and exactly one cleanup path. The
  queue/scheduler owner invokes abort before domain destruction.
- Publish/claim and terminal races use release/acquire state transitions and reject
  stale generation, worker, epoch, and duplicate completion.
- Provider descriptors are snapshotted by value; provider `userData` remains valid
  until the envelope's single terminal `Free` returns. `Free` runs only after
  external quiescence.

## Test entry points

- `zr_vm_type_layout_metadata_contracts_test`
- `zr_vm_artifact_schema_test`
- `zr_vm_resource_cross_domain_transfer_test`
- `zr_vm_resource_cross_domain_transfer_race_test`
- `zr_vm_resource_same_domain_handoff_test`
- `zr_vm_gc_domain_bridge_test`
- `zr_vm_gc_domain_multimutator_test`
- expanded parser/compiler/GC/AOT regression matrix recorded in the milestone record

## Evidence state

- Status: complete at 2026-07-22 19:19 +08:00
- GCC 11.4, Clang 14.0, and MSVC 14.44.35207 each passed the same 20 executable
  targets with 506 Unity tests and zero failures.
- ASan/UBSan passed the four focused M6 suites (51 tests total), and TSan passed the
  five direct concurrency/litmus tests without a data-race report.
- Independent review found no remaining Critical or Important finding after the GC
  root, callback lock/lifetime, partial-target cleanup, stale-status, and contract
  boundary fixes were replayed.
- The exact source snapshot, runtime caveats, logs, and per-suite evidence are recorded in
  `docs/plans/syntax/04-resource-ownership-drop-gc-bridge/m6-cross-domain-transport.md`.
