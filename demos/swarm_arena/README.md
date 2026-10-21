# Swarm Arena

GD-C4 adds **`zr_swarm`, a C host for the deterministic DOD `smoke` game**.
The host owns the frame loop, null backend submission, GC policy and file
metrics. Host and CLI drive share the GD-C3 simulation and match the default
120-frame, seed-12345 golden **224663030**. The CLI retains interpreter and
source-removed binary execution.

The GD-C2 native runtime and all required GD-C1 probes remain supported.
See [rules](docs/rules.md), [composition workarounds](docs/gd-c3-decisions.md),
[native APIs](docs/native-runtime.md), [readiness](docs/feature-readiness.md), and
[verification](docs/verification.md). Host choices and measurement limits are
in [GD-C4 decisions](docs/gd-c4-decisions.md); GD-C5 and later work remain deferred.

## Build with the system toolchain

From the repository root, using system CMake 3.31.6, Ninja, Clang/LLVM 19 and
libffi (no `build/tools` or `build/local-tools` paths):

```sh
cmake -S . -B build/demo-release -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/clang-19 -DCMAKE_BUILD_TYPE=Release \
  -DZR_VM_BUILD_DEMOS=ON -DBUILD_TESTS=ON \
  -DBUILD_LANGUAGE_SERVER=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF \
  -DBUILD_RUST_BINDING=OFF -DZR_VM_REGISTER_PERFORMANCE_CTEST=ON \
  -DPython3_EXECUTABLE=/usr/bin/python3 \
  -DZR_VM_LIBFFI_INCLUDE_DIR=/usr/include/x86_64-linux-gnu \
  -DZR_VM_LIBFFI_LIBRARY=/usr/lib/x86_64-linux-gnu/libffi.so \
  > /tmp/zr-swarm-gd-c4-configure.log 2>&1
cmake --build build/demo-release -j3 --target zr_vm_cli_executable zr_swarm \
  zrgame_plugins zrgame_ffi zrgame_native_tests zr_swarm_probe_host \
  zrgame_probe zrgame_inline_probe > /tmp/zr-swarm-gd-c4-build.log 2>&1
```

The memory-error-detector configuration uses Debug with
`-DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'`;
`build/demo-gcstress` additionally defines `ZR_DEBUG_GARBAGE_COLLECT_MEM_TEST`.
Use the same other flags and build targets. Run one configuration at a time
with CTest `-j1` to stay within available memory. Keep `Testing/Temporary`,
`CMakeFiles/CMakeConfigureLog.yaml` and `demos/swarm_arena/probe-results`
symlinked under `/tmp/zr-swarm-gd-c4-*`. Set
`TMPDIR=/tmp/zr-swarm-gd-c4-runtime` (create it first) for private runtime
evidence. All raw build/test/runtime logs belong under that task prefix.

## Run and test

```sh
export ZR_VM_NATIVE_PATH="$PWD/build/demo-release/demos/swarm_arena/native"
build/demo-release/bin/zr_vm_cli demos/swarm_arena/game/swarm_arena.zrp \
  --execution-mode interp -- --scenario smoke --lane dod --frames 120 \
  --seed 12345 --render null --submit bulk --expect-hash 224663030 \
  > /tmp/zr-swarm-gd-c4-run.log 2>&1

build/demo-release/bin/zr_swarm \
  --project demos/swarm_arena/game/swarm_arena.zrp \
  --scenario smoke --lane dod --frames 120 --seed 12345 \
  --render null --submit bulk --gc-policy slack --gc-budget-us 500 \
  --warmup 0 --out /tmp/zr-swarm-gd-c4-host-run --expect-hash 224663030 \
  > /tmp/zr-swarm-gd-c4-host-run.log 2>&1

# Fresh private compile; remove every .zr, then run binary three times.
python3 -I demos/swarm_arena/tools/run_game.py --mode binary \
  --cli build/demo-release/bin/zr_vm_cli \
  --native-dir build/demo-release/demos/swarm_arena/native \
  > /tmp/zr-swarm-gd-c4-binary.log 2>&1

# Stage game/tests in a private project, then use stock zr_vm_cli test.
python3 -I demos/swarm_arena/tools/run_zr_unit_tests.py \
  --cli build/demo-release/bin/zr_vm_cli \
  --native-dir build/demo-release/demos/swarm_arena/native \
  > /tmp/zr-swarm-gd-c4-units.log 2>&1

ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ZR_VM_TEST_TIER=core \
  ctest --test-dir build/demo-release -L demo -j1 --output-on-failure \
  > /tmp/zr-swarm-gd-c4-ctest.log 2>&1
```

