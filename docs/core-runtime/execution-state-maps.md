---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir_runtime.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.h
  - zr_vm_core/include/zr_vm_core/exec_ir_interpreter.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_run.c
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_core/include/zr_vm_core/exec_ir_owner_state.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_map_liveness.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_map_liveness.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_map_liveness.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_contract.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_match.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape_hash.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_allocation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_container_specialize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_inline.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_ownership_elision.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
tests:
  - tests/core/test_ssa_runtime_objects.c
  - tests/core/ssa_runtime_objects_faults.c
  - tests/core/ssa_runtime_objects_concurrency.c
  - tests/acceptance/ssa-runtime-objects.md
  - tests/parser/test_ssa_deopt_aggregates.c
  - tests/acceptance/ssa-deopt-aggregates.md
  - tests/parser/ssa_deopt_aggregate_fault_allocator.c
  - tests/parser/test_ssa_escape_ownership.c
  - tests/parser/ssa_escape_aggregate_cases.h
  - tests/parser/test_ssa_oracle_resume.c
  - tests/acceptance/ssa-oracle-resume.md
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/test_ssa_deopt_validation.c
  - tests/parser/test_ssa_state_map_liveness.c
  - tests/parser/test_ssa_state_map_ownership.c
  - tests/parser/ssa_state_map_fixture.h
  - tests/parser/ssa_owner_fault_allocator.c
  - tests/acceptance/ssa-deopt-validation.md
  - tests/acceptance/ssa-state-map-liveness.md
  - tests/acceptance/ssa-state-map-ownership.md
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
offset. Producer and consumer use the same core CFG ownership analysis to
recompute its state. The current instruction's transition is included only for
post-effect phases; a serialized owner state that differs from that result is
invalid. Unavailable or ambiguous live values are rejected, rather than silently
omitted from the map. This includes values needed by deopt reconstruction.

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
failure. Structural range/ID checks alone do not prove reconstruction value
availability; the shared ownership analysis also checks initialization at each
checkpoint before publication or materialization.

A nonzero handler block is valid only at a THROW boundary. The producer chooses
the first exception or cleanup block in the source block's successor range;
consumers recompute that same choice and require the exact block ID. An
in-range but unreachable or non-canonical successor is not a resumable handler.
When the source block has no exception/cleanup successor, an invalid handler ID
represents an unhandled throw.

## CFG liveness

The producer computes backward live sets over verified CFG successors until
the block inputs stop changing. Each instruction has separate before/after
bitsets. Results are removed when crossing their definition backwards; operands
are added before the instruction. Instruction numbering is storage order, not
execution order: a definition in a later stored block can dominate an earlier
stored use, and a use before a safepoint can keep a value alive through a loop
backedge. Normal and exceptional successors both contribute to live-out sets.

Block phi results are definitions at the destination entry. Their incoming
values are uses only on the matching predecessor edges, so another predecessor's
input is not kept alive at the current checkpoint. Ordinary SSA operands remain
uses even when the consuming result is dead; removing dead computations belongs
to DCE. An instruction's deopt reconstruction values are uses at both checkpoint
phases, with definitions still excluded from the before phase. Unreferenced
deopt records do not make values globally live. The verifier still checks their
storage ranges, but their mere presence does not create a recovery edge.

Managed live values feed the existing root projection; live borrowed values
still reject suspension. The CFG analysis therefore catches borrows needed in
the next loop iteration and avoids rejecting a borrow used only in a sibling
branch. A blockless verified function is analyzed as one straight-line region.
Analysis storage uses checked allocation sizes and is released before map
publication or on failure. An existing map survives a failed rebuild.

The old linear use scan and numeric definition-order filter have been removed.
The private liveness module owns only this analysis; checkpoint identity,
diagnostics, and publication remain in the builder. Ownership projection uses
the shared core analysis described below.

## CFG ownership state

`exec_ir_owner_state` propagates possible owner states forward from the function
entry until block inputs stop changing. Explicit external-entry values begin
available; reserved ordinary values begin uninitialized. Ordinary definitions
initialize their results only from available operands. MOVE and DROP consume
their source IDs, while a later dynamic execution of a definition establishes a
new initialized result. A throwing terminator's result is unavailable on its
exceptional successor.

