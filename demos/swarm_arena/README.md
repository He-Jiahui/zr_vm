# Swarm Arena

GD-C3 implements a deterministic **DOD, CLI-driven `smoke`** game with a bot,
counting-sort grid, bullets, queued damage/deaths, particles, metrics and mode C
bulk rendering through `zrgame.gfx`'s null backend. The default 120-frame,
seed-12345 golden is **224663030**, identical across three repetitions in each
of interpreter and source-removed binary execution.

The GD-C2 native runtime and all required GD-C1 probes remain supported.
See [rules](docs/rules.md), [composition workarounds](docs/gd-c3-decisions.md),
[native APIs](docs/native-runtime.md), [readiness](docs/feature-readiness.md), and
[verification](docs/verification.md). GD-C4 and later work remain deferred.

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
  > /tmp/zr-swarm-gd-c3-configure.log 2>&1
cmake --build build/demo-release -j3 --target zr_vm_cli_executable \
  zrgame_plugins zrgame_ffi zrgame_native_tests zr_swarm_probe_host \
  zrgame_probe zrgame_inline_probe > /tmp/zr-swarm-gd-c3-build.log 2>&1
```

The detector configurations use Debug with
`-DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'`;
`build/demo-gcstress` additionally defines `ZR_DEBUG_GARBAGE_COLLECT_MEM_TEST`.
Use the same other flags and build targets. Run one configuration at a time
with CTest `-j1`; concurrent sanitizer workers exhausted instance memory during
validation. Preserve the build-local
`Testing/Temporary` and `demos/swarm_arena/probe-results` symlinks to `/tmp` when
rerunning gates. Set `TMPDIR=/tmp/zr-swarm-gd-c3-runtime` (create it first) for
the retained native smoke's private evidence.

## Run and test

```sh
export ZR_VM_NATIVE_PATH="$PWD/build/demo-release/demos/swarm_arena/native"
build/demo-release/bin/zr_vm_cli demos/swarm_arena/game/swarm_arena.zrp \
  --execution-mode interp -- --scenario smoke --lane dod --frames 120 \
  --seed 12345 --render null --submit bulk --expect-hash 224663030 \
  > /tmp/zr-swarm-gd-c3-run.log 2>&1
rg '^SWARM_OK ' /tmp/zr-swarm-gd-c3-run.log

# Fresh private compile; remove every .zr, then run binary three times.
python3 -I demos/swarm_arena/tools/run_game.py --mode binary \
  --cli build/demo-release/bin/zr_vm_cli \
  --native-dir build/demo-release/demos/swarm_arena/native \
  > /tmp/zr-swarm-gd-c3-binary.log 2>&1

# Stage game/tests in a private project, then use stock zr_vm_cli test.
python3 -I demos/swarm_arena/tools/run_zr_unit_tests.py \
  --cli build/demo-release/bin/zr_vm_cli \
  --native-dir build/demo-release/demos/swarm_arena/native \
  > /tmp/zr-swarm-gd-c3-units.log 2>&1

ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ZR_VM_TEST_TIER=core \
  ctest --test-dir build/demo-release -L demo -j1 --output-on-failure \
  > /tmp/zr-swarm-gd-c3-ctest.log 2>&1
```

A run prints `SWARM_OK scenario=smoke lane=dod mode=cli frames=120
hash=224663030 ...` on one line, with counters, the render hash, tick/render
p95 microseconds and GC telemetry. Timing and GC values are excluded from the
state hash. GC counters/totals use per-frame snapshot deltas; `gc_max_us` is
the VM's cumulative collection maximum and can include boot. `--frames`
supports 1–120; `--seed` supports 1–2147483646.
`--gc-budget-us` supports 0–1000000 and only the `auto` policy is implemented.
`--help` prints usage. Bad arguments or an expected-hash mismatch print
`SWARM_ERROR`, run `finally` (reporting `SWARM_FINALLY cleanup=ok`), and exit 2.
The CLI prints script return values independently of OS status, so this failure
path explicitly exits after cleanup.

There are **71 demo CTests**: 67 retained checks plus
`demo_cli_smoke_interp_dod`, `demo_cli_smoke_binary_dod`,
`demo_zr_unit_tests`, and `demo_bad_scenario`. Smoke tests each enforce three
runs against [hashes.json](goldens/hashes.json), final counters and null-render
hash. Unit coverage is ten tests in the three requested files. The bad-input
gate checks six cases in each mode, including exception/finally cleanup and hash
mismatch. No detector diagnostic can satisfy these gates.

GD-C1 retains 38/38 required checks, 58/62 successful mode checks overall and
18/18 required P22 pairs. Its four optional failing mode checks still reproduce
P19 reflection and P14 v2-package diagnostics. The actual game uses a legacy
v1 `$swarmcore` dependency and `import("&swarmcore")`; it does not use
`@swarmcore`. Stock CLI `test` ignores `testSource`, which is why the staging
runner is required. All generated artifacts and raw logs for these runners
stay under `/tmp/zr-swarm-gd-c3-*`.

Demos are off by default and labeled `demo`. The core inventory remains 515:
`ctest --test-dir build/demo-release -N -LE demo -E '^performance_report$'`.
`ZR_VM_TEST_TIER=core` selects engine fixture tiers; use `-LE demo` to exclude
standalone demo CTests. Native plugins share `libzrgame_runtime.so` and load
from `ZR_VM_NATIVE_PATH`; the manifest's `nativeProviders` field is unused.

Host drive, other lanes/scenarios, full v1 gameplay, software/GL rendering,
images and performance budgets remain later milestones. The null backend
consumes sprites and hashes content; it writes no image.
