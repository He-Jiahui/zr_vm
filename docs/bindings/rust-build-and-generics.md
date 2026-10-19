---
related_code:
  - zr_vm_rust_binding/CMakeLists.txt
  - zr_vm_rust_binding/src/zr_vm_rust_binding/native.c
  - zr_vm_rust_binding/rust/Cargo.toml
  - zr_vm_rust_binding/rust/Cargo.lock
  - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/build.rs
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/tests/native_registration.rs
implementation_files:
  - zr_vm_rust_binding/CMakeLists.txt
  - zr_vm_rust_binding/src/zr_vm_rust_binding/native.c
  - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/build.rs
plan_sources:
  - user: 2026-10-12 Rust binding audit fixes for generic descriptor stride and portable locked Cargo builds
tests:
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/tests/native_registration.rs
doc_type: module-detail
---

# Rust binding builds and generic descriptors

The Rust binding workspace lives in `zr_vm_rust_binding/rust`. Its `Cargo.lock` is committed so `cargo check --workspace --locked` and `cargo test --workspace --locked` resolve the same dependency versions on each machine. Cargo's normal target directory and cache apply to standalone workspace commands.

When the root CMake project builds the binding, Cargo products go under `${CMAKE_BINARY_DIR}/cargo/target`. This keeps ordinary builds portable to Linux and macOS and avoids requiring a drive-specific output root. Managed validation can opt into strict physical output checks with `ZR_VM_CARGO_TARGET_ROOT` and optionally `ZR_VM_CARGO_OUTPUT_ROOT`. The target root must resolve to exactly `D:/cargo-targets`, `E:/cargo-targets`, or `F:/cargo-targets`; an output root must resolve below it. The CMake and Cargo build scripts reject aliases and symlinks when these variables are set.

Native generic parameter descriptors cross the C ABI as contiguous `ZrLibGenericParameterDescriptor` elements. Each descriptor owns its copied name, documentation, and constraint-name array until the registered module is released. Do not place per-element ownership metadata around these ABI descriptors: the VM indexes the array using the public descriptor size, so a wrapper with a larger stride corrupts every element after the first.

The native registration integration test declares two generic parameters with separate constraints, imports the module, and invokes its function. Import materializes the generic metadata, which exercises the descriptor stride for both entries.

## Validation

Run from `zr_vm_rust_binding/rust`:

```sh
cargo metadata --locked --format-version 1
cargo check --workspace --locked
cargo test --workspace --locked
```

The native registration test builds through the sys crate's CMake build script when no prebuilt native library directory is supplied. In managed environments, set `CARGO_TARGET_DIR` below the configured physical cargo-targets root before invoking Cargo.
