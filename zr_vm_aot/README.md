# `zr_vm_aot`

This directory is a hard-separated archive of the former AOT implementation.

- It is not part of the main `cmake` build.
- It is not registered in the main `CTest` flow.
- It is not part of the supported CLI, parser, library, or Rust binding surface.
- It is kept only so the retired AOT code, tests, and design notes remain inspectable.

Any future AOT revival must be reintroduced explicitly from this archive instead of relying on dormant hooks in the main repository.
