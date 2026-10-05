---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
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
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_callable_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_callable_identity.h
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - .codex/plans/20261004-source-aot-handoff-design-2005.md
tests:
  - tests/parser/test_ssa_source_callable_identity.c
  - docs/acceptance/ssa-source-callable-identity.md
doc_type: module-detail
status: accepted-finite-source-callable-identity
---

# Source Child Callable Identity

## Purpose and finite scope

Ordinary `ZrParser_Source_Compile` already returns child functions with existing
callable-return metadata. This slice adds a numeric source proof to a narrowly
admitted child declaration, so the returned `SZrFunction` can preserve its
declaration and canonical signature provenance after the temporary AST,
semantic context, and child semantic IR have been released.

The admitted form is an explicit ordinary `FUNCTION_DECLARATION` with an
explicit primitive signed i64 return annotation, whose body contains exactly
one return statement and whose return expression is exactly an integer
literal. It has no parameters, varargs, generic parameters, decorators,
receiver, async/yield behavior, effects, native/import behavior, captures,
child functions, finally, or cleanup behavior. The initial positive source is
`fn answer(): int { return 9; }`. Publication applies to the real child
function, rather than substituting its enclosing script entry.

This is one prerequisite in the existing source-to-AOT plan. Its output is a
pointer-free compiler provenance record. It does not retain the canonical type
graph or establish a global definition identity, metadata token, source frame,
serialized witness, or executable AOT artifact.

## Record and interpretation

`zr_vm_core/include/zr_vm_core/function.h` defines
`SZrFunctionSourceCallableIdentity` and the enclosing
`SZrFunction.sourceCallableIdentity` / `hasSourceCallableIdentity` fields.

| Field | Meaning in this finite producer |
| --- | --- |
| `schemaVersion` | `ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1`, currently 1 |
| `symbolId` | Nonzero source symbol ID from the actual compilation context |
| `typeId` | Nonzero canonical callable type ID from that same context |
| `canonicalSignatureHash` | Nonzero structural hash of the actual canonical FUNCTION signature |
| `returnPrimitive` | `ZR_VALUE_TYPE_INT64` |
| `parameterCount` | Zero |
| `receiverFlags` | Zero |
| `effectFlags` | Zero |
| `declarationRange` | Copied numeric source declaration start/end line and column |
| `hasExplicitNoArgsI64` | True only after the complete finite admission proof |

The IDs identify the original live compilation context. After compilation,
they are retained numbers for provenance comparison; they are not reusable
lookup handles. The canonical signature hash describes a structural callable
signature. It is neither a metadata-token ABI hash nor a globally unique
definition ID. A consumer cannot use it alone to identify a particular body
or declaration across separate compilations.

No record member borrows an AST pointer, type environment pointer, semantic
context, declaration name buffer, or IR array. Numeric ranges remain valid
without preserving the source AST. The outer presence flag and
`hasExplicitNoArgsI64` describe a proof; a nonzero ID alone is insufficient to
certify a callable.

## Admission and publication ordering

The producer must join the declaration to its callable entry through exact
`declarationNode` identity while the original semantic context is alive.
Name-only lookup would allow shadowed or unrelated functions to contribute
evidence. The selected source symbol and canonical FUNCTION signature must
agree on the same declaration and the admitted primitive return contract.
An absent, ambiguous, or inconsistent join cannot publish an identity.

The AST restricts the supported syntax; it does not supply the numeric value
or fabricate semantic evidence. Review found that the existing return
expression is compiled in an inner semantic-IR isolation which ends before
the parent child-function hook can inspect its value, and that the existing
path does not leave a real semantic RETURN in the child's outer proof graph.
The narrow producer therefore runs while that actual expression IR is alive.

`compiler_source_callable_identity_try_publish_return` is called from
`compile_statement.c` before the return expression's existing inner
`compiler_semantic_ir_isolation_end`. It consumes the actual constant semantic
IR produced by `Expression_Compile`, terminates its real CFG with
`compiler_semantic_cfg_terminate_return`, resolves value facts, and validates
semantic IR before the finite proof and numeric publication. Return value,
defining instruction, and canonical primitive type must agree on i64. The
finite proof is being frozen against the actual scalar literal lowering:
CONSTANT, a temporary PLACE_BASE emitted by fresh-temporary binding, and the
real RETURN. The permitted temporary place needs exact provenance and no
alias, projection, or loan facts. The final recorded shape awaits producer
freeze and GREEN. Old bytecode,
an AST literal value copied into a new graph, or a synthetic analysis graph
cannot substitute for this evidence.

