---
related_code:
  - zr_vm_core/include/zr_vm_core/gc_domain_clone.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c
  - zr_vm_core/include/zr_vm_core/ownership_transfer.h
  - zr_vm_core/src/zr_vm_core/ownership_transfer.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_cross_domain.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_graph_internal.h
  - zr_vm_core/src/zr_vm_core/ownership_transfer_graph_decode.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_commit.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_lifecycle.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/gc_domain_clone.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_graph_decode.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_commit.c
plan_sources:
  - docs/plans/ssa/06-gc-domain/04-cross-domain-clone.md
  - docs/core-runtime/cross-domain-transfer-contracts.md
tests:
  - tests/core/test_ssa_cross_domain_clone.c
  - tests/core/test_ssa_cross_domain_clone_oom.c
  - tests/core/test_resource_cross_domain_transfer.c
  - tests/core/test_resource_cross_domain_transfer_races.c
doc_type: module-detail
---

# Cross-domain structured clone

## Purpose

`GcDomainClone` is the domain-facing façade for the existing structured clone
transfer protocol. It makes the isolation boundary explicit: the source value
is borrowed, the source graph is encoded during preflight, and every managed
object allocated during commit belongs to the target domain. The façade does
not turn a source pointer or ownership control block into a shared reference.

## Protocol

`ZrCore_GcDomainClone_Prepare` validates that both states have live, distinct
domain identities and that object/byte/depth quotas are non-zero. It then
creates a `StructuredClone` transfer contract with `DropOnFailure` and delegates
the graph walk to `ZrCore_OwnershipTransfer_PrepareCrossDomain`. The graph walk
records source identity indices before descending fields, so repeated edges and
cycles are represented by one destination object per source object. Borrowed,
unique, weak, foreign-domain, prototype-bearing, and unsupported values are
rejected before a transfer can be published.

The remaining calls correspond one-to-one with the transfer state machine:

```text
Prepare -> Publish -> Claim(worker, epoch) -> Commit
                                      \-> Abort
```

`Commit` requires a null destination and uses target-domain root handles while
allocating and initializing the complete graph. The private graph decoder runs
inside `Exception_TryRun`; its caller owns the object roots, the current field's
key/value roots and a staged destination. Those values survive the protected
callback's non-local exit. Both ordinary failure and Throw release every
registered temporary root. The destination is written only after the complete
graph has been initialized.

After decode, the envelope releases `commitInProgress` before propagating the
original thrown status. A failed transaction remains `CLAIMED`, retains its
encoded graph and keeps the destination null. Once the caller has caught and
handled the target exception, the same worker/epoch can retry it, or the caller
can Abort/Free it. `MEMORY_ERROR` maps to the transfer diagnostic
`ALLOCATION_FAILED`; the exception is still observable by the outer TryRun.
The source value is never consumed. `Abort`
uses the source as an explicit cancellation authority, including when the
target domain has already become stale. The source-side branch is linear with
commit through the envelope's `commitInProgress` guard, so a commit and
cancellation have one terminal winner.

The façade closes the runtime envelope immediately after a successful commit or
abort while the source allocator is still valid. It retains a scalar
`SZrOwnershipTransferSnapshot`, allowing callers to inspect the terminal state
without retaining graph memory. `Free(NULL)` tolerates repeated calls; a
non-null wrapper is single-use once released. For an unfinished transaction,
`Free` first attempts to abort it. If a concurrent commit prevents the abort,
`Free` retains the wrapper and returns; the caller must coordinate a later retry.

## Ownership and lifetime invariants

- Target objects receive the target domain identity during normal allocation;
  no source managed address is copied into a target field.
- The graph's identity map preserves cycles and aliases within the destination,
  while source and destination object addresses remain distinct.
- Strings are copied as bytes. External resources are not accepted by the
  structured-clone path; resource materializers use the separate provider
  prepare/commit/abort contract in `ownership_transfer.h`.
- Quota, malformed domain identity, stale generation, decode, and state
  conflicts are reported through `SZrDomainTransferDiagnostic` with object,
  byte, and depth counters. No failure is converted into a boolean-only success
  or an implicit shared handle.
- A transaction must reach Commit/Abort (or be explicitly freed) before the
  source `GlobalState` is destroyed. Successful façade terminal calls perform
  that envelope disposal automatically, so a committed target graph can outlive
  the source domain.

## Test coverage

`test_ssa_cross_domain_clone.c` covers cycle/alias preservation, target-domain
root resolution after source teardown, quota rejection before target
allocation, source-side cancellation after target generation staleness, and
same-domain admission rejection. The existing resource transfer suites cover
provider materializer failures, exactly-once abort/drop, duplicate claims,
commit/abort races, and target shutdown. The focused clone test is intended to
be registered as `ssa_cross_domain_clone` with executable target
`zr_vm_ssa_cross_domain_clone_test`.

`test_ssa_cross_domain_clone_oom.c` injects a real field-pair allocation failure
and its allocator retry during target concurrent-major GC. Its two cases check
exact `MEMORY_ERROR` propagation, a null destination, root and mutation-depth
restoration, an unchanged and writable source, and either Abort/Free followed by
a new clone or retry of the same claim. The successful result must retain the
cycle and aliased child using independent target addresses. This private-Core
test is registered only for static builds as `ssa_cross_domain_clone_oom`.

## Scope and follow-up

This layer intentionally supports the generic object graph forms already
accepted by the canonical transfer encoder. Prototype-bearing objects, inline
array layouts, and remote proxy/reference semantics remain explicit future
contracts rather than silently falling back to shared pointers.

The public `Execute` convenience call owns a second protected Commit context.
When Commit throws, Execute aborts and frees its hidden transaction before
rethrowing the original status. Its caller still receives the target exception
and failed-stage diagnostic. A third OOM case observes the source allocator's
envelope allocation/release balance before state teardown, then executes a
successful object clone through the same convenience entry.

Source-side Prepare
allocation failures, raw-array materialization and provider callback Throw are
also separate contracts; the field-pair OOM regression does not validate them.
