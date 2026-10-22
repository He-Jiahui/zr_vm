# GD-C5 decisions

GD-C5 extends the existing DOD simulation and shared CLI/host Session. It adds
full v1 gameplay and the four named scenarios on the null backend with bulk
submission. Other lanes, renderers, interactive input, performance targets and
H/M milestones remain deferred. Runtime reflection and the v2 package alias
remain deferred; the project keeps `$swarmcore` / `import("&swarmcore")`.

## Golden change declared before regeneration

This decision was written before regeneration. The restored GD-C4 smoke
reference is **224663030**, with checkpoint 60 **227689432**. The GD-C5 rules
revision is **gd-c5-v1/1**. Smoke uses
the full gameplay systems, including separation, the three enemy kinds, both
weapons, pierce, drops and progression. Separation changes enemy acceleration
and positions. Kind selection, pickup drops and upgrades consume simulation
RNG; player progression, director state, bullet pierce and pickups also need
hash coverage. These changes make the GD-C3 hash inputs inequivalent, so a new
smoke golden is deliberate and necessary. Goldens were regenerated after the
rules and their ordering were written and functional checks passed.

## Source inventory and implementation boundaries

The retained modules are `config`, `director`, `events`, `player`,
`lanes/dod/{world,grid,systems}`, `game_session`, `game`, `main`, `render`,
`render_buffer` and `metrics`. The existing numeric columns, counting-sort
grid, dense swap removal and stable-ID event queue were extended. Weapon
logic belongs in `player`; the shared Session keeps the canonical 14 phases.
The host retains `main.c`, `options.c`, `host_vm.c`, `host_loop.c` and the null
platform. Scenario selection reaches `game.boot` and host artifacts; frame
storage and statistics support 1800 frames without the old 120-slot limit.

Full scenarios are completion/parity gates in Release. The same CTests in
memory-error-detector and forced-GC builds use bounded smoke-sized
scenario runs, while the full smoke and gameplay unit gates retain their
coverage. This follows design §8.4's stress restriction and keeps the long
scenario matrix separate from performance acceptance. Exact run lengths and
results are recorded in verification and the acceptance note.
The Release completion gate may run its independent CLI and host processes
together; their project is read-only and host artifacts have a separate path.
Stress checks run the two drives sequentially, with CTest `-j1` throughout.
The added scenario gates use 1800 frames in Release and four frames in each
stress tree. Full 120-frame smoke and all twenty gameplay units still run in
every tree, including the 64-particle allocation path.

## Gameplay choices

Grunt, Runner and Tank use (speed, HP, radius) values (72, 2, 10),
(120, 1, 7) and (42, 8, 16). Bosses are stronger Tanks in the director's
burst, rather than a fourth enemy kind. The director reserves room for its
first boss burst, so reaching the enemy cap cannot suppress that transition.
Smoke compresses the wave clock so its 120 frames exercise a boss transition.

The nearest weapon retains the scenario's aimed throughput: 2, 10, 10, 100
and 10 bullets per frame for smoke, arena, swarm, bullet storm and particle
burst. A separate eight-shot radial burst fires every 60 frames. Aimed bullets
start with two permitted hits; radial bullets start with three. Per-bullet
stable-ID history prevents repeat damage to the same enemy, including across
frames and dense swaps. Both lifetime expiry and exhausted pierce use dense
swap removal.

XP starts at level 1 with four points needed; each later threshold increases
by two. A level-up consumes exactly one Park–Miller draw and selects one of
five upgrades: damage, pierce, aimed count, magnet radius or radial count.
Caps bound each upgrade; selecting a capped upgrade still consumes that draw.
Drops and upgrades share simulation RNG with the director. Cosmetic particles
consume no RNG, and their allocation/storage never enters the state hash.

The particle-burst workload uses a short-lived numeric SoA request for each
64-particle death burst, then copies the request into the retained dense
particle storage. This deliberately exercises allocation at burst creation
without introducing per-particle game objects or affecting simulation RNG.
Other scenarios create four particles per kill using the retained pool.

