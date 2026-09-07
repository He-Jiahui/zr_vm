---
related_code:
  - zr_vm_language_server/stdio/stdio_initialize.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
implementation_files:
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/04-editor-feature-correctness.md
  - docs/plans/lsp/optimize/05-native-web-capability-parity.md
tests:
  - tests/language_server/test_stdio_optional_capability_allocations.c
  - tests/language_server/stdio_optional_capabilities_smoke.js
  - tests/language_server/stdio_capability_snapshot.js
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_protocol_inventory.js
  - tests/language_server/stdio_smoke.js
doc_type: module-detail
---

# Optional LSP Capability Negotiation

## Protocol Contract

Native stdio uses LSP 3.17 as its baseline. Inline completion and multi-range
formatting are independent optional 3.18 facilities. Neither is enabled merely
because the server implements its request handler.

| Client capability | Initialize result | Optional request |
| --- | --- | --- |
| No valid inline completion capability | `inlineCompletionProvider` absent | `textDocument/inlineCompletion` returns `-32601` |
| `textDocument.inlineCompletion` object | `inlineCompletionProvider: true` | Handler enabled |
| No valid `rangesSupport: true` | `documentRangeFormattingProvider: true` | `textDocument/rangesFormatting` returns `-32601` |
| `textDocument.rangeFormatting.rangesSupport: true` | `documentRangeFormattingProvider: {rangesSupport: true}` | Handler enabled |

The known `dynamicRegistration` field must be absent or boolean in both client
objects. `false` permits static publication; it does not disable the provider.
Unknown extension fields are tolerated. Missing, null, array, scalar or
ill-typed known fields do not enable an optional capability. Ordinary completion
and single-range formatting remain available independently of these flags.

The installed official `vscode-languageserver-protocol` 3.17.5 declarations
document both additions as 3.18 proposed: `protocol.inlineCompletion.d.ts`
defines the client object and `protocol.d.ts` defines
`DocumentRangeFormattingClientCapabilities` / `DocumentRangeFormattingOptions`.
There is no `documentRangesFormattingProvider` capability. The spelling in the
original August 22 optimize review is a historical error, preserved with an
explicit correction in the execution record.

## Lifetime And Ownership

`SZrStdioServer` owns two boolean negotiation flags. Zero-initialized server
allocation starts with both disabled. Successful initialize derives their
values from the request and uses the same values for publication and dispatch.
The adapter retains no pointers into the initialize JSON. A new server owns new
flags; repeated initialize is rejected by the lifecycle state machine before it
can alter the established negotiation.

Publishing an optional capability also requires successful JSON allocation.
Failed range object creation, field creation or parent attachment releases the
unattached object, disables optional ranges and attempts the ordinary boolean
range provider. Failed inline publication disables its flag. Persistent memory
exhaustion can prevent the ordinary fallback too; it must not leave an optional
dispatch enabled without publication. General initialize failure handling
remains part of the protocol/lifecycle acceptance gate.

These flags describe protocol support, not semantic or document identity.
Existing request snapshot, cancellation and edit checks still apply to enabled
handlers. Registry minimum-version metadata remains distinct from per-client
negotiation. Shared native/Web publication generation is a Plan 05 obligation.

## Exactness And Verification

The focused fixture compares the complete initialize capability object for
baseline, independent optional combinations and malformed inputs. Request
checks assert complete JSON-RPC envelopes: exact `-32601` when disabled, the
exact `return ` replacement and range when inline completion is enabled, and
two precise nonoverlapping formatting edits when ranges are enabled. Ordinary
completion preserves the two visible function labels and an exact text edit;
single-range formatting preserves its exact edit. Repeated initialize is
rejected and the original request behavior remains unchanged.

`stdio_capability_snapshot.js` also requires the semantic-token legend's exact
`tokenModifiers: ['declaration']` value. The 21 initialize negotiation cases
compare parsed responses against this object; four additional cases exercise
requests under each optional-provider combination. This protocol assertion owns
the wire legend contract. The canonical semantic-token source check only guards
parser query usage and forbidden reconstruction calls. It does not pin the
cJSON allocation helper used to construct the legend.

Every fixture owns its child process, completes shutdown/exit, checks exit zero
and empty stderr, and terminates on failure. The two source documents belong to
that process only. No source, binary or native provider identity is fabricated.

The C allocation fixture calls the production publication function with cJSON
allocation hooks. A successful control discovers the six allocations owned by
the two optional outputs, avoiding fixed allocation indices tied to unrelated
providers. Each site receives one transient failure and one persistent failure.
All 12 fault cases assert flag/publication consistency and zero outstanding
cJSON allocations; transient range failure also retains ordinary formatting.
The fixture restores global hooks and releases any detected leak after counting
it as a failure, so subsequent cases can run without hiding that failure.

This leaf validates protocol negotiation and specific retained behavior. It
does not establish parser-aware formatting, formatting-option coverage,
idempotence, semantic equivalence or grammar-based inline completion. Those
remain Plan 04 acceptance requirements. Evidence is in
[Plan 00 Task 3 Sub02](../plans/lsp/optimize/2026-09-05-plan00-task03-sub02-optional-capabilities.md).
