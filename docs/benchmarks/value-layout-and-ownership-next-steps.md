# Value layout and ownership follow-up

The r2 change reduces `SZrTypeValue`/`SZrTypeValueOnStack` from 48/64 to
40/48 bytes on the 64-bit ABI by using their natural 8-byte alignment. It does
not change ownership semantics. All native consumers must rebuild against the
new headers. Bytecode patch 46 is the minimum accepted version: older raw frame
byte offsets are rejected with a recompile diagnostic. Frame sizes and layout
offsets now encode a physical stack-slot index plus an intra-slot byte offset;
the reader validates the remainder and multiplication before forming bytes.
Inline payload sizes/alignments and type layout identities remain validated
ABI metadata; this is not a general cross-architecture artifact format.

The AOT audit covers `aot_ir.c`, frame descriptors/setup, frame manifests,
value access and call/return lowering in `backend_aot_c_*`. Runtime-derived
frame offsets and emitted `sizeof(SZrTypeValue)` / `sizeof(SZrTypeValueOnStack)`
already determine strides. Numeric 48/64 constants in arithmetic lowering are
integer widths, not value or stack strides. Generated C, shared-library,
artifact round-trip and frame-layout tests remain required after ABI changes.

## Ownership side table

A future compact scalar value should separate uncommon ownership metadata
from the ordinary tagged payload. First measure live ownership kinds, metadata
bytes and lookup frequency in calls, containers and GC-heavy programs. Prototype
one per-domain ownership record identified by a generation-checked handle;
keep the current inline representation behind a build option for A/B tests.

The handle must preserve control/weak identity, unique move invalidation,
borrow/loan provenance, shared counts and destructor order. Stack growth,
minor forwarding, aliases, return escape and native/AOT argument staging must
all update or resolve the same identity. Allocation failure must leave copies
and moves in the original state, without consuming an ownership obligation.
Do not use a raw stack address as a durable key. Define record lifetimes before
reusing handles; a stale weak or loan handle must never resolve to a new owner.

Acceptance requires the existing ownership and forced-GC matrix, allocation
failure injection, nested destruction/reentry and attached-thread TSan gates.
Measure lookup cost as well as reduced value size; a side table that makes
ordinary calls or scalar copies slower does not meet the performance goal.
This work needs a separate implementation and artifact/native ABI version.

## Object and function header diet

Inventory each field by object kind, lifetime and readers before moving it.
Candidates are cold profiling counters, rarely used metadata, optional
ownership bookkeeping, debug information and function compiler/ExecIR/AOT
attachments. Keep tracing/linkage, kind, color/age and forwarding requirements
explicit in a minimal common header; put optional data in separately allocated
kind-specific records. Favor existing cached summaries over duplicating facts.

Establish per-kind allocated-byte and cache-miss baselines first. Split one
family at a time, with typed accessors and allocation-failure rollback. Review
all raw casts, native/plugin ABI users, checkpoint snapshots, compiled
prototype blobs, object pools and AOT root maps before each field move. Gates
include full ASan/UBSan, concurrent GC/TSan, mutation fuzzing and the deep/GC/
call benchmark suite. Header size targets alone cannot justify weakening roots,
barriers, ownership cleanup or stable metadata identity.
