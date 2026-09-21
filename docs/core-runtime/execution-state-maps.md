# ExecIR logical execution state maps

State maps describe a resumable ExecIR position using logical IDs. They are
owned by an `SZrExecIrFunction` and do not expose machine registers, stack
addresses, native pointers, or allocator-private frame offsets.

Each map entry identifies a source position, instruction, cleanup state, and
resume ID. Its `liveValues` and `rootValues` fields are ranges into side-table
pools of value IDs. Root values are the managed references that a collector or
resumer must preserve; the physical storage for those values is selected later
by the runtime.

An entry's source identity is an exact projection of its instruction: the
instruction's explicit `sourceId` is used when present, otherwise its one-based
instruction ID is the required fallback. Consumers recompute this identity and
reject maps that substitute another nonzero source.

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

A nonzero handler block is valid only at a THROW boundary. It must name an
exception or cleanup block that is an actual CFG successor of the block
containing the checkpoint instruction; an in-range but unreachable block is not
a resumable handler.

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
