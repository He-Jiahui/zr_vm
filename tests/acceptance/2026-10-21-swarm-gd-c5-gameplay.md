# GD-C5 full v1 DOD gameplay acceptance

Requested scope is GD-C5 only: extend the retained DOD lane and shared CLI/host
Session for full v1 gameplay and the four named scenarios, using null rendering
and bulk submission. Engine source remains at `e1d3ab19`. Other lanes,
renderers, interactive input, performance harnesses, H/M milestones, runtime
reflection and the v2 package alias remain deferred.

## GD-C4 prerequisite baseline

Before applying gameplay changes, demo `e988a3ee` passes **73/73** in Release
(60.38 s), **73/73** in memory-error-detector (2202.79 s), and **73/73**
in forced-GC (1804.60 s). Each retains native **8/8**, required probes
**38/38**, P22 **18/18**, ten ZR units, smoke CLI/host **224663030**, and
checkpoint 60 **227689432**. Every complete suite passes without a retry.
The unchanged engine Release core passes **515/515** in **68.13 s**. All demo
core inventories remain 515. The retained policy is leak detection disabled.

Commands, timings and private evidence are under
`/tmp/zr-swarm-gd-c5-20261021/`; `results.jsonl` records individual runs.
The [verification record](../../demos/swarm_arena/docs/verification.md) contains
the gate table. **GD-C5 is complete**, with final acceptance on 2026-10-22.

## Implementation and determinism

The retained config, director, events, player, DOD stores/systems, Session,
render buffer and CLI/host entry points implement full v1. New bullet and
pickup stores preserve numeric SoA columns, stable enemy IDs and dense swaps.
Both weapons run before bullet movement and collision. Pickups and upgrades
run after particles and before hashing. Six director definitions include boss
bursts at frames 600 and 1500 in long scenarios; smoke compresses the clock.
Particle burst creates a temporary 64-particle numeric request per death.

[Rules](../../demos/swarm_arena/docs/rules.md) is now `gd-c5-v1/1`.
[Decisions](../../demos/swarm_arena/docs/gd-c5-decisions.md) records exact kind
values, upgrade selection/caps, pierce history, reserved boss capacity, module
composition, event construction and first-write ring storage. The smoke hash
change was declared before regeneration: full gameplay changes trajectories
and RNG draws, and adds progression, director, pickups and pierce to hashing.
Cosmetic particles, their allocation and render content stay outside that hash.

| Scenario | Frames | CLI hash | Host hash | Checkpoint 60 |
| --- | --- | --- | --- | --- |
| smoke | 120 | 552988638 | 552988638 | 42922906 |
| arena_2k | 1800 | 60149719 | 60149719 | 131330391 |
| swarm_10k | 1800 | 603625956 | 603625956 | 422884711 |
| bullet_storm | 1800 | 324791145 | 324791145 | 433197618 |
| particle_burst | 1800 | 203028298 | 203028298 | 96337168 |

All entries use seed 12345, DOD, null and bulk. Release scenario gates run
1800 frames per drive; the same four gates run four-frame profiles in stress
trees, alongside full 120-frame smoke and all twenty units, following design
§8.4. Host CSV/JSON schema, aggregates, checkpoints and final counters are
validated in addition to the simulation hash.

## Final gates

| Gate | Release | memory-error-detector | forced-GC |
| --- | --- | --- | --- |
| All demo CTests (73 retained + 4 scenario) | 77/77 | 77/77 | 77/77 |
| New scenario CTests | 4/4 | 4/4 | 4/4 |
| Scenario frames per drive | 1800 | 4 | 4 |
| Smoke frames | 120 | 120 | 120 |
| Smoke repetitions, interpreter + source-removed binary | 3 + 3 | 3 + 3 | 3 + 3 |
| ZR gameplay units | 20/20 | 20/20 | 20/20 |
| Native C tests | 8/8 | 8/8 | 8/8 |
| Required GD-C1 checks | 38/38 | 38/38 | 38/38 |
| Successful GD-C1 mode checks | 58/62 | 58/62 | 58/62 |
| P22 required pairs | 18/18 | 18/18 | 18/18 |
| CLI bad-input cases | 28/28 | 28/28 | 28/28 |
| Host failure cases | 28/28 | 28/28 | 28/28 |
| Core inventory, names/commands/properties | 515 unchanged | 515 unchanged | 515 unchanged |
| Detector findings | — | 0 | 0 |
| Validation seconds (scope below) | 1942.37 | 7469.56 | 7429.25 |

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

The resumed audit uses the saved 65-test stdout and fresh per-test artifacts;
an overlapping inventory query replaced CTest's temporary detail log. The
original 12-test detail log is intact. The verification record documents the
artifact checks and log limitation.

All configurations preserve native 8/8, required probes 38/38 and P22
18/18. The four deferred optional mode checks retain their expected
diagnostics and FAIL feature status. Core inventories match all 515
baseline names, commands and properties. The unchanged engine Release
515/515 result remains valid; no engine build or source change was needed
during resume.

Four-frame stress CLI/host references are `arena_2k=91709327`,
`swarm_10k=286636939`, `bullet_storm=822063058`, and
`particle_burst=202404383`. They supplement the full 120-frame smoke and
twenty gameplay units. Full scenario values are in the table above.

The earlier Release 72/73 attempt identified an incorrect negative-case
field assertion. The first detector attempt reached the 600-second smoke
deadline. Growing event-ring storage on first write resolves that timeout
with the same golden and deadline. These earlier attempts remain separate
from final acceptance.

Implementation and gate commits:

- `968abf05` — demo(swarm): re-establish GD-C4 baseline for full gameplay
- `24324794` — demo(swarm): extend DOD gameplay to full v1 rules
- `635e3850` — demo(swarm): drive full scenario profiles through the host
- `693c6ef5` — demo(swarm): document full gameplay scenarios and controls
- `258dcffa` — demo(swarm): grow event-ring storage on first write
- `64b5e369` — demo(swarm): gate all scenario profiles with CLI and host goldens

The pre-push fetch confirms `origin/main` at `291be0cba00f`, an ancestor
of unchanged perf `e1d3ab19`; the demo update is a fast-forward
from its remote branch. Only the demo branch requires a push. Raw logs and
the separate resume audit remain under `/tmp/zr-swarm-gd-c5-20261021/`.

P19 runtime reflection, P14 v2 package integration, other lanes, `sweep`,
software/GL rendering, interactive input, performance harnesses and all
H/M milestones remain deferred. The v1 `$swarmcore` dependency and
`import("&swarmcore")` remain in use.