A run prints `SWARM_OK scenario=smoke lane=dod mode=cli frames=120
hash=224663030 ...` on one line, with counters, the render hash, tick/render
p95 microseconds and GC telemetry. Timing and GC values are excluded from the
state hash. GC counters/totals use per-frame snapshot deltas; `gc_max_us` is
the VM's cumulative collection maximum and can include boot. `--frames`
supports 1–120; `--seed` supports 1–2147483646.
`--gc-budget-us` supports 0–1000000; CLI drive uses the `auto` policy.
`--help` prints usage. Bad arguments or an expected-hash mismatch print
`SWARM_ERROR`, run `finally` (reporting `SWARM_FINALLY cleanup=ok`), and exit 2.
The CLI prints script return values independently of OS status, so this failure
path explicitly exits after cleanup.

The host supports `--gc-policy auto|slack|manual`. Manual GC requests a minor
every `--gc-minor-interval` completed frames (default 30, range 1–120).
`--warmup` accepts 0. `--out` defaults to `out/swarm`; the host creates parents
and writes `frames.csv` and `summary.json` conforming to
[the schema](docs/perf-summary.schema.json). CLI output stays textual.
Unsupported or duplicate settings, missing values, bad projects and expected
hash mismatches exit 2 with `SWARM_ERROR`. Option errors name the field.
`--project` is required except with `--help`.

Host timing covers poll → tick → render → backend submit/present → GC policy.
GC snapshots come from the owner thread. Allocation values estimate positive
managed-memory deltas; exact pause p99 is unavailable and recorded as null.
These metrics establish no performance target. The detector gates retain the
existing policy: leak detection disabled.

There are **73 demo CTests**: 67 retained checks plus
`demo_cli_smoke_interp_dod`, `demo_cli_smoke_binary_dod`,
`demo_zr_unit_tests`, `demo_bad_scenario`, `demo_host_smoke_dod`, and
`demo_cli_host_parity`. CLI smoke tests each enforce three
runs against [hashes.json](goldens/hashes.json), final counters and null-render
hash. Unit coverage is ten tests in the three requested files. The bad-input
gate checks six cases in each mode, including exception/finally cleanup and hash
mismatch. Host tests check the golden, frame-60 checkpoint, final counters,
artifact schema/CSV consistency, all GC policies, failure cleanup and CLI
parity for both default and boundary inputs. Detector findings fail the gates.

GD-C1 retains 38/38 required checks, 58/62 successful mode checks overall and
18/18 required P22 pairs. Its four optional failing mode checks still reproduce
P19 reflection and P14 v2-package diagnostics. The actual game uses a legacy
v1 `$swarmcore` dependency and `import("&swarmcore")`; it does not use
`@swarmcore`. Stock CLI `test` ignores `testSource`, which is why the staging
runner is required. All generated artifacts and raw logs for these runners
stay under the configured `/tmp/zr-swarm-gd-c4-*` evidence root.

Demos are off by default and labeled `demo`. The core inventory remains 515:
`ctest --test-dir build/demo-release -N -LE demo -E '^performance_report$'`.
`ZR_VM_TEST_TIER=core` selects engine fixture tiers; use `-LE demo` to exclude
standalone demo CTests. Native plugins share `libzrgame_runtime.so` and load
from `ZR_VM_NATIVE_PATH`; the manifest's `nativeProviders` field is unused.

Other lanes/scenarios, full v1 gameplay, software/GL rendering,
images and performance budgets remain later milestones. The null backend
consumes sprites and hashes content; it writes no image.
