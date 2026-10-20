# Swarm Arena

A deterministic arena-survival demo for zr_vm, based on the design report
`/workspace/zr_vm-review/game-demo-design-20261018.md`.

Status: **GD-C2 complete: shared native runtime, host/gfx modules and null
backend**, including mode B `spriteInst`. All required GD-C1 probes pass in
Release, ASan/UBSan, and forced full GC with ASan/UBSan, in interpreter and
source-removed binary modes. See [native APIs](docs/native-runtime.md) and
[feature readiness](docs/feature-readiness.md). Gameplay and GD-C3+ remain deferred.

Demos are disabled by default and use CTest label `demo`. Run from the repository
root using the existing tools and dependency paths:

```sh
export PATH="/workspace/zr_vm-perf/build/local-tools/usr/bin:/workspace/zr_vm-perf/build/tools/bin:$PATH"
export LD_LIBRARY_PATH="/workspace/zr_vm-perf/build/local-tools/usr/lib/x86_64-linux-gnu"
cmake -S . -B build/demo-release -G Ninja \
  -DCMAKE_MAKE_PROGRAM=/workspace/zr_vm-perf/build/tools/bin/ninja \
  -DCMAKE_BUILD_TYPE=Release -DZR_VM_BUILD_DEMOS=ON \
  -DBUILD_LANGUAGE_SERVER=OFF -DBUILD_RUST_BINDING=OFF -DBUILD_TESTS=ON \
  -DPython3_EXECUTABLE=/workspace/zr_vm-perf/build/tools/bin/python3 \
  -DZR_VM_LIBFFI_INCLUDE_DIR=/workspace/zr_vm-perf/build/local-tools/usr/include/x86_64-linux-gnu \
  -DZR_VM_LIBFFI_LIBRARY=/workspace/zr_vm-perf/build/local-tools/usr/lib/x86_64-linux-gnu/libffi.so
cmake --build build/demo-release -j3 --target zr_vm_cli_executable zrgame_plugins zrgame_ffi zrgame_native_tests zr_swarm_probe_host zrgame_probe zrgame_inline_probe
ctest --test-dir build/demo-release -L demo -j4 --output-on-failure
```

Each configuration passes 67/67 demo CTests: the existing 64 GD-C1 checks plus
the native C runner (eight tests) and GD-C2 interpreter/binary smokes. GD-C1 has
38/38 required mode checks and 58/62 successful mode checks overall, P22, and
the feature gate. Four optional mode checks reproduce their recorded errors through `WILL_FAIL`;
their feature status remains FAIL. Successful binary cases compile fresh artifacts and
remove source directories before execution. Detector reports cannot pass as
expected feature failures.

Local build `probe-results` and CTest temporary directories are symlinked to
`/tmp/zr-swarm-*` so raw logs stay outside the repository. Preserve those links
when rerunning detector checks. [Verification](docs/verification.md) records
the three build configurations, commands, and evidence paths.

```sh
python3 -I demos/swarm_arena/tools/run_probe.py --case p04-main \
  --cli build/demo-release/bin/zr_vm_cli --mode binary \
  --native-dir build/demo-release/demos/swarm_arena/native --out /tmp/zr-swarm-p04-binary
python3 -I demos/swarm_arena/tools/run_probe.py --gate
python3 -I demos/swarm_arena/tools/run_native_smoke.py --mode binary \
  --cli build/demo-release/bin/zr_vm_cli \
  --native-dir build/demo-release/demos/swarm_arena/native
```

The required-feature gate succeeds. To inspect the core inventory, use
`ctest --test-dir build/demo-release -N -LE demo -E '^performance_report$'`:
515 tests. Engine regression suites run in the unchanged engine worktree;
native acceptance and GD-C1 checks run in the demo trees. `ZR_VM_TEST_TIER=core` selects
engine fixture tiers; use `-LE demo` to exclude standalone demo CTests.

Native plugins are built under `build/demo-release/demos/swarm_arena/native`,
selected with `ZR_VM_NATIVE_PATH` or the tested project/native search path.
The manifest's `nativeProviders` field is not used by the runtime. The null
backend counts instances and hashes their float32 content; it creates no image.
Game lanes, the `zr_swarm` host, gameplay hashes and performance baselines remain
future work. See [canonical rules](docs/rules.md) for those later phases.
