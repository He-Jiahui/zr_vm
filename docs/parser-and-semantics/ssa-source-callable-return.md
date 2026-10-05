---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/include/zr_vm_parser/core_function.h
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - tests/parser/test_ssa_source_callable_return.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
  - .codex/plans/20261004-source-aot-handoff-design-2005.md
tests:
  - tests/parser/test_ssa_source_callable_return.c
  - docs/acceptance/ssa-source-callable-return.md
doc_type: module-detail
status: finite-script-callable-return-metadata-gate
---

# Source Script Callable Return Metadata

This gate concerns the metadata returned by the existing public source compile
API for a narrowly recognized script entry function. It publishes only through
the existing `SZrFunction` callable-return fields. It does not add a canonical
type identity, retain compiler state, change the Core ABI, or claim to produce
an AOT artifact.

## Admission contract

The candidate is the actual current SCRIPT function from ordinary
`ZrParser_Source_Compile`, after source semantic analysis and CFG finalization.
The proof is based on the active, validated semantic IR and its CFG, rather
than emitted bytecode, source-text pattern matching, or a guessed legacy
opcode. The analysis computes reachability from the real entry block using the
CFG edges without changing CFG visitation state. Only reachable exits
participate in the proof; unreachable synthetic joins do not establish a
return contract.

Metadata is eligible only when the callable has no parameters or varargs and
has no child functions, imports, effects, or closures. The supported return
case requires finite acyclic reachable paths, each ending in a real return
with exactly one operand. The returned value, its defining instruction, and
the canonical primitive type must agree on signed i64. Publication uses the
existing typed type-reference representation and its existing `hasCallableReturnType`
flag. The type reference is populated before the flag is set.

The producer declines metadata when any reachable path falls through, has an
operandless return, has an untyped or non-i64 result, or otherwise fails the
complete-path proof. A declined candidate remains an ordinary successful
source compile when the existing compiler accepts it; absence of the metadata
is not itself a compile error. Structural corruption or allocation failure in
an admitted analysis is an actual failure and follows the existing compile
failure cleanup path.

## Lifetime and representation

The type reference is copied into fields already owned by the returned
`SZrFunction`. The semantic context and AST are released as part of the normal
compile lifecycle, so the published data must not depend on pointers into
either. For the current primitive i64 case, the public metadata is represented
by the existing base type and type-reference fields; this gate does not create
a context-local canonical type ID or preserve the semantic graph.

The hook belongs after source-module semantic CFG finalization, while the
validated semantic information is still alive, and before compiler state is
freed. It is intentionally before later call-binding finalization: changing
the callable signature hash as a consequence of newly published metadata is
part of the existing ordering contract, not evidence that the compiler
retained a new artifact.

## Negative and positive coverage

The focused Unity fixture contains eight public-API cases:

- Four positive comparisons: `1 < 2` and swapped operands, plus `1 > 2` and
  swapped operands. They exercise true and false conditions, and each checks
  the returned function's i64 callable metadata.
- Four guards: reachable fallthrough, mixed i64/bool returns, an operandless
  return, and an implicit-return script. These must compile ordinarily without
  advertising the i64 return contract.

Positive cases also keep the first returned function alive while compiling a
second source, then inspect the first function again after the second
compile's temporary compiler context and AST have been released. This checks
that publication is stored on the function rather than borrowed from the
temporary compilation state.

The first accepted red run is recorded in the external receipt
`source-callable-return-red-v1`. Its frozen test source SHA-256 is
`5ed8fd4f3cc0027a1890b8bcd87d2d2864d2e78a438acfe9a993649141ef9299`.
It demonstrated the intended gap: the four eligible positive cases compiled
but failed because script entry callable return metadata was absent, while
the four guard cases passed. The reviewed green run is recorded in
`E:\\cargo-targets\\zr_vm\\reports\\ssa-20261004-01a0fe2b\\source-callable-return-green-v1\\receipt.json`.
The focused fixture reports 8/8 passing cases; the companion run reports
4, 13, 6, 7, 63, 35, and 8 passing cases plus the standalone scratch
eligibility check, with zero failures and zero ignored cases. The CTest log
SHA-256 is `968cc6600f796ca2cb81a04fb6f54a3d56846b54dd9f51c22b2ca36b89bdb8bd`.
The module summary is finalized before this hook, so late allocation or
binding failure remains subject to the compiler's existing summary rollback
limitation; this gate does not claim OOM rollback coverage.

## Security scope and explicit limits

Validation is limited to local parser/compiler behavior, ordinary source
compilation, CFG inspection, and the existing local test harness. It does not
invoke network, FFI, provider, capability, hotpatch, or external-service
paths, and it does not probe security boundaries. The gate does not establish
callable behavior by executing generated code or by consulting an AOT backend.

This metadata gate does not publish canonical child-function identity, a
retained semantic artifact, token or frame metadata, or an AOT artifact. It
does not claim Linux, native 32-bit, ASan, LLVM, full parser-leaf, or full SSA
plan validation. See the companion acceptance record for the exact bounded
validation evidence and pending items.
