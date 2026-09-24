---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
tests:
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/test_ssa_deopt_validation.c
  - tests/acceptance/ssa-deopt-validation.md
doc_type: module-detail
---

# ExecIR logical execution state maps

State maps describe a resumable ExecIR position using logical IDs. They are
owned by an `SZrExecIrFunction` and do not expose machine registers, stack
addresses, native pointers, or allocator-private frame offsets.

Each map entry identifies a source position, instruction, cleanup state, and
resume ID. Its `liveValues` and `rootValues` fields are ranges into side-table
pools of value IDs. Root values are the managed references that a collector or
resumer must preserve; the physical storage for those values is selected later
by the runtime. The root pool is an exact projection of the live pool: every
live GC, unique, or shared value appears once as a root, and borrowed or plain
values do not appear there. Consumers reject either omission or invention.

An entry's source identity is an exact projection of its instruction: the
instruction's explicit `sourceId` is used when present, otherwise its one-based
instruction ID is the required fallback. Consumers recompute this identity and
reject maps that substitute another nonzero source.

All phases for one instruction/source checkpoint share one `resumeId`, and a
`resumeId` identifies only one instruction/source checkpoint. Consumers reject
maps that split a checkpoint across resume IDs or reuse one resume ID for a
different instruction before materializing any state.

## Boundary phases

Entries can be recorded at three logical phases:

- `BEFORE_EFFECT`: the effect has not started.
- `AFTER_EFFECT`: the effect completed and its result/effect token is visible.
- `CLEANUP_COMPLETE`: required ownership or exception cleanup has completed.

Consumers treat this list as an explicit whitelist. Values outside these three
phases, including negative enum casts, are rejected by lookup, validation, and
materialization before any caller-owned state is changed.

The boundary flags identify why the checkpoint exists (GC, throw, suspend,
deoptimization, or allocation). A suspend boundary cannot carry borrowed
values across the suspension. This protects the lifetime rule without making a
state map depend on a particular frame layout.

Boundary classification is shared by the parser producer and core consumer.
Materialization recomputes the complete boundary mask from the instruction and
opcode schema and requires an exact match, so a suspend/throw/GC property cannot
be omitted or invented by serialized map metadata.

`exceptionState` is the exact THROW/SUSPEND projection of those boundary flags;
materialization rejects entries whose exception state introduces or omits either
bit instead of publishing contradictory recovery metadata.

Each `ownerStates` item is paired with the value at the same `liveValues`
offset. Consumers recompute its initialized/unknown state and every preceding
MOVE or DROP effect, including the current instruction only for post-effect
phases; a serialized owner state that differs from that result is invalid.
The recomputation also validates every preceding MOVE/DROP operand ID before
using it, so a malformed owner transition cannot be hidden behind a valid map.

When an instruction carries a `deoptId`, its state-map phases reuse the matching
deopt state's nonzero `resumeId` rather than inventing a second identity. The
deopt state must also name the same source ID; materialization rejects a deopt
ID whose source or resume identity differs, while separately validating its
reconstruction value range and requiring every reconstructed value to be present
in the checkpoint's live-value pool. A deopt ID must resolve to exactly one
deopt-state record; duplicate definitions are rejected rather than resolved by
array order.

Before state-map construction, the core function verifier checks every deopt
reconstruction range against the logical `deoptValueCount`, including records
not referenced by an instruction. It checks the start before subtracting and
the count against the remaining pool, so wrapped ranges and accesses into spare
capacity fail without reading their elements. An empty range is valid at the
pool end, but not beyond it. Each referenced reconstruction ID must be nonzero
and within the function's value pool. A range failure reports `INVALID_RANGE`;
an invalid value reports `INVALID_VALUE`, with the function token, deopt source,
and first referring instruction (zero for an unreferenced record). This preflight
runs before liveness scans and leaves an already published state map intact on
failure. It does not yet establish that every reconstruction value dominates
its resume point.

A nonzero handler block is valid only at a THROW boundary. The producer chooses
the first exception or cleanup block in the source block's successor range;
consumers recompute that same choice and require the exact block ID. An
in-range but unreachable or non-canonical successor is not a resumable handler.
When the source block has no exception/cleanup successor, an invalid handler ID
represents an unhandled throw.

## Transactional materialization

`ZrCore_ExecIr_MaterializeState` first validates the function token,
generation, entry identity, ranges, and value ownership. It then copies the
logical value and root IDs into temporary storage. Only after every check and
copy succeeds does it replace the caller-owned materialized target. A failed
resume therefore leaves the previous target untouched and cannot replay an
already committed effect.

The caller-owned target must have coherent pointer/capacity pairs. Its value,
root, and owner-state storage ranges must neither overlap each other nor any
capacity-sized state-map side-table range, including interior pointers. This is
checked before preparation because commit releases every old target array.

ExecIR value construction, structural verification, and state-map
materialization all reject ownership or nullability values outside their named
enum domains, including negative enum casts on compilers with signed enums.

State-map cloning is deep: entries and both ID pools are independently owned.
Lifecycle operations are safe for an empty map and can be used by function
clone/free paths. A distinct clone destination must already have a coherent
initialized storage shape and must not share any side-table allocation with the
source. Sharing is checked as a full capacity-sized byte-range overlap, not only
as equal base pointers, so an interior pool pointer also fails before either map
is changed or freed.

The same storage-shape contract applies inside each map: entries, live values,
roots, and owner states are four independently owned, pairwise-disjoint ranges.
Clone and materialization reject internal base or interior overlap before
copying data or publishing state.

## Current limitations

The initial implementation is deliberately a logical contract rather than a
complete runtime resume engine. It does not yet allocate physical frame slots,
rebuild native registers, encode handler tables, or perform CFG-sensitive liveness
optimization. Later runtime stages may add richer cleanup and deoptimization
metadata while preserving the stable source/resume IDs and transactional
materialization boundary.
