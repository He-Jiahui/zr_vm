---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/lsp_capability_registry.h
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - zr_vm_language_server/stdio/stdio_initialize.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server/stdio/stdio_navigation.c
  - zr_vm_language_server/stdio/stdio_editor_features.c
  - zr_vm_language_server/stdio/stdio_position_encoding.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - zr_vm_language_server/stdio/stdio_initialize.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/05-native-web-capability-parity.md
tests:
  - tests/language_server/test_lsp_capability_registry.c
  - tests/language_server/stdio_protocol_inventory.js
  - tests/language_server/stdio_resolve_capabilities_smoke.js
  - tests/language_server/stdio_smoke.js
  - zr_vm_language_server_extension/test/serverCapabilities.test.js
doc_type: module-detail
---

# LSP Resolve Capability Contract

## Base Requests And Deferred Resolution

A provider's base runtime support and its deferred resolution support are
separate contracts. Publishing an initial list does not imply that a runtime
can enrich or revalidate an item later. Registry resolve metadata must describe
the runtime that implements that later operation; an identity copy is not a
resolver.

| Provider | Initial response | Native resolve | Web resolve |
| --- | --- | --- | --- |
| Workspace symbols | Symbol and complete location | absent | absent |
| Inlay hints | Position and complete label | absent | absent |
| Document links | Range and target URI | absent | absent |
| Code lenses | Range and command | absent | absent |
| Completion | Completion items | material resolver | absent |
| Code actions | Actions with available edits | snapshot revalidation | absent |

The four identity-only providers remain available through their initial
requests. Their resolve methods are not registered. Native requests to a
withdrawn method return JSON-RPC `-32601`; Web uses the language-server
connection's ordinary unregistered-method behavior.

## Registry Invariants

`SZrLspCapabilityDescriptor` describes immutable process-lifetime metadata.
Pointers returned by registry lookup are borrowed static descriptors, not
semantic snapshot facts. Resolve runtime coverage must be a subset of base
runtime coverage. A descriptor without resolve has an empty resolve mask and
`NONE` behavior. A published resolver has nonempty runtime coverage and
`MATERIAL` behavior; `IDENTITY` is rejected.

Native completion and code-action resolve are not evidence of Web support.
The runtime-aware predicate used by native initialize evaluates the descriptor
and its resolve mask. The worker advertises no deferred resolver until the
corresponding bridge/export and actual revalidation or enrichment exist.

## Semantic And Edit Safety

Capability metadata does not create symbols, types, references or diagnostics.
Initial payloads continue to come from the existing compiler queries and
protocol serializers. No text-based semantic inference is introduced when
resolve is withdrawn.

Native code actions retain the captured document and semantic snapshot identity
in their data. Resolve validates that identity before returning edits. A stale
action has its edit removed and receives a `disabled.reason` explaining that
the document changed. This is a resolved, disabled action rather than a JSON-RPC
error. The Web worker must not claim this resolver by copying incoming action
JSON.

## Verification Boundary

Registry tests cover base-provider retention, runtime-specific material resolve,
invalid masks and identity publication rejection. Protocol inventory inspects
the actual native initialize response and checks exact MethodNotFound envelopes
for removed methods, using the shared stdio client. Stdio smoke checks complete
initial link/lens/hint/symbol data and preserves material code-action edit and
stale-snapshot scenarios. The focused resolve smoke reaches those assertions
independently of the broad semantic smoke's earlier scenarios.

The extension test executes the transpiled production worker's initialize and
request registration callbacks with a controlled connection and bridge. It
verifies publication, handler absence and unchanged initial payload forwarding;
it is not a replacement for the later actual WASM/browser parity gate.

Other capability overclaims, compiler relation completeness and full native/Web
manifest generation remain separate optimize tasks.
