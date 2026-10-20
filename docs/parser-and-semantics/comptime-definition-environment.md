---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_call.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_callable.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_execution_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_execution_scope.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_decorator_execution.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_import.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - tests/compileTime/test_comptime_definition_environment.c
  - tests/parser/test_compile_tool_project_import.c
  - tests/compileTime/test_comptime_frame_lifetime.c
  - tests/harness/path_support.c
  - tests/cmake/macro-foundation-tests.cmake
  - lua/rust/tests/ui/hygiene/local_inner_macros.rs
  - lua/src/lparser.c
  - lua/cpython/Python/bytecodes.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_call.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_callable.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_execution_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_decorator_execution.c
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/compileTime/test_comptime_definition_environment.c
  - tests/parser/test_compile_tool_project_import.c
  - tests/compileTime/test_comptime_frame_lifetime.c
doc_type: module-detail
---

# Comptime Definition Environment

This M1 support contract separates the environment of a function declaration
from the environment of a supplied argument. Imported source functions belong
to the compiler-owned provider loaded through a locked build dependency.
Their bodies and omitted defaults resolve names in that provider. Supplied
argument expressions resolve in the caller, including explicit function values.
An imported function does not inherit the caller's local variables. Blocks
inside a function retain their ordinary lexical parents.

## Evidence and Intended Boundary

The repository's Rust `local_inner_macros.rs` fixture exercises definition-side
helper resolution. Lua's `singlevaraux` searches lexical function parents before
the environment, while CPython's `LOAD_GLOBAL` uses its globals and builtins.
These references support an explicit definition environment; they do not by
themselves prove ZR source or binary behavior.

The pre-repair source evaluator entered the imported owner before evaluating
arguments and gave the callee frame the caller as its parent. The helper
lookup also fell through from a provider miss to the consumer's function
table. These are separate boundaries and require actual regression evidence
before a production repair. Changing only the active-owner pointer would leave
the wrong import bindings visible; scope entry and cleanup must use the
existing CompileTool execution-scope operations.

The authorized four-path repair extracts source invocation and the existing
three binary runtime-projection helpers into private `compile_time_call.c/.h`.
The executor retains its existing decorator argument-evaluation entry point;
both call paths share a pure supplied-argument selector. The interpreted call
uses an isolated callee frame. Supplied ASTs evaluate in the caller; omitted
defaults temporarily enter the owner and can read earlier callee parameters.
The existing owner-scoped cache-key initialization, body/cache operations,
call-depth checks and cleanup remain in their original sequence relative to
their corresponding work. Provider function lookup stops on a provider miss.
Shared block-parent traversal and exported-only external lookup stay unchanged.
The decorator and execution-scope modules are outside this production lease.

## Function Values and Provider Lifetime

The private `compile_time_callable.c` module owns function-value materialization
and recognition. Source functions produce native pointers to the existing
compiler-owned function records. Runtime projections retain their existing
module import and protected/public export lookup. No record or module is copied.

A private provider helper can be bound to a local, returned from an exported
function and passed explicitly to another function. Recognition checks pointer
membership in the current compiler's real function and imported-module arrays,
then in each real `compileToolProviderParent` ancestor's arrays. The supplied
pointer is compared without dereferencing it; only a registered record is
returned. Invocation uses that record's existing owner scope. External name
lookup still enforces export visibility, and native runtime closures still
receive the existing compile-time effect rejection.

These native pointers borrow compiler/module lifetime. GC does not move or free
those native records, while module rollback and compiler destruction invalidate
them. A child compiler can borrow a parent's actual imported provider; it must
be destroyed before that parent. Recognition neither extends lifetime nor adds
allocations, roots, cache operations or resource charges.

## Real Locked-Source Fixture

`zr_vm_comptime_definition_environment_test` is registered as CTest
`macro_comptime_definition_environment`, with the existing 30-second foundation
timeout. It builds a synthetic provider source and `.zrm` archive in the reused
configured build output. Both source and archive receive their actual content
hashes. A real manifest and dependency lock identify `@derive`; canonicalization
and BuildFacts load the provider through the normal project import path.
The consumer probe uses an actual non-pruning `comptime { let probe = ...; }`
block. Its initializer is read from the parsed block and evaluated through the public evaluator after
BuildFacts. It is not a runtime use of a CompileTool binding.

The fixture verifies the open artifact, compiler-owned module AST, source name,
function owner records, export flags and absence of runtime projections before
evaluation. It does not insert synthetic bindings, owner pointers or hygiene
identities into the compiler. The consumer's same-named helper returns 22 and
the provider's private helper returns 11. A missing provider function must not
capture the consumer's function returning 33. Supplied positional and named
arguments must produce 22; an omitted default must produce 11.

