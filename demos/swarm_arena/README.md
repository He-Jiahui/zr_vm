# Swarm Arena

GD-C5 implements full v1 gameplay on the existing DOD lane: separation,
Grunt/Runner/Tank enemies, nearest and radial weapons, pierce, XP pickups,
magnet collection, deterministic upgrades, boss waves and cosmetic particles.
CLI and DOD host drive share one Session and support null rendering with bulk
submission. GD-H1 adds independent host `--lane c` reference gameplay and a
DOD==C smoke gate under the same rules; see [GD-H1 decisions](docs/gd-h1-decisions.md).
The default 120-frame smoke golden is **552988638** for seed
12345; checkpoint 60 is **42922906**. The deliberate change from GD-C4 is
explained in [decisions](docs/gd-c5-decisions.md).

| Scenario | Enemy cap | Default frames | Purpose |
| --- | --- | --- | --- |
| smoke | 200 | 120 | Fast gameplay, detector and forced-GC gate |
| arena_2k | 2000 | 1800 | Full v1 arena |
| swarm_10k | 10000 | 1800 | Dense separation |
| bullet_storm | 1000 | 1800 | 6000 aimed bullets/second before upgrades |
| particle_burst | 2000 | 1800 | 64 particles and a transient SoA request per kill |

See [rules](docs/rules.md), [native APIs](docs/native-runtime.md),
[readiness](docs/feature-readiness.md), [verification](docs/verification.md),
and the [acceptance note](../../tests/acceptance/2026-10-21-swarm-gd-c5-gameplay.md).
Host metric precision and GC policy semantics remain in
[GD-C4 decisions](docs/gd-c4-decisions.md).

## Build with the system toolchain

From the repository root, using system CMake, Ninja, Clang/LLVM 19 and libffi:

```sh
cmake -S . -B build/demo-release -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/clang-19 -DCMAKE_BUILD_TYPE=Release \
  -DZR_VM_BUILD_DEMOS=ON -DBUILD_TESTS=ON \
  -DBUILD_LANGUAGE_SERVER=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF \
  -DBUILD_RUST_BINDING=OFF -DZR_VM_REGISTER_PERFORMANCE_CTEST=ON \
  -DPython3_EXECUTABLE=/usr/bin/python3 \
  -DZR_VM_LIBFFI_INCLUDE_DIR=/usr/include/x86_64-linux-gnu \
  -DZR_VM_LIBFFI_LIBRARY=/usr/lib/x86_64-linux-gnu/libffi.so \
  > /tmp/zr-swarm-gd-c5-configure.log 2>&1
cmake --build build/demo-release -j3 --target zr_vm_cli_executable zr_swarm \
  zrgame_plugins zrgame_ffi zrgame_native_tests zr_swarm_probe_host \
  zrgame_probe zrgame_inline_probe > /tmp/zr-swarm-gd-c5-build.log 2>&1
```

Retain the documented Debug flags in the existing memory-error-detector and
forced-GC trees. Use system tools and the same targets. Run these configurations
sequentially with CTest `-j1`. Keep CTest Temporary, probe results, configure
logs and Ninja logs symlinked under `/tmp/zr-swarm-gd-c5-*`. Create and set
`TMPDIR=/tmp/zr-swarm-gd-c5-runtime` for private runner evidence. Relocate any
recreated `.ninja_log` after a build. All raw logs belong under that prefix.

## Run and test