Build the complete candidate locally, then copy the numeric record and set
the outer presence flag. A thin final hook in `compiler_function.c` checks
that ordinary child `hasCallableReturnType` and primitive i64 metadata agree
with the published record. Other return expressions retain their existing
isolation behavior. No new compiler-state field is required. Unsupported
forms remain ordinary compiler inputs and receive no certified record. The
helper is an optional proof: an absent join, unsupported graph, or failed
resolve/validation/proof declines the witness without adding a diagnostic.
Existing compiler errors retain their existing cleanup behavior. This gate
does not add a transaction or classify a failed proof as a new source error.

The enclosing source compiler eventually frees its temporary AST and
compiler state before returning. Publication therefore has to finish while
the original declaration and canonical graph can still be checked, rather
than reconstructing a replacement graph after `Source_Compile` returns.

## Core lifecycle and child assembly

`ZrCore_Function_New` initialization in `function.c` zeros the record and
clears the presence flag. The tombstone cleanup path also clears both. This
record owns no allocation and adds no GC tracing or separate free operation.

Existing child assembly in `compiler_function.c` and
`compiler_function_assembly.c` copies the complete `SZrFunction` by value into
the parent's inline child list. The numeric identity and presence flag travel
with that copy. This mechanism needs no retained semantic-context owner.
The positive public-API fixture checks the inline child reached through the
returned parent, so it observes the actual publication and assembly path.

Ordinary compile failure returns no function for the caller to inspect. The
malformed-source guard then retries a valid compile to check local usability.
It does not inject OOM or a late binding failure and does not establish global
module-summary/cache rollback after finalization.

## Public fixture coverage

`tests/parser/test_ssa_source_callable_identity.c` now contains eleven Unity
cases, extended after the original eight-case RED:

| Case | Observable contract |
| --- | --- |
| Explicit `fn answer(): int { return 9; }` | Existing i64 child return metadata is present; the new complete identity witness is present |
| Compile `first`, keep it rooted, then compile `second` | Both children have witnesses; every field of the first record remains unchanged after the second temporary compilation is released |
| Two named children in one source | Both witnesses are present; symbol IDs and declaration lines differ while canonical signature hashes match |
| Explicit bool return | Returned function tree contains no certified identity |
| Parameterized i64 function | Returned function tree contains no certified identity |
| Nested closure capturing `offset` | Returned function tree contains no certified identity |
| Implicit return | Returned function tree contains no certified identity |
| Unannotated i64 return | Returned function tree contains no certified identity |
| Binary integer return `4 + 5` | Returned function tree contains no certified identity |
| Multiple-statement body `let value = 9; return value;` | Returned function tree contains no certified identity |
| Malformed declaration followed by valid retry | Malformed compile returns NULL; retry still publishes ordinary existing child i64 return metadata |

The fixture performs thirteen public source-compile calls: twelve expected successful
compiles and one malformed compile expected to return NULL. Positive cases
separately assert the ordinary child-return precondition before the feature
assertion. The lifetime case roots the returned parent functions through the
existing GC ignore API and compares fields individually, avoiding padding
bytes as evidence. Negative cases recursively inspect the returned tree.
No compiled source is executed by this fixture.

The two-child case locates children by their retained source names, checks
declaration lines 1 and 2 and distinct symbol IDs, and demonstrates that a
shared callable signature hash does not identify a declaration. The original
RED remains an eight-case historical run; these additional three cases are
awaiting GREEN and are not retroactively included in that RED evidence.

Both ordinary source-test CMake registration and the direct validation driver
include `ssa_source_callable_identity`. Registration is not evidence that both
routes were run. The companion acceptance document records the actual RED
and GREEN evidence once Root has collected it.

## Validation status and remaining gates

The actual pre-producer RED v2 ran eight focused cases: two missing-witness
positive failures, six passing guards, and no reported UBSan fault. Its frozen
fixture SHA-256 is
`60b96105f6d6413bec08a1b878fe1bc8a0434863b395ed8857180db8fc3f0027`.
The receipt SHA-256 is
`c2dedea41e6cb39002c5abaebe9d029060c4f0dcffd33edce004a021b334d815`.
See the companion acceptance record for the immutable receipt and rejected
V1 prerequisite failure. RED is diagnostic evidence, not a passing result.

Final GREEN validation is pending. The bounded route is the reused Windows x64
clang-cl 19 Debug UBSan direct build from the current checkout, with no source
snapshot or copy. Root owns build execution and the immutable receipts.

Ordinary top-level CMake, WSL/Linux, native 32-bit, ASan, LLVM/AOT consumers,
full parser coverage, and the full 47-item SSA plan remain outside this
acceptance. Later work must independently establish retained canonical graph
ownership, tokens, source frame/artifact publication, and same-source AOT
consumption if those contracts are needed. Local parser/Core tests require no
network, FFI, provider, capability, hotpatch, or external-service invocation.
Binary return expressions, multiple statements, branching, loops, and general
acyclic CFG admission remain open; this gate certifies only the single integer
literal return form described above.
