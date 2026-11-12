---
related_code:
  - tests/cmake/zr_vm_register_executable_suite.cmake
  - .github/workflows/pr-regression-validation.yml
  - .github/workflows/interpreter-gates.yml
  - tests/CMakeLists.txt
  - tests/cmake/run_executable_suite.cmake
implementation_files:
  - .github/workflows/pr-regression-validation.yml
plan_sources:
  - user: 2026-11-13 review GitHub PRs and merge eligible changes
tests:
  - tests/language_server/test_incremental_parser.c
  - zr_vm_rust_binding/rust/zr_vm_rust_binding/tests/native_registration.rs
  - tests/language_server/test_lsp_parser_diagnostics.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/parser/test_fused_add_operand_packing.c
  - tests/parser/test_aot_llvm_specialized_cast_fusion.c
  - tests/rust_binding/test_rust_binding_api.c
doc_type: testing-guide
---

# PR integration regression gates

The supplemental workflow validates the combination of reviewed parser, core,
AOT, language-server, and Rust fixes on pull requests to main and pushes to
main. It complements the interpreter sanitizer/performance workflow. It does
not change the enabled state of the separate multi-platform CMake workflow.

Linux jobs configure independent GCC and Clang Debug build trees outside the
checkout and build the registered native test dependencies. Their selected
CTest suites exercise changed parser diagnostics, source ownership, fused
instruction packing, worker errors, batch strides, AOT lowering, incremental
parsing, editor ranges, navigation allocation, and native binding boundaries.
The suite inventory preflight requires the named regression entries before
CTest runs, and CTest rejects an empty selection. Cargo tests use the CMake
target's generated native link environment and the committed lockfile.

The Windows job provides MSVC compatibility evidence for the touched native
boundaries. Extension checks install dependencies from package-lock.json and
compile/test the browser bridge. These checks do not rebuild an Emscripten
WASM binary or prove browser execution of generated WASM assets.

Each job records the checked-out commit and tree and retains failure logs.
The merge process checks the latest PR head and CI result and verifies that
the actual main merge tree equals the tested integration tree. A changed
head, changed target, merge conflict, absent required suite, or failed check
requires review and verification of the resulting combination.

Regression programs that belong to executable aggregate suites are validated
through the registered suite command, rather than treated as independent CTest
names from inactive CMake fragments. The registration preflight requires the
compile-time program in `language_pipeline` and the incremental-parser, initial
diagnostics, source-contract, interface, advanced-editor, and inlay-facts
programs in `language_server`. It also verifies that these programs are built
and executable. Both aggregate suites remain in the executed CTest selection.

The pipeline aggregate passes a generated `SUITE_MANIFEST` file, while the
language-server aggregate supplies `EXECUTABLES` directly. Preflight accepts
exactly one form, confines manifest reads to the build tree, and extracts the
full executable list emitted by the CMake generator. The runner uses that full
list when no tier filter is requested; smoke/core/stress fields are not used as
substitutes for the full inventory.

The incremental parser's isTokenEquivalent field expresses whether the
existing AST may be reused. A comment-only change over an earlier unparsed edit
therefore retains a false reuse flag and a dirty document. The regression checks
both dirty staging and a full reparse of the latest values; textual token
equivalence does not make the older AST current.

The native binding registration fixture declares T and U in actual
parameter types and passes an integer and a string. A fully concrete signature
with unused generic declarations cannot provide inference evidence for those
parameters. The fixture retains both generic descriptors, constraints, callback
result, and result assertion while exercising successful inference.

Native generic constraints use the supported builtin equatability protocol.
Primitive type names are not protocol or nominal prototype constraints.

Native fixture modules set an explicit version of 1.0.0 so compile-time import
metadata and runtime callable metadata agree. The generic registration test
checks generic inference and execution independently of omitted-version defaults.
