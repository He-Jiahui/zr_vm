# 2026-07-30 AOT 12 Package Method Export Required Root

## Scope

This sub-milestone closes the callable-retention gap between package manifest exports and code stripping:

- a package `METHOD` declaration with a current-module `MemberDef` binding retains its unique callable target;
- the version 1 function manifest reports `root.package_export` with no predecessor;
- missing, invalid, unresolved, ambiguous, or unmappable required method bindings fail graph construction;
- package `TYPE` and `FIELD` declarations do not become function roots, and unknown kinds fail closed;
- the writer removes its partial generated-C file after a required-target failure.

The collector consumes structured token bindings only. It does not search the declaration target string. Canonical
cross-module `ModuleIdentity` and `MemberRef` resolution remain later AOT 11/12 work.

## RED Evidence

Unit and public-writer tests were added against unchanged production code. The first GCC focused build failed because
`ZR_AOT_REACHABILITY_REASON_PACKAGE_EXPORT` and
`backend_aot_compute_static_callable_reachability_with_preserve_roots()` did not exist. The code-stripping test
translation unit compiled and linked in that build, isolating RED to the missing graph contract.

## Coverage Inventory

- A non-source-exported child with a unique current-module `MemberDef` binding is retained at stable function index 2.
- A null declaration array with nonzero count, missing binding, `MemberRef`, missing symbol, ambiguous symbol, and
  unknown declaration kind each reject graph construction.
- Type and field declarations leave both otherwise-unreachable child functions unmarked.
- The public writer preserves functions 0/1/2, emits the bound token and `root.package_export`, and reports zero
  removed functions for the fixture.
- The unresolved public-writer case returns false and leaves no generated C artifact.
- Existing entry/direct-call/source-export/manifest, property accessor, Resource Drop, reflection constructor,
  MethodSpec, reflection annotation, type-layout, metadata-size, and MethodDef stripping tests remain green.

## Validation

Effective source is commit `0bf1381` plus the exact seven code/test overlays for this sub-milestone. Validation reused
the frozen roots:

- WSL source: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260730-0346-fe684d12`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44

Results:

- WSL GCC: focused CTest 2/2; direct reachability 30/0 and code stripping 24/0.
- WSL Clang: focused CTest 2/2; direct reachability 30/0 and code stripping 24/0.
- Windows MSVC: focused CTest 2/2; direct reachability 30/0 and code stripping 24/0.
- WSL GCC adjacent metadata regression: `aot_c_zrp_metadata_pruning` and
  `aot_c_zrp_metadata_export_token_remap` passed 2/2.
- SHA-256 for all seven code/test files matched the main worktree in both frozen trees.
- Generated C retained functions 0/1/2, published member token `0x0300000b` and the expected package-export row; the
  unresolved negative left no artifact.
- Independent review returned no findings.

The MSVC build retained only the existing MSB8029/MSB8064 warnings caused by locating an isolated build below
`%TEMP%`.

## Acceptance Decision

Accepted as the current-module package method export required-root sub-milestone. Package metadata can no longer
advertise a token-bound local method whose body code stripping removed solely for lacking a bytecode or source-export
edge. Canonical cross-module package identity, full AOT 11/12, and AOT 07-12 remain active.
