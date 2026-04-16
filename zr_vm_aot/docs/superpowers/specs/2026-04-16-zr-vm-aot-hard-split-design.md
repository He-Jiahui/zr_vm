---
related_code:
  - CMakeLists.txt
  - tests/CMakeLists.txt
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_emitter.c
  - zr_vm_library/include/zr_vm_library.h
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_rust_binding/include/zr_vm_rust_binding.h
  - zr_vm_rust_binding/src/zr_vm_rust_binding/api.c
  - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/lib.rs
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/src/lib.rs
implementation_files:
  - CMakeLists.txt
  - tests/CMakeLists.txt
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_library/include/zr_vm_library.h
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_rust_binding/include/zr_vm_rust_binding.h
plan_sources:
  - user: 2026-04-16 全仓 AOT 硬分离，主仓移出到 zr_vm_aot，主链路不再相关
tests:
  - tests/CMakeLists.txt
  - tests/cmake/run_cli_suite.cmake
  - tests/cmake/run_projects_suite.cmake
doc_type: design
---

# ZR VM AOT Hard Split Design

## Goal

Remove AOT from the supported main-repo product surface and isolate the existing AOT implementation under a dormant top-level `zr_vm_aot/` archive.

After this split:

- the main repo no longer exposes AOT through build options, CLI modes, parser public headers, library umbrella headers, or Rust binding ABI
- default `cmake`, `ctest`, CLI, Rust, and docs flows do not mention or depend on AOT
- residual AOT implementation and directly related tests live under `zr_vm_aot/` only and are not wired into main build or test entrypoints

## Non-Goals

- keeping backward-compatible AOT flags in the main CLI
- keeping AOT manifest fields in supported main ABI contracts
- making `zr_vm_aot/` a fully maintained standalone product in this slice
- preserving checked-in AOT artifacts in active main test fixtures just for historical continuity

## Problem Statement

AOT is no longer a local parser backend. It currently leaks through several main-repo contracts:

- top-level build option `ZR_VM_BUILD_AOT`
- parser public writer API (`WriteAot*`)
- library public umbrella include (`aot_runtime.h`)
- CLI compile, run, manifest, and help surfaces
- Rust binding manifest ABI and safe Rust structs
- default test suite registration and benchmark infrastructure

That means the main repository still promises AOT as a supported capability even if it is turned off by default. A hard split requires deleting those promises, not merely hiding them behind `OFF` toggles.

## Chosen Architecture

### 1. Main repo contract contraction

The supported main-repo contract is reduced to:

- interpreter execution
- `.zro` binary compilation and execution
- optional `.zri` intermediate emission
- project manifest metadata for source hash, `.zro`, `.zri`, and imports only

AOT-specific enums, flags, helpers, headers, manifest fields, and test registration are removed from supported main modules.

### 2. Dormant archive layout

Residual AOT implementation moves under a new top-level folder:

```text
zr_vm_aot/
  README.md
  zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  zr_vm_library/include/zr_vm_library/aot_runtime.h
  zr_vm_library/src/zr_vm_library/aot_runtime.c
  zr_vm_parser/src/zr_vm_parser/backend_aot/...
  tests/...
  docs/...
```

The archive keeps the previous source layout fragments so the code can still be understood or revived later without mixing back into supported main modules.

### 3. Main build and test behavior

Top-level main CMake no longer defines or reports `ZR_VM_BUILD_AOT`.

Main `tests/CMakeLists.txt` no longer:

- gates suites on AOT
- registers AOT parser tests
- passes AOT build flags into test runners
- includes AOT cases in default language pipeline or CLI/project suites

Any future AOT work must be reintroduced explicitly from `zr_vm_aot/`, not through dormant conditionals in the main tree.

## Public Surface Changes

### Parser

- remove `SZrAotWriterOptions`
- remove `ZrParser_Writer_WriteAotCFile*`
- remove `ZrParser_Writer_WriteAotLlvmFile*`
- remove main parser dependency on `zr_aot_abi.h`

### Library

- remove `#include "zr_vm_library/aot_runtime.h"` from `zr_vm_library/include/zr_vm_library.h`
- move `aot_runtime.[ch]` out of supported main library sources

### CLI

- delete execution modes `aot_c` and `aot_llvm`
- delete flags `--emit-aot-c`, `--emit-aot-llvm`, `--require-aot-path`
- delete AOT compile steps, host toolchain probing, runtime configuration, and manifest bookkeeping
- shrink manifest entries to source hash, zro hash, zro path, zri path, imports

### Rust binding

- delete AOT manifest getter functions from `zr_vm_rust_binding.h`
- delete matching raw FFI declarations from `zr_vm_rust_binding_sys`
- delete matching safe Rust `ManifestEntry` fields and loaders from `zr_vm_rust_binding`

## Manifest Policy

The supported main manifest contract becomes AOT-free.

Supported fields:

- `module`
- `hash`
- `zro_hash`
- `zro`
- `zri`
- `imports`
- `import`
- `end`

AOT-only fields are removed from both writing and reading logic in main code. This is intentional contract shrinkage rather than a compatibility-preserving parse path.

## Test And Fixture Policy

Active main tests keep only interp/binary behavior.

Actions:

- move direct AOT tests under `zr_vm_aot/tests/`
- remove AOT registration from main `CTest`
- remove or relocate checked-in AOT-only fixture projects, golden directories, and generated benchmark artifacts from active main paths
- update main docs so test ordering and CLI docs no longer describe AOT as part of supported coverage

Generated AOT artifacts that exist only as historical snapshots may be deleted rather than preserved in active main fixtures.

## Migration Order

1. Write design and plan docs.
2. Contract main public/build/test surfaces.
3. Delete main CLI and Rust binding AOT behavior.
4. Move residual AOT implementation and direct tests into `zr_vm_aot/`.
5. Remove stale AOT docs/assets from active main paths.
6. Validate main build/test flows without any AOT participation.

## Acceptance Criteria

The split is complete when all of the following are true:

- top-level main CMake has no `ZR_VM_BUILD_AOT`
- main CLI help and parser reject no AOT modes or flags because those options no longer exist
- main parser public header exports no `WriteAot*`
- main library public umbrella header exports no AOT runtime header
- main Rust binding ABI and safe crates expose no AOT manifest fields
- default main `ctest` registration contains no AOT-specific targets or AOT suite branches
- remaining AOT code and direct AOT tests live only under `zr_vm_aot/`

