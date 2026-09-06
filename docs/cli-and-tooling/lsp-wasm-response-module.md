---
related_code:
  - zr_vm_language_server/wasm/wasm_response.h
  - zr_vm_language_server/wasm/wasm_response.c
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - tests/language_server/test_wasm_response.c
  - tests/language_server/test_wasm_exports.c
  - tests/cmake/zr_vm_lsp_wasm_response_tests.cmake
implementation_files:
  - zr_vm_language_server/wasm/wasm_response.h
  - zr_vm_language_server/wasm/wasm_response.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/05-native-web-capability-parity.md
tests:
  - tests/language_server/test_wasm_response.c
  - tests/language_server/test_wasm_exports.c
  - tests/cmake/zr_vm_lsp_wasm_response_tests.cmake
doc_type: module-detail
---

# WASM response ownership and error envelope

The WASM adapter exposes two C helpers. `ZrLanguageServer_Wasm_ErrorResponse`
receives the caller-selected JSON-RPC code and always emits `success:false`,
`code`, and a string `error`. It does not infer a code from human-readable
messages. `ZrLanguageServer_Wasm_SuccessResponse` consumes its `cJSON *data`
argument; an absent data tree is a serialization failure, while an explicit
`cJSON_CreateNull()` is a valid LSP no-result. Every failed cJSON allocation
deletes the partial tree and returns no borrowed pointer.

`wasm_exports.cpp` owns the core result lifetime and passes explicit shared
codes at the export boundary. A legacy core hover call that reports no match
is serialized as a successful `data:null`, matching the native stdio adapter.
Workspace diagnostics abort the complete response when URI enumeration,
diagnostic computation, result-id construction, or any report serialization
fails; a partial report array is never published as success.

The response unit test injects each envelope allocation failure for four error
codes and null, empty-array, and object data. The export harness additionally
opens an empty document, checks a real core hover success envelope, and injects
every observed workspace serialization allocation. The worker probe separately
checks the `data:null` no-result path. These tests do not yet prove a
transport-neutral core status that distinguishes no-hover from an internal
failure; that remains a Plan 01/02 gate.
