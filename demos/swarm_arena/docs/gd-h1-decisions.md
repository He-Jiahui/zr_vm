# GD-H1 decisions

GD-H1 adds the independent C reference lane and the host DOD==C smoke gate.
It implements the existing **gd-c5-v1/1** rules without changing hash inputs,
goldens, or the DOD/CLI simulation. GD-H2 and later milestones remain deferred.

## Native reference and host ownership

`native/runtime/csim.c` owns deterministic C gameplay: boot-time lookup tables,
the fourteen ordered phases, Park–Miller RNG, stable enemy IDs, counting-sort
grid, FIFO damage/death events, both weapons and per-bullet hit histories,
pickups/upgrades, particles, and both hash passes. Runtime arithmetic uses
`double`; quantization truncates toward zero. The translation unit explicitly
uses `-fno-fast-math -ffp-contract=off` on GCC/Clang. There is no rule or golden
regeneration in GD-H1 and no cross-platform libm equivalence claim.

Dense arrays of native records provide reference storage. Whole-record swaps
copy all gameplay and motion fields, preserving the DOD lane's dense ordering.
The derived stable-ID lookup grows on demand. The native event ring reserves
its capacity at boot, writes entries on enqueue, and never reads unused slots.
Its popped event is copied before appending deaths, allowing reuse on wrap.
Request generation directly appends enemies in request order: no player or RNG
operation occurs between generating and applying a request. The 64-particle
cosmetic burst uses a temporary native request before filling the retained
particle pool. These native storage choices do not enter the gameplay hash.

The host selects C before opening a VM. `--project` remains required and must
be readable, but C does not import or execute its scripts. The C adapter shares
the existing host loop, camera/culling rules, native sprite batch, null backend,
expected-hash check, CSV writer, and atomic JSON publication. Both lanes submit
one completed bulk batch per frame. C summary markers identify `lane=c`;
JSON identifies `lane: "c"` and `exec: "native"`. The additive execution enum
keeps the existing `zr-swarm-perf/1` fields and CSV columns intact.

C has no managed heap. Auto/slack/manual options retain their parsed metadata
but perform no GC work in this lane. Managed-byte, allocation-estimate, GC and
manual-request counters are zero. JSON explicitly says native allocations are
not measured; `alloc.exact` stays false and pause p99 stays null. Timings/RSS
are observations and establish no performance acceptance.

## Acceptance gates

`demo_host_smoke_c` runs smoke with null rendering, bulk submission, seed 12345
and 120 frames. It requires the retained DOD hash **552988638**, checkpoint 60
**42922906**, and every golden final counter, including render content. It
repeats the run and checks bounded GC-policy runs, unsupported lanes,
unreadable project paths, output failure, and expected-hash mismatch status
when reusing a previously successful output directory.

`demo_lane_equivalence` runs both independent simulations through the same
host executable. It schema-validates each artifact, compares every frame's
gameplay hash and sprite count, all final success fields except the lane name,
and deterministic required summary fields including entity maxima. Timing,
GC, allocation and process observations are excluded from equality. In addition
to the canonical smoke run, it checks seed 1 at 61 frames (a non-checkpoint
final positional pass) and maximum seed 2147483646 at one frame. Corrupted frame
hash, final hash, kill count and entity-maximum controls must fail comparison.
A gameplay hash mismatch is a failing test, never a reported-only difference.

The 77 existing GD-C5 gates remain registered unchanged; GD-H1 adds two, for
79 demo CTests. Existing deferred P19/P14 checks and their expected failures
stay intact. Engine sources and `/workspace/zr_vm-perf` are outside this pass;
fresh binaries are built from this demo tree's engine sources. Verification,
binary identities and commands are recorded in [verification](verification.md).
