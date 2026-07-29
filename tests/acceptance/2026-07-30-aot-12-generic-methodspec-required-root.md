# 2026-07-30 AOT 12 Generic MethodSpec Required Root

## Scope

This sub-milestone closes the current-module callable-retention gap left by the earlier manifest MethodSpec binding:

- a writer generic preserve root with a MethodSpec binding retains the uniquely bound `MemberDef` callable;
- the version 1 function manifest reports `root.generic_methodspec` with no predecessor;
- null, invalid, missing, ambiguous, or unmappable required bindings fail graph construction and public writer output;
- TypeSpec-only generic roots do not retain a callable in the function collector;
- the writer removes its partial generated-C file after a required-target failure.

The slice does not claim cross-module `MemberRef` resolution, dictionary/witness edges, or complete generic metadata
mark-and-sweep closure.

## RED Evidence

The unit and public-writer tests were added before production changes. The first GCC focused build failed because
`ZR_AOT_REACHABILITY_REASON_GENERIC_METHODSPEC` and
`backend_aot_compute_static_callable_reachability_with_generic_roots()` did not exist. The code-stripping test source
compiled in the same build, isolating the initial RED to the missing graph contract. After implementation, one
generated-text assertion exposed a test-only table-tag assumption (`0x05`); direct inspection showed the repository's
actual `MemberDef` token `0x03000007`, and the assertion was corrected without changing production behavior.

## Coverage Inventory

- A non-exported MethodSpec-bound callable retains stable function index 2 with `GENERIC_METHODSPEC` and no predecessor.
- A null generic-root array with nonzero count, a missing `MemberDef`, a `MemberRef`, and duplicate token bindings reject
  graph construction.
- A TypeSpec-only generic root leaves both child callables unmarked unless another edge retains them.
- The public writer emits all three expected functions and the stable `root.generic_methodspec` row.
- The public unresolved MethodSpec case returns false and leaves no generated C artifact.
- Existing entry/direct-call/export/manifest, property accessor, Resource Drop, reflection annotation, type-layout,
  metadata-size, and MethodDef stripping tests remain green.

## Validation

Effective source is commit `7634662` plus the exact seven code/test overlays for this sub-milestone. Validation reused
the frozen roots:

- WSL source: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260730-0346-fe684d12`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44

Results:

- WSL GCC: focused CTest 2/2; direct reachability 26/0 and code stripping 20/0.
- WSL Clang: focused CTest 2/2; direct reachability 26/0 and code stripping 20/0.
- Windows MSVC: focused CTest 2/2; direct reachability 26/0 and code stripping 20/0.
- SHA-256 for all seven code/test files matched the main worktree in both frozen trees.
- Generated C retained functions 0/1/2, carried `methodSpec.methodToken = 0x03000007`, and contained the expected
  MethodSpec-root row; the unresolved negative left no artifact.
- A broader GCC matrix passed metadata token, generic instantiation, and code stripping. Its generic-call source suite
  failed before AOT writing because current parser syntax intentionally rejects the suite's legacy keywordless
  declaration and `$` construction syntax; no parser or fixture changes are part of this milestone.

The MSVC build retained only the existing MSB8029 warning caused by locating an isolated build below `%TEMP%`.

## Acceptance Decision

Accepted as the current-module generic MethodSpec required-root sub-milestone. A manifest-preserved generic method can
no longer lose its callable body solely because no bytecode call edge referenced it. Full AOT 12 and AOT 07-12 remain
active.
