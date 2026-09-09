---
related_code:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_cross_snapshot_external_reference_cases.h
  - tests/language_server/test_lsp_analysis_provider_generation_cases.h
implementation_files:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_cross_snapshot_external_reference_cases.h
  - tests/language_server/test_lsp_analysis_provider_generation_cases.h
plan_sources:
  - docs/plans/lsp/optimize/2026-09-08-plan01-task06-sub11-parity-fixture-ownership.md
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_analysis_provider_generation_cases.h
doc_type: module-detail
---

# LSP Semantic Query Parity Harness

The zr_vm_language_server_semantic_query_parity_test executable checks repeated
parser queries and LSP consumers against source, binary and native snapshots.
It also verifies canonical identities after legacy analyzer state is detached,
and rejects missing, stale or mismatched external declaration identities.

## Ownership

SemanticQuery_TypeAt copies an inferred type into caller storage. Each query
result must be passed to ZrParser_InferredType_Free, including partial-failure
paths. This matters for native signatures with owned element-type arrays.
Canonical query views and referenced GC strings retain their existing ownership.

LspSemanticQuery_BuildHover allocates an SZrLspHover and its contents array.
The fixture frees the array and structure before destroying the LSP context;
the strings are GC-managed. Temporary analyzer pointers are restored before
the context is destroyed.

The implementation and cross-snapshot location checks compare non-null URIs
using the public ZrCore_String_Equal API. The private LSP string helper is not part of the Windows
shared-library export surface and must not be called by the test executable.

## Validation

Build the parity target in each compiler configuration, then run its executable
to completion. Functional PASS messages alone are insufficient: sanitizer
teardown and the process exit code are part of this harness's acceptance.
Valgrind should run with full leak reporting and an error exit code.

The temporary binary import source graph is a lower-level ownership dependency.
See [IO source lifetime](../module-system/io-source-lifetime.md) for its recursive
destructor contract. Sub10 covers the compile-time import consumer's releases.

The provider-generation cases also verify that LSP reanalysis does not reuse a
same-AST whole-document or scoped cache after `ProviderChanged`. The rebuilt
parser context must carry the current nonzero generation into both binary and
native external references.
