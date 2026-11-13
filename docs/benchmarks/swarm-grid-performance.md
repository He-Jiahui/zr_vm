---
related_code:
  - demos/swarm_arena/game/src/lanes/dod/grid.zr
  - demos/swarm_arena/game/src/lanes/dod/systems.zr
  - demos/swarm_arena/game/tests/grid_tests.zr
  - tests/fixtures/projects/swarm_grid/src/grid_tests.zr
  - tests/benchmarks/run_swarm_performance.py
  - tests/benchmarks/swarm_grid_runner.c
  - demos/swarm_arena/CMakeLists.txt
implementation_files:
  - demos/swarm_arena/game/src/lanes/dod/grid.zr
  - demos/swarm_arena/game/src/lanes/dod/systems.zr
  - tests/benchmarks/run_swarm_performance.py
  - tests/benchmarks/swarm_grid_runner.c
plan_sources:
  - user: 2026-11-03 继续优化 Swarm 示例的运行性能
  - .codex/plans/20261103-swarm-performance.md
tests:
  - tests/fixtures/projects/swarm_grid/src/grid_tests.zr
  - demos/swarm_arena/game/tests/grid_tests.zr
  - tests/acceptance/2026-11-03-swarm-grid-performance.md
doc_type: module-detail
---

# Swarm spatial grid performance

The DOD lane builds one spatial grid per tick, before steering and integration.
Both steering and collision visit neighboring cells in row/column order, then
visit each cell's enemies in ascending dense world-slot order. This sequence
affects floating-point accumulation and collision event order; preserving it is
required even when changing the physical grid storage.

The original builder resets all 4096 cell counts and computes a global prefix
sum over all 4096 cells on every frame. This work is independent of the live
enemy count. The sparse builder remembers the cells occupied by the previous
build, clears their counts/ranges, and records the current occupied cells in
first-encounter order. It computes a separate contiguous block for each current
cell and scatters enemies into those blocks in dense world order. Cached cell
indices avoid recomputing an enemy's cell in the scatter pass.

`starts[cell]` is the beginning of that cell's block; its end is
`starts[cell] + counts[cell]`. Adjacent cell IDs need not have adjacent blocks.
Consumers must use the count rather than the next cell's start. Empty cells
have zero candidates after movement, swap removal, an empty rebuild, and reuse.
The backing block order is private to the grid and is not a gameplay sort.

For N enemies and U occupied cells, rebuild work is O(N + U_previous + U),
rather than O(N + 4096). The occupied list is bounded by min(capacity, 4096),
and the cached cell and scatter arrays are bounded by enemy capacity. Arrays
are allocated at boot, with no per-frame grid container allocation. The native
C lane keeps its independent implementation as an equivalence oracle.

## Reproduction without source copies

Build and run the focused grid gate from the actual checkout:

```sh
E_ROOT=/mnt/e/cargo-targets/zr_vm
BUILD="$E_ROOT/build/20261103-swarm-performance/gcc-release"
cmake --build "$BUILD" --target zr_swarm_grid_tests
ctest --test-dir "$BUILD" -R '^demo_grid(_game_units)?$' --output-on-failure
```

The tests compare each queried cell to an independent scan of world slots and
exercise movement, swap removal, empty/repeated rebuilds, zero capacity,
clamping, every cell occupied and a single cell containing all 4096 enemies.
The small C runner uses the real game's project context and reads the actual
test fixture. Like the stock CLI test runner, it appends each test call in memory
and creates an independent VM state for each case.
The compiler requires a logical source identity inside the game's source root;
`__swarm_grid_tests.zr` is therefore an in-memory name, with no file staged into
the game. Its diagnostic line numbers refer to the actual fixture followed by
the appended calls. `demo_grid_game_units` runs the game's three existing grid
tests through the same source-copy-free boundary. The grid and world modules
load from the real game.

Use the measurement helper for repeated host observations and deterministic
checks:

```sh
python3 -B -I tests/benchmarks/run_swarm_performance.py \
  --host "$BUILD/bin/zr_swarm" \
  --out "$E_ROOT/reports/20261103-swarm-performance/before" \
  --cpu 2 --repeat 3 --check-c-lane

# After the source optimization, keep the same binary and CPU.
python3 -B -I tests/benchmarks/run_swarm_performance.py \
  --host "$BUILD/bin/zr_swarm" \
  --out "$E_ROOT/reports/20261103-swarm-performance/after" \
  --baseline "$E_ROOT/reports/20261103-swarm-performance/before/results.json" \
  --cpu 2 --repeat 3 --check-c-lane
```

Each output directory must be new. The helper generates only a manifest whose
source and dependency paths reference the real checkout, and retains its path
and hash as reproduction evidence. Outputs stay under the caller's explicit directory.
It does not stage source, remove scripts, or reuse stale compiled game output.
Run from a freshly built host; record its compiler configuration and native
library identities with the report. Baseline comparison requires the identical
host binary, native libraries and CPU, and compares every final success field, entity maxima,
and every frame's gameplay hash/sprite count. Repeated runs and optional C-lane
runs must agree. Committed goldens are enforced for matching scenario lengths.

The default cases are smoke/120, arena_2k/60, swarm_10k/8, bullet_storm/60 and
particle_burst/60. Override them with repeated `--case SCENARIO:FRAMES` options;
these short dense cases are not an 1800-frame completion claim. Timing summaries
report the median of per-run mean tick time and the median of per-run p95;
render and full frame observations are retained separately. A caller may set
`--max-smoke-tick-us` as a local performance budget. Hardware noise, background
work, startup/compilation time and native-library changes must be considered
before interpreting an improvement as a general performance guarantee.

Actual measurements, toolchain gates and remaining limitations are recorded in
[the acceptance record](../../tests/acceptance/2026-11-03-swarm-grid-performance.md).
