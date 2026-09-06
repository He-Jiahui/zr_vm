---
related_code:
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_protocol_inventory.js
  - tests/language_server/wasm_capability_inventory.js
  - tests/language_server/lsp_native_inventory_contract.js
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/CMakeLists.txt
implementation_files: []
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server
  - language_server_stdio_protocol_conformance
  - language_server_stdio_protocol_inventory
  - language_server_wasm_capability_inventory
  - language_server_wasm_capability_inventory_regression
doc_type: acceptance-record
---

# Plan 00 Task 1: Current GCC Replay

## Scope

This record captures a fresh current-checkout GCC Debug build and the resulting
LSP replay after the WASM inventory commits. It is a baseline update attempt,
not a full Plan 00 acceptance: the checkout still contains active semantic and
runtime overlays owned by other sessions.

## Environment

- WSL Ubuntu 22.04, GCC 11.4.0, CMake/Ninja, shared libraries on and static
  libraries off.
- Build directory: `.codex/build-lsp-opt-gcc`.
- JavaScript checks use the private Node `v22.13.1` Linux runtime because the
  extension's TypeScript dependency is 5.9; CTest is configured with the same
  `ZR_VM_NODE_EXECUTABLE`.
- The worktree was nonempty before this replay; unrelated paths were not staged
  or changed by the replay.

## Verification

```text
cmake -S /mnt/e/Git/zr_vm -B /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIB=ON -DBUILD_STATIC_LIB=OFF \
  -DZR_VM_NODE_EXECUTABLE=/home/hejiahui/.codex-tools/node-22.13.1/node-v22.13.1-linux-x64/bin/node
cmake --build /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --target \
  zr_vm_language_server_stdio zr_vm_language_server_lsp_capability_inventory_probe \
  zr_vm_language_server_lsp_interface_test zr_vm_language_server_lsp_project_features_test \
  zr_vm_language_server_lsp_advanced_editor_features_test --parallel 8
  [841/841] linked successfully

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --output-on-failure \
  -R "language_server_(stdio_protocol_inventory|wasm_capability_inventory|wasm_capability_inventory_regression)$"
  100% tests passed, 3 tests passed

<Node 22> stdio_protocol_conformance.js <current GCC stdio server>
  30/30 cases passed, exit 0

ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --output-on-failure -R "^language_server$"
  failed at zr_vm_language_server_symbol_table_test: exit code No such file or directory
```

The extension gates also pass: `npm --prefix zr_vm_language_server_extension
run test:unit` reports 41/41, `npx tsc -p . --noEmit` exits 0, and
`git diff --check` reports no whitespace error.

## Focused Semantic Failures

The three built semantic executables were run directly to preserve the current
failure boundary. `zr_vm_language_server_lsp_interface_test` exits 1 with 8
failures; `zr_vm_language_server_lsp_project_features_test` exits 1 with 14;
and `zr_vm_language_server_lsp_advanced_editor_features_test` exits 1 with 1.
The exact case names are listed in
[WASM worker wiring acceptance](../../../../tests/acceptance/2026-09-06-lsp-wasm-worker-wiring.md).
They remain assigned to the active semantic overlay and Plan 00 Task 1/Plan 03
consumer work; no failure was waived by this replay.

## Decision

The focused compiler build, protocol replay, inventory CTest, extension unit and
noEmit gates are accepted as current evidence. The Plan 00 Task 1 full-build and
aggregate LSP gate remains open because the aggregate suite lacks the first
required executable, and the semantic focused failures remain unresolved.
