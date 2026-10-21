# Swarm Arena GD-C3 — DOD CLI smoke acceptance

Date: 2026-10-21. Scope: exactly GD-C3. Engine `ed10137f` is unchanged.
Implementation commits are `437f2dbc` (rebuilt GD-C2 baseline), `a3fce261`
(deterministic storage/unit tests), and `745052a3` (CLI smoke and golden gates).

## Result

The stock CLI runs the DOD smoke with the 14 canonical phases, a deterministic
bot, minimal director, preallocated SoA enemy/bullet/particle storage, stable
counting-sort grid, queued damage/deaths, mode C bulk submission through
`zrgame.gfx` and the null backend. RNG/hash code lives only in
`game/packages/swarmcore`. CLI timing and GC telemetry are excluded from hashes.

The default seed-12345, 120-frame golden is **224663030**. All three repetitions
in both interpreter and source-removed binary mode match in each configuration
(18 gated smoke runs). Frame-60 checkpoint is **227689432**; final null content
hash is **183257039**. The golden also enforces final counters: 187 enemies,
46 bullets, 88 particles, 240 shots, 120 applied hits, 60 kills, score 600,
player HP 100, and 295 visible sprites. Golden data is committed in
`demos/swarm_arena/goldens/hashes.json`.

| Gate | Release | ASan/UBSan | Forced full GC + ASan/UBSan |
| --- | --- | --- | --- |
| GD-C2 baseline before game code | 67/67 | 67/67 | 67/67 |
| Final demo CTests | 71/71 | 71/71 | 71/71 |
| Native C tests | 8/8 | 8/8 | 8/8 |
| ZR unit tests | 10/10 | 10/10 | 10/10 |
| Bad-input checks, both modes | 12/12 | 12/12 | 12/12 |
| GD-C1 required checks | 38/38 | 38/38 | 38/38 |
| All successful GD-C1 mode checks | 58/62 | 58/62 | 58/62 |
| P22 required pairs | 18/18 | 18/18 | 18/18 |
| Core CTest inventory | 515 unchanged | 515 unchanged | 515 unchanged |
| Final demo wall seconds | 35.38 | 717.65 | 703.46 |
| Detector findings | — | 0 | 0 |

The unchanged engine Release core suite passes **515/515** in 63.63 s on the
rebuilt toolchain. No engine source changed, so full engine detector and
forced-GC core selections were not rerun. Their trees were recreated and their
CLIs built. Core inventory names, commands and properties in every demo tree
are identical to the pre-GD-C3 baseline.

The four new CTests are `demo_cli_smoke_interp_dod`,
`demo_cli_smoke_binary_dod`, `demo_zr_unit_tests`, and `demo_bad_scenario`.
All have label `demo` (and `game`). Binary checks compile fresh artifacts,
remove every `.zr` file including packages/tests, then start fresh CLI processes
with binary execution markers. Native C tests and retained GD-C2 plugin smokes
remain green. The bad-input test requires exit **2**, a field-naming diagnostic,
no success marker and exactly one `SWARM_FINALLY cleanup=ok`; it includes an
expected-hash failure after running the loop. An additional 24 Release checks
cover help, partial runs, both extreme seeds, GC budget bounds, duplicate and
unsupported options in both modes, with matching partial-run hashes.

## Decisions and limits

The implementation uses a v1 `$swarmcore` dependency and
`import("&swarmcore")`. P14's v2 `@swarmcore` package integration and P19 runtime
reflection remain deferred, with their existing expected-error gates intact.
The game does not depend on either feature. Slash workspace paths, canonical
native array field types, scalar event-module interfaces and dynamic module
values for plugin calls avoid composition problems without an engine patch.
Stock `zr_vm_cli test` ignores `testSource`; the runner stages the three
canonical test files and only their needed modules in a private project, then
executes the stock test command. Exact decisions and limitations are in
`demos/swarm_arena/docs/gd-c3-decisions.md`.

Only one enemy kind, seek steering, nearest-target fire, queued damage/deaths
and cosmetic particles are in this rules subset. Pickups occupy an explicit
no-op canonical phase. Separation, other kinds/weapons, pierce, pickups/XP,
upgrades and larger wave/scenario behavior remain GD-C5. Host drive and file
metrics remain GD-C4. No later milestone, other lane, renderer, image or
performance-budget acceptance is implemented or claimed.

The instance migration required fresh builds using system CMake 3.31.6,
Ninja 1.12.1, Clang/LLVM 19.1.7, Python 3.13.5 and system libffi 3.4.8-2.
Baseline test counts and feature behavior are unchanged. No local tool
directory is used. Concurrent sanitizer
processes caused memory pressure and wrapper timeouts; those attempts are
preserved, and final gates run sequentially with CTest `-j1`. Debug/forced-GC
smoke cost remains above the design's aspirational five-second detector target.
This is a documented performance limitation. These results retain disabled
leak detection and do not claim leak-clean shutdown.

## Reproduction and evidence

Current configure/build/run commands are in `demos/swarm_arena/README.md` and
`docs/verification.md`. Rebuilt flags are unchanged apart from compiler and
system dependency paths. From the demo worktree:

```sh
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ZR_VM_TEST_TIER=core
export TMPDIR=/tmp/zr-swarm-gd-c3-runtime
for configuration in release asan gcstress; do
  ctest --test-dir "build/demo-${configuration}" -L demo -j1 --output-on-failure \
    > "/tmp/zr-swarm-gd-c3-${configuration}-demo-final.log" 2>&1
  ctest --test-dir "build/demo-${configuration}" --show-only=json-v1 \
    -LE demo -E '^performance_report$' \
    > "/tmp/zr-swarm-gd-c3-demo-${configuration}-core-inventory-final.json"
done
```

Raw logs, runtime projects, source-removal evidence and result JSON live only
under `/tmp/zr-swarm-gd-c3-*`. CTest Temporary/probe-results use `/tmp` symlinks.
`/tmp/zr-swarm-gd-c3-final-evidence.json` aggregates the final gate outcomes,
CLI/runtime SHA-256 values and runner evidence paths; committed
`probes/status.json` records the same gate summaries. Initial concurrent
attempts are not counted as successful gates. No raw diagnostic dump is
included in committed documentation.

The pre-push fetch confirms `origin/main` is still `291be0cb`, already an
ancestor of perf `ed10137f`, which is already merged into demo. No engine
merge/push is needed. Only `demo/swarm-arena-20261018` is pushed; `.codex/config.toml`
remains local and uncommitted.