Phi results are assigned on incoming edges from the corresponding operands.
All phi inputs are read from the same edge snapshot before any phi destination
is assigned. An available input establishes the new logical result; a moved,
dropped, or uninitialized input cannot become available merely by passing
through a phi or a copy. This supports loop-carried ownership without carrying
the previous iteration's moved state into a freshly defined phi result.

At a join, possible states are combined. An initialized/moved or
initialized/dropped mixture cannot be represented as one available live owner,
so the checkpoint is rejected with `STATE_MAP_INVALID` and its source/instruction
identity. `OWNER_UNKNOWN` describes an available value with unknown ownership
classification; it never stands in for unknown initialization. Dead values need
no live-state entry. Unreachable blocks do not emit checkpoints and consumers
reject maps that claim an unreachable instruction is resumable.

The core analysis validates the storage, ranges, and IDs it reads before
traversal, including results, CFG successors and phi inputs. Its transient
before/after arrays are not serialized. Both callers release the analysis on
success and failure; failure-injection tests visit each analysis allocation in
turn and check preservation of the old map/materialized target and zero leaked
analysis allocations. Runtime aggregate preparation has a separate failure
suite described below; physical frame reconstruction remains a later stage.

## Transactional materialization

### Aggregate reconstruction recipes

Each deopt state can select a range of function-owned aggregate recipes. A
recipe names a nonzero object identity, type token and logical layout ID, then
references a field range. Field indices describe the logical layout; they are
independent of the order or offsets of physical storage. A field is explicitly
uninitialized, bound to an SSA value, or bound to another aggregate identity.
Unused IDs must be zero. Several fields may refer to the same identity;
forward references and cycles are represented without recursion or duplicated
object definitions. Identities are unique within the selected deopt graph.

Structural verification checks every recipe range and scalar value ID before
analysis, including unreferenced metadata. Object references must resolve in
the selected graph. Duplicate identities and logical field indices are errors;
unknown field kinds, contradictory IDs and unresolved references are rejected.
These failures carry the deopt source and first referring instruction, or zero
when no instruction refers to the state.

SSA fields are recovery uses at the corresponding checkpoint. They enter the
same CFG live sets, owner-state validation and managed-root projection as
ordinary reconstruction values. A borrowed field therefore cannot cross a
suspend boundary, and an unavailable field cannot be silently omitted. An
unreferenced deopt graph does not keep values live globally. Aggregate identity
references are metadata, not already allocated managed pointers.

The same recipe currently applies to every emitted phase. Fields remain
explicit demands in BEFORE_EFFECT even when the checkpoint instruction defines
that SSA ID. The owner analysis then rejects an unavailable result before any
map is published, rather than emitting a map that its consumer cannot restore.

Materialization independently validates these recipes and requires every
selected SSA field in the entry's live pool. It deep-copies only the selected
graph, rebases field ranges into the prepared field pool, and retains stable
identity IDs and uninitialized fields. This preparation commits with the other
logical state arrays; failure leaves the previous target and its roots intact.
Function cloning owns independent recipe pools. Target storage must not alias
the function's recipe arrays, since replacement frees the old target.

The existing SROA layout map describes physical field placement and remains a
separate projection. These recipes supply the missing logical value bindings;
they do not replace layout validation or authorize a scalarization pass before
it can produce a complete recipe. The runtime object consumer below restores
the graph through a separate runtime-only type/field binding.

Optimization consumers retain recipe VALUE references just like other recovery
uses: DCE preserves their definitions, place promotion and fusion preserve
their observable values, and allocation/ownership plans account for recovery
requirements. Escape analysis includes recovery checkpoints in the value's
last-use boundary, including a checkpoint on the suspension itself. A recovery
use before suspension does not by itself extend lifetime across that boundary.

The shared aggregate hash serializes counts, ranges, identities, type/layout
tokens and each field's index/kind/binding. Pass-manager, fusion, escape,
call-graph and container-plan identities include it, so changed recipes cannot
reuse stale analysis. Invalid metadata has no usable hash. Current inlining
continues to reject functions whose deopt state it cannot remap.

### Runtime object preparation

`ZrCore_ExecIr_MaterializeObjects` consumes the same resume request and privately
prepares its logical snapshot. Its runtime bindings associate each logical
type/layout pair with an existing prototype and expected layout generation;
field indices map to instance-field descriptors. These pointers stay outside
serialized ExecIR. Unknown/stale bindings, duplicate field destinations and
insufficient or overlapping output storage fail before object allocation.

