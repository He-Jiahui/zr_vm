# 2026-07-30 AOT 12 Reflection Constructor Required Root

## Scope

This sub-milestone closes the conservative callable-retention gap for reflection `createInstance`:

- public executable constructors on serialized concrete class and struct prototypes retain their callable target;
- the version 1 function manifest reports `root.reflection_constructor` with no predecessor;
- an eligible constructor whose function constant cannot resolve fails graph construction and public writer output;
- abstract/resource/interface/non-public/abstract-member/non-meta/non-constructor cases do not become roots;
- the writer removes its partial generated-C file after a required-target failure.

The slice intentionally roots every eligible serialized constructor. Proving the owner type reflection-reachable and
narrowing this safe set remain later graph work.

## RED Evidence

Unit and public-writer tests were added against unchanged production code. The first GCC focused build failed because
`ZR_AOT_REACHABILITY_REASON_REFLECTION_CONSTRUCTOR` did not exist. The code-stripping test translation unit compiled in
the same build, so the RED was specific to the missing reason/collector behavior rather than fixture structure.

## Coverage Inventory

- Public non-abstract constructors on class and struct prototypes retain stable function index 1 with no predecessor.
- An out-of-range eligible constructor `functionConstantIndex` rejects graph construction.
- Abstract and resource class prototypes, an interface prototype, a private constructor, an abstract constructor,
  a non-meta member, and a destructor ignore an unresolved constant.
- The public writer preserves function 1, trims unrelated function 2, and emits `root.reflection_constructor`.
- The public unresolved constructor case returns false and leaves no generated C artifact.
- Existing entry/direct-call/export/manifest, property accessor, Resource Drop, MethodSpec, reflection annotation,
  type-layout, metadata-size, and MethodDef stripping tests remain green.

## Validation

Effective source is commit `90ff8e4` plus the exact five code/test overlays for this sub-milestone. Validation reused
the frozen roots:

- WSL source: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260730-0346-fe684d12`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44

Results:

- WSL GCC: focused CTest 2/2; direct reachability 28/0 and code stripping 22/0.
- WSL Clang: focused CTest 2/2; direct reachability 28/0 and code stripping 22/0.
- Windows MSVC: focused CTest 2/2; direct reachability 28/0 and code stripping 22/0.
- SHA-256 for all five code/test files matched the main worktree in both frozen trees.
- Generated C retained functions 0/1, omitted function 2, and contained the expected constructor-root row; the
  unresolved negative left no artifact.
- Independent review returned no findings.

The MSVC build retained only the existing MSB8029 warning caused by locating an isolated build below `%TEMP%`.

## Acceptance Decision

Accepted as the conservative reflection constructor required-root sub-milestone. Runtime createInstance binding can no
longer select a public constructor body that code stripping removed solely for lacking a bytecode call edge. Full AOT
10 R3, AOT 12, and AOT 07-12 remain active.
