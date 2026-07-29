# 2026-07-30 AOT 12 Resource Drop Required Root

## Scope

This sub-milestone closes the first Resource Drop callable-retention gap from
`docs/plans/aot/12-code-stripping.md`:

- a non-abstract `ZR_META_DESTRUCTOR` on a serialized resource prototype retains its callable target;
- the version 1 function manifest reports `root.resource_drop` with no predecessor;
- an executable resource destructor whose function constant cannot resolve fails graph construction and the public
  writer;
- non-resource, non-meta, other-meta, and abstract members do not become Drop roots;
- the writer removes its partial generated-C file after a required-target failure.

This slice is deliberately conservative: every executable resource destructor in serialized prototype metadata is a
root. Coupling destructor retention to reachable type/layout nodes remains open.

## RED Evidence

The unit and public-writer tests were added against unchanged production code. The MSVC runs failed only the four new
required behaviors:

- reachability: 24 tests with two failures; the retained count was `Expected 2 Was 1`, and unresolved metadata was
  `Expected FALSE Was TRUE`;
- code stripping: 18 tests with two failures; the destructor function/manifest row was absent, and the unresolved
  writer returned true.

The four-case filter matrix already passed, proving the RED was specific to missing executable Resource Drop roots.
After the new reason was added, the existing unknown-reason test exposed its expected enum-boundary maintenance: its
old `PROPERTY_ACCESSOR + 1` sentinel had become the valid Resource Drop value. Moving the sentinel beyond the new enum
restored the malformed-schema proof.

## Coverage Inventory

- A valid resource destructor retains stable function index 1 with `RESOURCE_DROP` and no predecessor.
- An out-of-range resource destructor `functionConstantIndex` rejects graph construction.
- A non-resource destructor, a non-meta member, a constructor meta method, and an abstract destructor ignore an
  unresolved constant.
- The public writer preserves function 1, trims unrelated function 2, and emits `root.resource_drop`.
- The public unresolved destructor case returns false and leaves no generated C artifact.
- Existing property accessor, entry, direct-call, export, manifest, reflection annotation, type-layout, metadata-size,
  and MethodDef stripping tests remain green.

## Validation

Effective source is commit `dff63c4` plus the exact five code/test overlays for this sub-milestone. Validation reused
the frozen roots:

- WSL source: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260730-0346-fe684d12`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44

Results:

- WSL GCC: focused CTest 2/2; direct reachability 24/0 and code stripping 18/0.
- WSL Clang: focused CTest 2/2; direct reachability 24/0 and code stripping 18/0.
- Windows MSVC: focused CTest 2/2; direct reachability 24/0 and code stripping 18/0.
- SHA-256 for the reason schema, function graph implementation, and both test files matched the main worktree in both
  frozen trees.
- Generated C reported functions 3/2/1 before/after/removed, contained the expected Drop-root row, and the unresolved
  negative left no artifact.

The MSVC build retained only the existing MSB8029 warning caused by locating an isolated build below `%TEMP%`.

## Acceptance Decision

Accepted as the Resource Drop required-root sub-milestone. Runtime destructor dispatch can no longer target a function
removed solely because no bytecode instruction directly referenced it. Full AOT 12 and AOT 07-12 remain active.
