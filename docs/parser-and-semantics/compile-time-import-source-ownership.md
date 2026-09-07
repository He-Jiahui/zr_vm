---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_core/src/zr_vm_core/io_source_free.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
plan_sources:
  - docs/plans/lsp/optimize/2026-09-08-plan01-task06-sub10-compile-time-import-ownership.md
tests:
  - tests/parser/test_compile_time_import_ownership.c
  - tests/parser/test_type_inference.c
  - tests/language_server/test_lsp_semantic_query_parity.c
doc_type: module-detail
---

# Compile-Time Import Source Ownership

`compile_statement_load_imported_compile_time_module` projects imported binary
compile-time declarations into compiler-owned module records. Its input stream,
temporary decoded source graph and retained projection have separate lifetimes.

## Load and Release

The global source loader supplies an `SZrIo`. Binary loading decodes that stream
through `ZrCore_Io_ReadSourceNew` and closes it once through its existing callback.
The completed `SZrIoSource` is temporary and must be released through
`ZrCore_Io_ReadSourceFree` on every subsequent exit:

- Missing module or entry function.
- Failure to allocate the imported module record.
- Rejection while projecting compile-time declarations.
- Successful projection, before publishing the module into the compiler cache.

The decoded graph is not retained in `importedCompileTimeModules`. A cache hit
returns the existing compiler-owned projection and performs no new source read.
Decoder failures before a source is returned retain the core reader's existing
error behavior; this boundary only owns completed source graphs.

## Retained Projection

The variable projection allocates its own declaration record and path-binding
array. Function projections allocate their own inferred types, parameter names,
default-value flags and values. Name strings and object/array/string constants
remain GC-managed values; freeing IO storage does not release those objects.
Consequently the decoded graph can be freed immediately after the projection
helpers finish, while the projected defaults and types remain usable.

If projection fails, existing cleanup releases partial compiler-owned variables,
functions and the module record. Source graph cleanup is also required on that
branch and does not depend on compiler teardown.

See [IO source lifetime](../module-system/io-source-lifetime.md) for the recursive
native-storage destructor and [binary metadata source](binary-metadata-source.md)
for the ordinary analysis-time import path.

## Regression Contract

The focused test uses a real writer artifact and a normal source-loader callback.
It counts IO allocation/free counts and exact byte sizes, checks that every
opened reader closes, and requires the decoded storage to be gone before the
compiler itself is freed. A function projection is inspected after that check
to detect dangling parameter/default references under ASan. A missing projection
name exercises rejection without retaining a module in the cache. Another case
rejects the second function after the first projection has allocated its owned
arrays, exercising source release followed by partial projection cleanup.

Validation results and the historical 5,187-byte type-inference leak are recorded
in the linked subtask record. The separate full-LSP peak-memory and broader
project-test failures are not closed by this ownership contract.