The full rules document specifies exact phase arithmetic, query order,
wave definitions, capacities and hash order, written before golden generation.

The first retained Release run passes 72/73. Its new host negative case puts
`--scenario` first but correctly receives an error naming the invalid
`--frames`. Ordering the invalid field first restores the runner's existing
field assertion. This is a test correction; no rule, golden or host option
behavior changes.

## Retained composition constraints

Helpers are declared before callers that need their imported metadata. Player
methods bind native math through local literal imports, following the GD-C3
constructor pattern. Progression is a normal function in `player.zr`, keeping
the package hash function in its supported module scope. New public module
functions accept and return scalar RNG state: package-qualified RNG parameters
did not load in this graph, and dynamic RNG calls returned a wrong value.
The existing director method keeps its working typed RNG argument. Both paths
use the same Park–Miller step in the v1 swarmcore package; hash arithmetic
remains there too. These are demo composition choices and require no engine
source change.

New bullet and pickup containers live in `lanes/dod/bullet_store.zr` and
`pickup_store.zr`. The retained enemy and moving columns stay in `world.zr`.
Putting the new containers beside both retained classes caused member lookup
failures in an existing queued-damage test; separate modules pass all twenty
units. The layouts and arithmetic are unchanged by that split.

Enqueue constructs the tagged-union value in a local before loading the ring
storage for indexed assignment. Combining construction and assignment skipped
two damage events in binary execution. The split preserves the intended FIFO
rules and memory lifetime; three source-removed binary smoke repetitions now
match the interpreter and host. Full kind/boss death payloads remain explicit.

The first full-v1 memory-error-detector smoke hits the inherited 600-second
process deadline with a ring prefilled with thousands of unused placeholders.
The ring now appends each slot on its first write, then reuses storage on wrap.
The tail visits every index in order before its first wrap, so no unwritten
slot can be popped. Logical capacity, FIFO order and scalar decoding remain
unchanged. This storage choice changes no rule or hash input and requires no
golden regeneration. A private completion check matches **552988638** in
**115.24 s**; three Release interpreter repetitions, three source-removed
binary repetitions and all twenty units also pass. All 77 distinct tests pass
in each final configuration after applying the change. Engine source remains
unchanged.

Final rules wording spells out the existing distance cutoffs for dodge, seek,
separation and aim. This clarifies the implementation; it changes no runtime
arithmetic, hash input or golden.

## Resume and acceptance decisions

Release and memory-error-detector each pass one complete 77-test run.
Forced-GC has 77 distinct successful outcomes: 12 retained passes before
the previous session ended, followed by a successful sequential 65-test
resume starting at `demo_cli_host_particle_burst_dod`. Interrupted
particle-burst attempts had no completed outcome; this test was rerun. No
runtime source, golden, compiler flag or test deadline changed during resume.
The original partial logs and the resumed logs are preserved separately.

Forced-GC validation time is 7429.25 s: 6826.05 s for the first 12
completed tests plus 603.20 s for the 65-test resume. It excludes
interrupted work and session downtime. Release and detector values are
complete-suite wall times. Leak detection remains disabled.

The resumed acceptance audit uses saved CTest stdout and fresh per-test
artifacts after a concurrent inventory query replaced the temporary detail
log. The original 12-test detail log is intact. Later inventory queries run
after test completion; [verification](verification.md) records the exact
evidence and this limitation.

The acceptance note keeps its original `2026-10-21` filename and records
completion on 2026-10-22, preserving its existing links. The resume
updates documentation and readiness status only. Status needed refresh because
its top-level summary still deferred GD-C4; historical probe records remain
intact. Release and detector results are retained because their binaries
match their recorded hashes. Current build hashes are in `gd_c5`; the
earlier validation and milestone objects keep their historical evidence.
P19 reflection, P14 v2 package integration, other lanes, software/GL
rendering and all H/M milestones remain deferred.