The consumer creates one ordinary runtime object per recipe identity. It
allocates every shell before restoring references and prepares native hash
storage separately from movable objects. Restoring into reserved cells avoids
ordinary value-struct copy semantics, so repeated references and cycles retain
their exact identity. Constructors, field initializers, getters and setters do
not run. Uninitialized fields remain absent; an initialized-null field is
present with a null value.

Temporary `LOCAL_ADDRESS` root frames protect live source values, resolved
prototypes/field names, old output objects, new shells, ambient exception
state and the caller's existing ignored-object registry. Every address used
after allocation is obtained from its current root.
The caller's value/prototype/output pointer payloads can therefore change due
to GC relocation even when preparation fails; their logical identity remains
unchanged. Allocation exceptions are caught inside the operation, and failure
releases detached storage and temporary roots while preserving the old output
and ambient exception. Failed work does not invoke user drops. Existing ignored
registrations are restored after barriers, including indirect closure captures;
the collector's remembered-object registry is reserved before field attachment.
Caught allocation exceptions also preserve the caller's nested GC mutator and
native scopes. Detached or critical native contexts cannot perform this
safepoint operation and are rejected before heap work.
After reserving storage, the transaction acquires a bounded collection pause
before retaining raw addresses across field writes. It holds the pause and
concurrent-marking lock through publication and heap cleanup, so nested write
barriers cannot park for a competing moving collector. Failure to acquire the
pause preserves the old graph. An existing caller-owned pause remains nested
and is not released by this operation.
The current collector reassigns ordinary object regions in place. Runtime tests
force real collections and a competing collector pause, and assert graph
identity and collection ordering; they do not demonstrate physical pointer
relocation. The root protocol still obtains pointer payloads from registered
storage after allocation or a safepoint.

Successful preparation publishes object pointers in selected recipe order and
then removes its temporary roots. Output storage belongs to the caller, which
must put the published objects in its own roots before the next safepoint.
Callers sharing a domain with other active threads must root their inputs
before entering a GC-aware mutator scope, then retain that scope through this
call and output-root publication. Inactive entry is supported only without
other active mutators; roots alone do not protect cached pointers during
preflight against a peer collector.
The operation is synchronous; it has no separate asynchronous cancellation or
pending handle. Repeating it creates another complete graph. Empty graphs
publish count zero. This prepares objects only; the future frame transaction
must publish new frame roots before releasing old frame roots.

Plain GC/scalar fields and class/struct prototypes are supported. Resource
prototypes and ownership-bearing values are rejected until owner transfer can
commit together with the target frame. No implicit cloning, ownership
conversion or drop is used to approximate that transfer.

### Preparation and publication

`ZrCore_ExecIr_MaterializeState` first validates the function token,
generation, entry identity, ranges, and value ownership. It then copies the
logical value and root IDs into temporary storage. Only after every check and
copy succeeds does it replace the caller-owned materialized target. A failed
resume therefore leaves the previous target untouched and cannot replay an
already committed effect.

The caller-owned target must have coherent pointer/capacity pairs. Its value,
root, owner-state and aggregate storage ranges must be mutually disjoint and
must not overlap the function, its directly owned pools, its frame layout and
slots, its GC map and pools, or either the supplied or function-owned state map
and pools. Checks cover capacity-sized byte ranges, including interior pointers,
before preparation because commit releases every old target array.

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

The oracle now pauses and resumes actual reference execution using this logical
contract; see [oracle checkpoint execution](oracle-projections.md#checkpoint-execution-and-resume).
It validates identity, restores mapped live values, preserves effect history
and consumes the saved pause before further execution. This does not yet allocate physical frame slots,
rebuild native registers, or perform a runtime frame switch. Aggregate field
recipes support plain runtime object reconstruction; ownership transfer,
automatic SROA recipe production, inline frames and conditional cleanup flags
remain pending. The current
analysis validates checkpoint value availability; it does not replace full
ownership/borrow/alias verification of every non-checkpoint operation or prove
linear resource balance. Later stages must preserve stable source/resume IDs and
transactional publication while completing those contracts.
