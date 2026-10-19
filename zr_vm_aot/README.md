# `zr_vm_aot`

This directory contains both an active AOT backend and historical reference files.

| Path | Role |
| --- | --- |
| `zr_vm_parser/src/zr_vm_parser/backend_aot/` | Active backend sources, compiled into the parser by `zr_vm_parser/CMakeLists.txt` and used directly by SSA/AOT tests. |
| `tests/parser/` | AOT test sources registered by the main test build. |
| `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c` and its header | Historical runtime reference used as a source-text oracle by AOT contract tests. The current runtime is in the top-level `zr_vm_library`. |
| `zr_vm_common/include/zr_vm_common/zr_aot_abi.h` | Historical ABI reference; current production ABI is in the top-level `zr_vm_common`. |

The directory is therefore part of the current build and CTest dependency graph.
The historical files are not interchangeable with current implementations and
must not be deleted merely because they share file names. Relocating the active
backend requires updating parser, WASM, SSA and AOT test source/include paths
together; active validation tasks currently depend on these paths.
