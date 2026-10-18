# Swarm Arena

A deterministic arena-survival demo for zr_vm, based on Claude's design report
`/workspace/zr_vm-review/game-demo-design-20261018.md` (2026-10-18).

Status: **GD-C0 complete; GD-C1 blocked by required P01**. Struct-array element
writeback fails in both interp and binary. Implementation stopped under John's
required-probe rule; see [feature readiness](docs/feature-readiness.md) for the
reproducers and all 22 statuses. Gameplay has not started.
The intended v1 has DOD, OOP, Hybrid and C lanes, CLI and embedded-host drivers,
null and software renderers, deterministic hashes and frame/GC metrics.

Builds and generated evidence belong to this worktree. Existing perf and review
worktrees are read-only. Demos are disabled by default and use CTest label `demo`.

```sh
/workspace/zr_vm-perf/build/tools/bin/cmake -S . -B build/demo-release -G Ninja \
  -DCMAKE_MAKE_PROGRAM=/workspace/zr_vm-perf/build/tools/bin/ninja \
  -DCMAKE_BUILD_TYPE=Release -DZR_VM_BUILD_DEMOS=ON \
  -DBUILD_LANGUAGE_SERVER=OFF -DBUILD_RUST_BINDING=OFF -DBUILD_TESTS=ON \
  -DZR_VM_LIBFFI_INCLUDE_DIR=/workspace/zr_vm-perf/build/local-tools/usr/include/x86_64-linux-gnu \
  -DZR_VM_LIBFFI_LIBRARY=/workspace/zr_vm-perf/build/local-tools/usr/lib/x86_64-linux-gnu/libffi.so
/workspace/zr_vm-perf/build/tools/bin/cmake --build build/demo-release -j6 --target zr_vm_cli
/workspace/zr_vm-perf/build/tools/bin/ctest --test-dir build/demo-release -L demo --output-on-failure
```

The demo CTests currently return nonzero: six reproduction/control checks pass
and `demo_feature_gate` fails. Four reproduction tests use `WILL_FAIL`; their
passing CTest status records the known errors, not feature readiness. Binary
probes remove the copied source directory after compilation to prevent fallback.
Release, ASan/UBSan and forced-GC trees all reproduce these outcomes; sanitizer
checks report no errors with leak detection disabled. These probe checks do not
provide the game's still-missing v1 smoke/performance coverage.

```sh
python3 -I demos/swarm_arena/tools/run_probe.py --cli build/demo-release/bin/zr_vm_cli \
  --mode binary --variant main --out out/p01-binary
python3 -I demos/swarm_arena/tools/run_probe.py --gate
```

Both commands correctly exit 1 until P01 is resolved. To inspect the unchanged
core test inventory, use `ctest --test-dir build/demo-release -N -LE demo
-E '^performance_report$'`: 425 tests. `ZR_VM_TEST_TIER=core` selects fixture tiers
inside the engine runners; standalone demo tests need the explicit `-LE demo`.

Run commands from the repository root. The CLI executable target is
`zr_vm_cli_executable` (its output name is `zr_vm_cli`); Ninja also accepts the
output-name target above. Native plugins will be built under
`build/demo-release/demos/swarm_arena/native`, selected with `ZR_VM_NATIVE_PATH`.
The manifest's `nativeProviders` field is not used by the runtime.

See [canonical rules](docs/rules.md) and [feature readiness](docs/feature-readiness.md).
The phase order is C0–C5, H1–H6, then M-tier as time permits. A required C1 failure
stops implementation and is escalated with its reproducer; engine fixes require
separate review. v1 completion and perf claims require the checks in report §8.5.