Companion cases pass a consumer callback directly and through a local variable,
reject accidental capture of a nested caller local, preserve provider block
lookup, repeat scalar/callback behavior after full GC, and reject external
access to the provider's private helper. Rejection diagnostics are checked
against the real provider identifier's source and all six position fields.
Each case starts with a fresh compiler and cold cache. Scope aliases, bindings,
active owner, call depth and GC root count must return to their previous values.
External private access is rejected by exported-only module lookup; the
current fallback diagnostic names the actual consumer alias `derive`, with its
real source range. That access control is distinct from internal private helper
lookup and does not promise a new private-member diagnostic in this repair.

## Ownership and Validation Status

Unity teardown owns the compiler, consumer AST, project, runtime, archive-read
buffer and created fixture paths, including assertion-failure exits. The test
asserts that its source and archive paths are absent before creating either.
Only files created by that test are removed. The cleanup footer reports
measured file bytes and errors; the manifest path supplied to `Project_New` is
a logical identity and is not written as a source-tree copy.

The first native run62008 naturally exited1: build0,12 prerequisite failures
at consumer parsing, no behavior credit. Peer inspection identified the
removed legacy `comptime let` syntax; the fixture now uses the supported
module-level comptime block. All24 created fixture files were removed,
measured30348 bytes, with zero cleanup errors. Native17814 then naturally
exited1/build0,12cases7PASS5FAIL: four intended failures (consumer function and
local capture, supplied positional/named22 becoming11), plus one false fixture
expectation that the private-access diagnostic names `ownHelper`. Peer and Root
grounded the existing rejection in exported-only lookup and alias fallback;
the corrected fixture asserts `Unknown compile-time identifier: derive` and
its actual range. All24 r2 fixture files were removed, measured30348 bytes/errors0.
Fresh clean RED native63325 naturally exited1/build0/test4:12cases8PASS4FAIL,
only the four intended call-boundary assertions fail. All identity, cleanup,
cold-cache, default, callback, lexical block, full-GC and exact private-access
diagnostic controls pass. The test log hash is90C33D8F; fixture0ED151EE and
CMake85708630 are frozen. Root confirms all24 r3 files30348 measured bytes
removed/errors0 and both source/archive absent. Independent peer accepts the
actual clean RED. The frozen four-path candidate passed the complete first
GREEN native42047: build0/test0,12PASS/0FAIL/0ignored. The test log hash is
BECAD5E8; all24 fixture files30348 measured bytes were removed/errors0. Root's
read-only inverse reconstructed both complete pre-repair files at their exact
hashes and byte counts. Independent review found no new actionable delta bug.
Broader regression is pending. Six further real-source companions cover
defaults reading earlier parameters, positional/named caller-variable shadowing,
explicit/default scalar cache hits and replay after GC, and a forced full GC
during the second array-argument allocation. The last case scans actual live
array contents before/after collection and checks result77 and restored roots.
The earlier argument is also held by its parameter binding; this case does not
isolate temporary-root deduplication or prove unchanged cumulative charges.
The eighteen-case companion fixture passed native49580: build0/test0,
18PASS/0FAIL/0ignored, logF8B217C0. Cleanup removed36 test-owned files,
58788 measured bytes/errors0. Full49-program foundation and original language
pipeline replay completed as native16417 naturally1: both builds0,
foundation49/49PASS54.08s, original aggregate8 at the same catalogue assertions,
remaining121 programs108PASS/13FAIL. Structured comparison to the typed
aggregate baseline exits0 with zero exit/footer/failed-name/message changes,
zero missing footers and zero unaccounted failures. All four production pins
and the eighteen-case fixture remained unchanged. This establishes the bounded
repair's regression result; the original failing programs remain open.
The provider function-value fixture now has26 cases. Clean RED-r2 native90343
naturally exited1/build0/test5:21PASS and five intended invocation failures.
Independent producer identity and held-GC identity, native-closure/noncallable
rejection, child-parent ownership prerequisites and the original18 cases pass.
Its test log hash isBDEF995D. The bounded two-path repair extracts the old
materializer unchanged and recognizes real registered provider functions.
Root and peer independently reconstruct the complete prior support file in
memory; Root also compares all43651 bytes to its prelease memory image.
No filesystem source copy is created.

First GREEN native88458 naturally exited0/build0/test0:26PASS/0FAIL/0ignored,
test log86EC93D2. Both changed production files and the test retain their frozen
hashes after exit. All52 test-owned fixture files110448 measured bytes were
removed with zero errors. Fresh compilation of both callable and support C
files is recorded. Existing D9025 and unrelated CMake object-path warnings
remain in the build log. This is focused source-provider acceptance; broader
regression and binary-provider function-value parity remain pending.

The existing twelve
aggregate-budget cases and their unchanged-input
48-program foundation replay remain separate evidence. Original pipeline
failures, consumer global/import-alias
isolation and WSL GCC/Clang are still pending. This document does not accept M1
or authorize progression to M2.
