---
related_code:
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/include/zr_vm_core/ownership_transfer.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/artifact_encoding.c
  - zr_vm_core/src/zr_vm_core/artifact_rows.c
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_cross_domain.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_lifecycle.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_value_copy.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_telemetry.c
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/type_layout_initialization.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/ownership_transfer.h
  - zr_vm_core/src/zr_vm_core/ownership_transfer.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_cross_domain.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_lifecycle.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_value_copy.c
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/type_layout_initialization.c
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/src/zr_vm_core/artifact_encoding.c
  - zr_vm_core/src/zr_vm_core/artifact_rows.c
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
plan_sources:
  - docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md
tests:
  - tests/core/test_resource_cross_domain_transfer.c
  - tests/core/test_resource_cross_domain_transfer_races.c
  - tests/core/test_type_layout_metadata_contracts.c
  - tests/parser/test_artifact_schema.c
  - tests/acceptance/2026-07-22-syntax-04-m6-cross-domain-transport.md
  - tests/acceptance/2026-07-22-syntax-04-m7-concurrent-major-artifact-aot-lsp.md
doc_type: module
---

# Cross-domain TransferEnvelope contracts

Syntax 04 M6 extends the same-domain owner handoff envelope with artifact-reproducible
cross-domain transport. The runtime recognizes exactly five transfer kinds:
`Forbidden`, `ValueCopy`, `StructuredClone`, `ImmutableHandle`, and `ResourceMove`.
The kind is a canonical TypeLayout/artifact contract, not a decision reconstructed
from a display name, source type name, diagnostic message, or runtime payload shape.

## Artifact and TypeLayout identity

Artifact schema v2 adds the known fixed-width `DOMAIN_TRANSFER_TABLE`. Each 48-byte
row is keyed by an exact TypeDef token and contains transfer kind, schema version/hash,
provider token/hash, and flags. Rows are strictly token ordered and unique. A TypeDef
without a row has no cross-domain contract and is therefore `Forbidden`; an artifact
without this optional table remains valid. Unknown kinds or flags, malformed schema
slices, missing provider identity, and duplicate TypeDef rows reject the artifact.

TypeLayout schema v2 carries the same identity in `SZrTypeLayout`. An explicit
`hasDomainTransferContract` distinguishes `Forbidden` from an absent contract.
GcFree blittable POD without an explicit contract defaults to `ValueCopy`; all other
layouts default to `Forbidden`. Provider-backed layouts are rejected when they expose
GC, ownership, or reference fields, so a provider token cannot authorize a hidden
source-domain edge.

The AOT C layout descriptor emits the same kind, schema identity, and provider
identity. A source layout, binary artifact, VM consumer, and AOT consumer therefore
name one transport contract.

## Envelope lifecycle and identity

Cross-domain envelopes reuse the M5 state machine:

```text
Prepared -> Queued -> Claimed -> Committed
                         \\-> Aborted
Prepared/Queued ----------------> Aborted
```

The envelope captures source and target domain identity, transfer id, transfer
generation, kind, schema/provider identity, quota counters, serialized payload, and
provider token. Publish and claim retain release/acquire ordering. Claim and terminal
operations require the exact target domain, worker id, and claim epoch. Stale domain
generation, duplicate completion, and an invalid state transition fail without
changing ownership.

Only one terminal owner disposes an envelope. `Prepared` and `Queued` cancellation
aborts source-side payload; a claimed target shutdown aborts target-side decoded
payload before destroying that domain; committed payload belongs solely to the
target Place. `ZrCore_OwnershipTransfer_Free` is the single terminal disposer and is
not a concurrent multi-free API. The caller must establish an external quiescent
point before `Free`: no other thread may retain or access the envelope.

## ValueCopy and StructuredClone

Scalar `ValueCopy` snapshots the canonical scalar value. Layout-backed ValueCopy
uses `PrepareCrossDomainValueCopy` to copy closed GcFree inline bytes and
`CommitCrossDomainValueCopy` to validate target layout version/hash before writing
target storage. The source buffer may change after prepare without changing the
snapshot. GC, ownership, reference, and unsupported runtime values are rejected.

`StructuredClone` serializes object/string graphs into an envelope-owned graph. The
decoder preserves alias identity and cycles by allocating an object before decoding
its children and by resolving existing object ids before applying depth limits.
Every object/string must belong to the source domain. External-domain edges and
ordinary GC/ref/Span/PoolRef/native pointer transport are rejected. Object count,
byte count, and depth quotas are enforced before target commit.

## Provider-backed transport

`ImmutableHandle` and `ResourceMove` require exact provider token and contract hash.
Provider callbacks return a structured `EZrDomainTransferStatus`:

- `prepare` produces an opaque fixed-size provider token;
- `commit` constructs the target value;
- `abort` is no-throw cleanup for a zero, partial, prepared, or failed token.

Provider prepare failure always calls `abort`, including partial-token failure.
Provider commit success without a target is rejected. A `ResourceMove` target must
be a direct `Unique`; successful commit transfers the only owner to the target.
Allocation, decode, provider, cancellation, and shutdown failures leave exactly one
cleanup path. With `DROP_ON_FAILURE`, an uncommitted ResourceMove source is released
once and is never restored to the source Place.

Callbacks may enter GC-aware native scopes and poll their domain, but must not
recursively drive the same transfer token or envelope. The runtime does not infer
provider behavior from callback address or native function name. The envelope
snapshots the provider descriptor by value during prepare; provider-owned `userData`
must remain alive until the envelope's single terminal `Free` call returns. This
includes abort cleanup that runs after the terminal state has been published.

M6 exposes the abort path used by domain shutdown but does not maintain a hidden
global envelope registry. The queue/scheduler owner must abort pending or claimed
envelopes before destroying the corresponding domain.

## Diagnostics and current boundary

`SZrDomainTransferDiagnostic` reports a stable status plus observed object, byte, and
depth counts. It distinguishes forbidden contract, domain mismatch, source GC edge,
quota failure, allocation/decode failure, provider prepare/commit failure, stale
generation, and state conflict.

M7 also records transport progress in each domain's `SZrGcStats`. Source-domain counters own
prepare, publish, outbound object and byte totals; target-domain counters own claim, commit,
inbound object and byte totals. Terminal abort is charged to the domain whose transfer side
executes cleanup. The recorder validates domain identity and generation under the domain lock,
so stale envelopes cannot charge a reused domain. These counters are observational only and do
not change the envelope's release/acquire state machine.

M6 provides the runtime transport and artifact contract only. It does not connect
`zr.thread`/TaskScheduler, publish language-level `Send`/`Sync`, or claim delivery or
Job side effects are exactly once. Syntax 12 may consume this substrate only after
the M6 runtime gate; scheduler migration must keep the same transfer id and must not
fall back to the legacy dynamic transport.
