# 2026-07-30 AOT 12 Property Accessor Required Root

## Scope

This sub-milestone covers the property-accessor function roots required by `docs/plans/aot/12-code-stripping.md`:

- non-abstract compiled prototype getter, setter, and initializer members retain their callable target;
- the version 1 function manifest reports `root.property_accessor` with no predecessor;
- an executable accessor whose function constant cannot resolve fails graph construction and the public writer;
- abstract contract-only accessors and non-accessor members remain ignored without requiring a callable target;
- the writer removes its partial generated-C file after that failure.

It does not close field/property metadata-token nodes, constructor nodes, or complete S1/S2/S3/S6.

## RED Evidence

The first unit fixture used indistinguishable empty functions, so the existing function-equivalence rule resolved the
target to the entry function. The fixture was corrected with distinct source locations before accepting RED evidence.

With production unchanged, the corrected MSVC runs produced exactly one failure in each focused executable:

- reachability: 19 tests, `test_static_callable_reachability_rejects_unresolved_property_accessor_roles` failed with
  `Expected FALSE Was TRUE`;
- code stripping: 16 tests, `test_aot_c_code_stripping_rejects_unresolved_property_accessor_root` failed with the
  same message.

The getter/setter/initializer positive loop and public writer positive already passed, isolating the silent unresolved
target path. Replacing that path's `continue` with `return ZR_FALSE` made both suites green.

The strict gate then exposed the contract-only exception: with an abstract getter/setter/initializer loop added, the
20-test MSVC reachability run had one `Expected TRUE Was FALSE` failure. Filtering abstract members before callable
resolution made that suite green. Review then added non-accessor guard coverage without requiring another production
change.

## Coverage Inventory

- Accessor roles 1, 2, and 3 each retain a stable target with `PROPERTY_ACCESSOR` and no predecessor.
- Accessor roles 1, 2, and 3 each reject an out-of-range `functionConstantIndex`.
- Abstract accessor roles 1, 2, and 3 ignore the unresolved constant because they have no executable target.
- Missing property identity and accessor roles 0 and 4 ignore the unresolved constant as non-accessor metadata.
- The public writer preserves function 1, trims unrelated function 2, and emits `root.property_accessor`.
- The public unresolved initializer case returns false and leaves no generated C artifact.
- Existing entry, direct-call, export, manifest, reflection annotation, type-layout, metadata-size, and MethodDef
  stripping tests remain green.

## Validation

Effective source is commit `5553590` plus the exact three code/test overlays for this sub-milestone. Validation used
the existing frozen roots:

- WSL source: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260730-0346-fe684d12`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44

Results:

- WSL GCC: focused CTest 2/2; direct reachability 21/0 and code stripping 16/0.
- WSL Clang: focused CTest 2/2; direct reachability 21/0 and code stripping 16/0.
- Windows MSVC: focused CTest 2/2; direct reachability 21/0 and code stripping 16/0.
- SHA-256 for the function graph implementation and both test files matched the main worktree in both frozen trees.
- Generated C reported functions 3/2/1 before/after/removed and contained the expected property-root row.

The MSVC build retained only the existing MSB8029 warning caused by locating an isolated build below `%TEMP%`.

## Acceptance Decision

Accepted as the executable property-accessor required-root sub-milestone. Required getter/setter/initializer callables
can no longer disappear because malformed compiled accessor metadata was silently ignored, while abstract contract-only
members continue to require no callable target. Full AOT 12 and AOT 07-12 remain active.
