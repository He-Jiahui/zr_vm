---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_callable_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_callable_identity.h
  - tests/parser/test_ssa_source_callable_identity.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_callable_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_callable_identity.h
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - .codex/plans/20261004-source-aot-handoff-design-2005.md
tests:
  - tests/parser/test_ssa_source_callable_identity.c
  - docs/parser-and-semantics/ssa-source-callable-identity.md
doc_type: acceptance
status: accepted-finite-source-callable-identity
---

# Source Child Callable Identity Acceptance

## Scope and baseline

This finite gate checks that ordinary `ZrParser_Source_Compile` preserves a
source declaration/canonical-signature proof on the actual returned child
`SZrFunction`, after the temporary AST and semantic context are released.
The new record is numeric compiler provenance. Its context-local IDs are not
post-compile lookup handles. The gate does not publish a retained canonical
graph, metadata token, source frame, serialized witness, or AOT artifact.

The existing compiler already publishes child `callableReturnType` for the
explicit i64 positive declarations. The intended pre-fix feature RED is
absence of the additional identity witness after that existing metadata
precondition succeeds. An ordinary source-compile failure, missing existing
return metadata, or sanitizer fault would instead be a prerequisite failure.
The actual RED v2 below establishes that feature gap.

The finite producer admits only an explicit i64 ordinary function declaration
with exactly one return statement whose expression is an integer literal,
without parameters/varargs/generics/decorators/async/yield, receiver, effects,
native/import behavior, captures, child functions, finally, or cleanup.
Actual `Expression_Compile` semantic IR supplies the value evidence; a narrow
return hook constructs the real RETURN while the expression's original inner
isolation is alive, validates the actual IR, and publishes numeric provenance.
Binary expressions, multiple statements, branches, loops, and general pure
CFG admission remain open.

## Test inventory

The final `ssa_source_callable_identity` fixture has eleven Unity cases and
thirteen public source-compile calls, including one expected malformed-source
failure. It compiles source through the public API and inspects the returned
function tree without executing generated code.

| Coverage | Cases and assertions |
| --- | --- |
| Positive publication | Explicit zero-argument i64 child; ordinary child return metadata precedes the new witness assertion |
| Lifetime and copy | Keep `first` rooted while compiling `second`; compare all first-record fields after the second compile and check both witnesses |
| Declaration/signature distinction | Two named children in one source retain distinct symbol IDs and declaration lines 1/2, but share the same canonical signature hash |
| Unsupported return type | Explicit bool return receives no certified identity |
| Unsupported parameters | Parameterized i64 function receives no certified identity |
| Unsupported capture/child behavior | Nested closure tree receives no certified identity |
| Return proof guards | Implicit return and unannotated i64 return receive no certified identity |
| Exact syntax scope | Binary integer return and multiple-statement body receive no certified identity |
| Compile failure and retry | Malformed source returns NULL; valid retry preserves existing i64 child return metadata |

The original immutable RED used eight cases. The two-child positive and two
additional syntax guards were added afterward and await the final GREEN;
they do not change the recorded historical RED counts or fixture hash.

The schema assertions require version 1, nonzero source symbol/type IDs and
canonical structural signature hash, primitive INT64 return, zero parameters,
receiver/effect flags, and a populated numeric declaration range. These are
retained values; the tests never resolve IDs after `Source_Compile` returns.
The malformed-source retry is not an OOM or late-failure rollback test.

## Frozen RED evidence

The immutable actual RED receipt is
`E:\cargo-targets\zr_vm\reports\ssa-20261005-01a0fe2b\callable-identity-red-v2\receipt.json`.
Its SHA-256 is
`c2dedea41e6cb39002c5abaebe9d029060c4f0dcffd33edce004a021b334d815`.
The frozen fixture SHA-256 is
`60b96105f6d6413bec08a1b878fe1bc8a0434863b395ed8857180db8fc3f0027`;
the CTest log SHA-256 is
`e98edab72765aec1e6f2dbac125085068a03dcf97f1bd160b908b412c3a299c1`.
The receipt and current frozen fixture hashes were independently read and
checked while documenting this evidence.

Configure and build exited zero. CTest exited 8 because exactly the two
positive cases failed at the missing-witness feature assertion, after the
ordinary compilation and existing child return metadata preconditions.
All six guards passed; eight cases ran, zero were ignored, and no UBSan
diagnostic was reported. This RED establishes the intended feature gap and
is not a passing acceptance result.

The earlier immutable RED v1 is not accepted feature evidence. The fixture
passed the global object rather than the actual state to
`ZrCore_GarbageCollector_IgnoreObject`, causing a UBSan prerequisite failure
before the intended feature assertion. Root corrected that one fixture call
and reran V2. Preserve both historical receipts; V2 is the actual feature RED.

## GREEN tooling and evidence

Pending Root's current-source implementation and native validation. The
reused direct build directory is
`E:\cargo-targets\zr_vm\build\ssa-20261004-01a0fe2b\metadata-guards-direct-v2`.
The planned route is Windows x64 clang-cl 19 Debug UBSan, directly compiling
the current checkout without source snapshots/copies. Native commands and
Git operations belong to Root.

The final evidence must name the actual producer translation unit, configure,
build, and CTest command/exit status, focused and companion group counts,
zero failures/ignored cases, sanitizer output, and the source/test/CMake input
pins checked across the run. A build-only success is insufficient. No GREEN
receipt, hash, or passing result is claimed at this stage.

Both ordinary top-level source-test CMake and the direct driver register the
fixture. Only a route with a recorded completed test run contributes to
acceptance; registration of the ordinary route does not prove it passed.

## Acceptance decision and limits

Pending native evidence. The new schema and documented admission contract
are reviewable, but this record does not yet accept the producer. Final
acceptance must be limited to the real direct RED/GREEN scope and keep any
unrun route explicit.

Ordinary top-level CMake, WSL/Linux, native 32-bit, ASan, LLVM/AOT backends,
full parser suite, and the full 47-item SSA plan are not claimed. Neither
successful retry nor lifetime inspection proves OOM injection, module cache
transactionality, late-stage publication rollback, or general callable
admission. No source frame, token, retained graph, or same-source AOT
acceptance follows from this record.

Validation is restricted to local parser/Core compilation and tests. It
requires no network, FFI, provider, capability, hotpatch, external-service
activity, or security-boundary probes.
