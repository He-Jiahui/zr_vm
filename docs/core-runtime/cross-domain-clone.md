---
related_code:
  - zr_vm_core/include/zr_vm_core/gc_domain_clone.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c
  - zr_vm_core/include/zr_vm_core/ownership_transfer.h
  - zr_vm_core/src/zr_vm_core/ownership_transfer.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_cross_domain.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_commit.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_lifecycle.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/gc_domain_clone.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer.c
plan_sources:
  - docs/plans/ssa/06-gc-domain/04-cross-domain-clone.md
  - docs/core-runtime/cross-domain-transfer-contracts.md
tests:
  - tests/core/test_ssa_cross_domain_clone.c
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
allocating and initializing the complete graph. If any allocation, decode, or
target-state check fails, the destination remains unpublished and the caller
can abort the transaction; the source value is never consumed. `Abort` uses the
source as an explicit cancellation authority, including when the target domain
has already become stale. The source-side branch is linear with commit through
the envelope's `commitInProgress` guard, so a commit and cancellation have one
terminal winner.

The façade closes the runtime envelope immediately after a successful commit or
abort while the source allocator is still valid. It retains a scalar
`SZrOwnershipTransferSnapshot`, allowing callers to inspect the terminal state
without retaining graph memory. `Free` is idempotent for the wrapper and also
attempts to abort an unfinished transaction safely.

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

## Scope and follow-up

This layer intentionally supports the generic object graph forms already
accepted by the canonical transfer encoder. Prototype-bearing objects, inline
array layouts, and remote proxy/reference semantics remain explicit future
contracts rather than silently falling back to shared pointers.
