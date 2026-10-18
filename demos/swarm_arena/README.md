# Swarm Arena

A deterministic arena-survival demo for zr_vm, based on Claude's design report
`/workspace/zr_vm-review/game-demo-design-20261018.md` (2026-10-18).

Status: GD-C0 scaffold. Gameplay begins only after the GD-C1 feature gate passes.
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

Run commands from the repository root. The CLI executable target is
`zr_vm_cli_executable` (its output name is `zr_vm_cli`); Ninja also accepts the
output-name target above. Native plugins will be built under
`build/demo-release/demos/swarm_arena/native`, selected with `ZR_VM_NATIVE_PATH`.
The manifest's `nativeProviders` field is not used by the runtime.

See [canonical rules](docs/rules.md) and [feature readiness](docs/feature-readiness.md).
The phase order is C0–C5, H1–H6, then M-tier as time permits. A required C1 failure
stops implementation and is escalated with its reproducer; engine fixes require
separate review. v1 completion and perf claims require the checks in report §8.5.
