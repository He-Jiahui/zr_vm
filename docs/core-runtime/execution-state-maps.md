# ExecIR logical execution state maps

State maps describe a resumable ExecIR position using logical IDs. They are
owned by an `SZrExecIrFunction` and do not expose machine registers, stack
addresses, native pointers, or allocator-private frame offsets.

Each map entry identifies a source position, instruction, cleanup state, and
resume ID. Its `liveValues` and `rootValues` fields are ranges into side-table
pools of value IDs. Root values are the managed references that a collector or
resumer must preserve; the physical storage for those values is selected later
by the runtime.

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

## Transactional materialization

`ZrCore_ExecIr_MaterializeState` first validates the function token,
generation, entry identity, ranges, and value ownership. It then copies the
logical value and root IDs into temporary storage. Only after every check and
copy succeeds does it replace the caller-owned materialized target. A failed
resume therefore leaves the previous target untouched and cannot replay an
already committed effect.

State-map cloning is deep: entries and both ID pools are independently owned.
Lifecycle operations are safe for an empty map and can be used by function
clone/free paths.

## Current limitations

The initial implementation is deliberately a logical contract rather than a
complete runtime resume engine. It does not yet allocate physical frame slots,
rebuild native registers, encode handler tables, or perform CFG-sensitive liveness
optimization. Later runtime stages may add richer cleanup and deoptimization
metadata while preserving the stable source/resume IDs and transactional
materialization boundary.