```sh
export ZR_VM_NATIVE_PATH="$PWD/build/demo-release/demos/swarm_arena/native"
export TMPDIR=/tmp/zr-swarm-gd-c5-runtime
mkdir -p "$TMPDIR"
build/demo-release/bin/zr_vm_cli demos/swarm_arena/game/swarm_arena.zrp \
  --execution-mode interp -- --scenario smoke --lane dod --frames 120 \
  --seed 12345 --render null --submit bulk --expect-hash 552988638 \
  > /tmp/zr-swarm-gd-c5-cli.log 2>&1
build/demo-release/bin/zr_swarm \
  --project demos/swarm_arena/game/swarm_arena.zrp \
  --scenario smoke --lane dod --seed 12345 --render null --submit bulk \
  --gc-policy slack --gc-budget-us 500 --warmup 0 \
  --out /tmp/zr-swarm-gd-c5-host --expect-hash 552988638 \
  > /tmp/zr-swarm-gd-c5-host.log 2>&1

# Omit --frames to use the selected scenario's default length.
build/demo-release/bin/zr_swarm \
  --project demos/swarm_arena/game/swarm_arena.zrp \
  --scenario arena_2k --lane dod --render null --submit bulk \
  --out /tmp/zr-swarm-gd-c5-arena \
  > /tmp/zr-swarm-gd-c5-arena.log 2>&1

build/demo-release/bin/zr_swarm \
  --project demos/swarm_arena/game/swarm_arena.zrp \
  --scenario smoke --lane c --frames 120 --seed 12345 --render null --submit bulk \
  --out /tmp/zr-swarm-gd-h1-c --expect-hash 552988638
ctest --test-dir build/demo-release -R '^(demo_host_smoke_c|demo_lane_equivalence)$' \
  -j1 --output-on-failure

python3 -I demos/swarm_arena/tools/run_game.py --mode binary \
  --cli build/demo-release/bin/zr_vm_cli \
  --native-dir build/demo-release/demos/swarm_arena/native \
  > /tmp/zr-swarm-gd-c5-binary.log 2>&1
python3 -I demos/swarm_arena/tools/run_zr_unit_tests.py \
  --cli build/demo-release/bin/zr_vm_cli \
  --native-dir build/demo-release/demos/swarm_arena/native \
  > /tmp/zr-swarm-gd-c5-units.log 2>&1
ZR_VM_TEST_TIER=core ctest --test-dir build/demo-release -L demo -j1 \
  --output-on-failure > /tmp/zr-swarm-gd-c5-ctest.log 2>&1
```

A completed run prints one `SWARM_OK` line with the scenario, drive, frames,
seed, simulation hash, checkpoint 60, entity counts, hits, kills, drops,
progression, boss waves and render content hash. CLI output adds descriptive
tick/render p95 and GC observations. Timing, GC, renderer and particles are
excluded from the simulation hash. Host artifacts remain schema-checked
`frames.csv` and `summary.json`; exact pause p99 is unavailable and null, and
allocation observations are estimates. These metrics establish no performance
acceptance. The retained policy is leak detection disabled.

`--frames` accepts 1–120 in smoke and 1–1800 elsewhere; defaults are selected
after parsing, independently of option order. Seeds accept 1–2147483646.
Both drives accept auto/slack/manual GC and budgets 0–1000000 microseconds;
manual requests a minor every `--gc-minor-interval` completed frames (default
30, range 1–1800). CLI slack requests one bounded minor step when frame time
remains; host slack uses its existing owner-thread step loop. Policies cannot
change simulation hashes. `--help` prints usage. Invalid options and hash
mismatches exit 2 with a field-naming `SWARM_ERROR`; CLI also reports exactly
one finally cleanup marker. Host `--project` is required except with help.
Host `--warmup` stays 0, and `--out` defaults to `out/swarm`. CLI output stays
textual. Unsupported lanes, renderers, submission modes and `sweep` fail.

The C host lane uses the same output schema with `lane=c`, `exec=native` and
the same CSV columns. Its GC options are inert because it has no managed heap;
managed counters stay zero, and native allocations are explicitly unmeasured.
CLI drive continues to support DOD only.

The demo CTests retain the 77 GD-C5 checks and add `demo_host_smoke_c` and
`demo_lane_equivalence`, for 79 total. The four
`demo_cli_host_<scenario>_dod` gates retain their completion/parity checks. Release
runs each new scenario for 1800 frames with CLI and host. Stress builds use
four-frame scenario checks alongside full 120-frame smoke and twenty ZR units.
The new Release pair runs may execute together; stress pairs and CTests run
sequentially. Every gate checks committed hashes and final state, plus host
schema/CSV consistency. Smoke still repeats three times per interpreter/binary
mode; the binary runner removes every copied script before fresh execution.
The invalid-input gate covers fourteen cases per mode. Host checks cover
all GC policies, boundary inputs, failure cleanup and twenty-eight failures.

Required GD-C1 checks remain 38/38, with 18/18 required P22 pairs. The four
optional failing mode checks retain the deferred reflection/v2-package status.
The game keeps v1 `$swarmcore` / `import("&swarmcore")`. The unit staging runner
still bridges the stock CLI's source discovery to `game/tests`.

Demos remain off by default with CTest label `demo`. Core inventory stays 515;
use `-LE demo -E '^performance_report$'` to inspect it. Native plugins share
`libzrgame_runtime.so` and load from `ZR_VM_NATIVE_PATH`. OOP/Hybrid lanes, `sweep`,
software/GL rendering, images, interactive input, performance harnesses and all
GD-H2+ / M milestones remain deferred. The null backend writes no image.
