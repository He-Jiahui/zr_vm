# Native Library Build

Without `ZR_VM_RUST_BINDING_LIB_DIR`, the sys crate builds the existing ZrVM
CMake project from the enclosing source tree. CMake and a native C toolchain
must be installed, and the source tree must include its third-party submodules.
All generated files stay below Cargo's `OUT_DIR`. The CMake build targets only
`zr_vm_rust_binding_shared` and its native dependencies; it never invokes Cargo.

`cargo test` and `cargo run` receive the generated native library search paths.
An application distributed outside Cargo must package these shared libraries
and their dependencies using its normal native library deployment process.

Set `ZR_VM_RUST_BINDING_LIB_DIR` to an existing import library directory to use
a separately built native SDK. Its runtime library directory must be available
to the operating system's loader. This explicit mode also supports cross
compilation; automatic source builds require matching host and target triples.

`CMAKE` can select the CMake executable. CMake's usual generator environment
variables select the native build system. The build uses one native worker so
it stays within the parent Cargo build slot.
