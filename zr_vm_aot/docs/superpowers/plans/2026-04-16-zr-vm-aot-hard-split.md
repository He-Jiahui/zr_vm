# ZR VM AOT Hard Split Implementation Plan

> Active todo list for the 2026-04-16 hard split. This plan executes directly against `main` and removes AOT from supported main-repo paths.

## Phase 1: Contract The Supported Main Surface

- [ ] Remove top-level `ZR_VM_BUILD_AOT` option and related status/compile definitions from [CMakeLists.txt](/E:/Git/zr_vm/CMakeLists.txt).
- [ ] Remove AOT registration branches from [tests/CMakeLists.txt](/E:/Git/zr_vm/tests/CMakeLists.txt) and related test runner parameters.
- [ ] Remove AOT declarations from [writer.h](/E:/Git/zr_vm/zr_vm_parser/include/zr_vm_parser/writer.h).
- [ ] Remove AOT umbrella export from [zr_vm_library.h](/E:/Git/zr_vm/zr_vm_library/include/zr_vm_library.h).

## Phase 2: Delete Main CLI AOT Behavior

- [ ] Remove `aot_c` / `aot_llvm` execution modes and AOT flags from [command.h](/E:/Git/zr_vm/zr_vm_cli/src/zr_vm_cli/command/command.h) and [command.c](/E:/Git/zr_vm/zr_vm_cli/src/zr_vm_cli/command/command.c).
- [ ] Remove AOT compile pipeline, host toolchain probing, and AOT artifact bookkeeping from [compiler.c](/E:/Git/zr_vm/zr_vm_cli/src/zr_vm_cli/compiler/compiler.c).
- [ ] Remove AOT manifest fields and path resolvers from [project.h](/E:/Git/zr_vm/zr_vm_cli/src/zr_vm_cli/project/project.h) and [project.c](/E:/Git/zr_vm/zr_vm_cli/src/zr_vm_cli/project/project.c).
- [ ] Remove AOT runtime configuration and execution branches from [runtime.c](/E:/Git/zr_vm/zr_vm_cli/src/zr_vm_cli/runtime/runtime.c).

## Phase 3: Delete Main Rust Binding AOT Surface

- [ ] Remove AOT manifest getter declarations from [zr_vm_rust_binding.h](/E:/Git/zr_vm/zr_vm_rust_binding/include/zr_vm_rust_binding.h).
- [ ] Remove matching C ABI implementations from [api.c](/E:/Git/zr_vm/zr_vm_rust_binding/src/zr_vm_rust_binding/api.c).
- [ ] Remove raw FFI declarations from [zr_vm_rust_binding_sys/lib.rs](/E:/Git/zr_vm/zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/lib.rs).
- [ ] Remove safe Rust manifest fields and loaders from [zr_vm_rust_binding/lib.rs](/E:/Git/zr_vm/zr_vm_rust_binding/rust/zr_vm_rust_binding/src/lib.rs).

## Phase 4: Move Residual AOT Into `zr_vm_aot/`

- [ ] Create dormant archive folder `zr_vm_aot/` with a short ownership/status note.
- [ ] Move `zr_aot_abi.h`, `aot_runtime.[ch]`, and parser `backend_aot/` sources into the archive tree.
- [ ] Move direct AOT tests and any AOT-only docs into `zr_vm_aot/`.
- [ ] Remove AOT-only generated assets from active main fixtures/benchmarks when moving them is not useful.

## Phase 5: Clean Main Docs And Validate

- [ ] Remove AOT references from active main CLI/tooling docs and test-order docs.
- [ ] Run a focused configure/build/test pass proving the default main tree no longer depends on AOT.
- [ ] Run Rust binding cargo checks/tests against the contracted manifest ABI.
- [ ] Report remaining risks limited to dormant `zr_vm_aot/` only.
