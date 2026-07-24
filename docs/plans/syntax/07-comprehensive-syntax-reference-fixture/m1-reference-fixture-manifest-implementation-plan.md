# Syntax 07A M1 Reference Fixture And Coverage Manifest Implementation Plan

## Goal

Implement Task 1 from
`docs/plans/syntax/2026-07-19-07-comprehensive-syntax-reference-fixture-design.md`.
The milestone creates the discovery-only skeleton for `syntax_reference_v1`; it does not promote
unimplemented language features or modify production parser, semantic, runtime, AOT, or LSP behavior.

## Scope

- Create `tests/fixtures/projects/syntax_reference_v1/` with a project manifest, current source slots,
  negative inputs, design-pending source slots, provider/package/artifact placeholders, and path-free
  golden JSON.
- Add `tests/parser/test_syntax_reference_v1.c` and the focused CMake target
  `zr_vm_syntax_reference_v1_test`.
- Keep the three discovery collections disjoint: `current`, `negative`, and `design-pending`.
- Record the owning collection on every feature and verify that its source belongs to that collection.
- Require every pending feature to name an owner plan, owner gate, and expected post-promotion result.
- Verify formatted and minified current host sources produce the same syntax-tree fingerprint and the
  same source-range-independent SemIR fingerprint.

## Exclusions

- Do not compile design-pending fixture files as current project evidence.
- Do not implement `08` reflection, `09` pooling, `10` resolver/FFI/provider, or `11-14` language
  features in this milestone.
- Do not create machine-specific `file:` golden values or register a real native provider before 10C.
- Do not change existing Syntax design drafts, LSP headers, or generated fixture directories owned by
  other work.

## TDD Sequence

1. Add the fixture-discovery target and test, then run it with the fixture absent. It must fail because
   `golden/coverage.json` cannot be read.
2. Add the skeleton manifest and source files. Re-run to verify stable feature ids, collection boundaries,
   owner gates, and required paths.
3. Add the provider/locator portability assertion first; let it fail on missing workspace/package/artifact
   paths; then add the static provider skeleton.
4. Add an AST/SemIR equivalence assertion with a deliberately divergent minified host source; restore the
   equivalent source only after the mismatch is observed.
5. Add a structural mapping RED: source collections must be pairwise disjoint, every feature must declare
   its collection, and every current slot must contain the claimed syntax evidence and parse.
6. Register the focused target as `syntax_reference_v1` in CTest. Run the focused target and CTest entry in
   GCC, Clang, and MSVC isolated builds, record evidence, audit exact paths, and make one milestone commit.

## Acceptance

- `coverage.json` contains exactly one entry for each stable 07A feature id.
- `design-pending` entries have `ownerPlan`, `ownerGate`, and `expectAfterPromotion`, and none are in the
  current collection.
- `native:engine.render` and the Workspace `engine.render` slot remain distinct logical identities.
- The locator golden only contains `file:${SYNTAX_REFERENCE_FILE_URI}`, never a host path.
- Every feature's `status`, `collection`, and `source` agree; collection file lists are pairwise disjoint.
- Current source slots contain the advertised syntax and are parser-accepted; pending source slots remain
  outside current compiler evidence.
- The formatted/minified host fixtures agree on `.zrs` bytes and on `.zri` after excluding only
  `START_LINE` and `END_LINE` source-map metadata.
