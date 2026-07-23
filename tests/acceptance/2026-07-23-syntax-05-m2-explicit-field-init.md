# Syntax 05 M2 Explicit Field/Init Acceptance

## Scope

- Canonical `let`/`var` bindings for locals, loops, and explicit member fields.
- Structured constructor/init-accessor phase with exact `INIT` versus `SET` selection.
- Exactly-once immutable explicit-field initialization and shallow immutability boundaries.
- FieldDef/PropertyDef storage, reflection, artifact, and initialization-bitmap separation.

## TDD Evidence

- Initial focused RED: `let` was not a token/parser entry and constructor assignment could not select
  an init-only property accessor.
- Semantic RED/GREEN: repeat and compound immutable writes, foreign/static init receivers, nested
  callable phase leakage, and inline-struct subfield mutation were rejected from structured facts.
- Range gate: foreign receiver failure covers the exact assignment expression; static init failure
  covers the exact `init` keyword.
- Artifact gate: source and reloaded `.zro` prototype bytes match, reflection exposes two fields and
  one property, and only the two fields consume constructor bitmap positions.
- Runtime RED/GREEN: ordinary member dispatch initially rejected constructor writes to readonly field
  descriptors; serialized member-entry provenance now gates the one initialization-only object API.
- Project RED/GREEN: a constructor declared before its init property initially missed the accessor and
  emitted dynamic index assignment; fields/properties/callables now bind in support-first phases while
  retaining declaration order.
- Review RED/GREEN: focused coverage expanded from 13 to 21 cases for destructuring `let`, exact keyword
  ranges, syntax writer ownership, hidden init calls, init-body repeated writes, typed preceding-method
  resolution, strict heap initialization capability, inline struct `let`, and constructor return/throw
  paths. GCC focused is `21 Tests / 0 Failures / exit 0`.
- Final review RED/GREEN: hidden init accessors are unavailable through direct call, bare member reference,
  and alias/dynamic call. Constructor definite-assignment consumes the visible property's linked init
  accessor effect, so accessor-only immutable-field initialization executes successfully and a preceding
  direct write plus the same effect is rejected at compile time. Normal early-return paths are intersected
  with fallthrough; an accessor that may return before writing cannot satisfy constructor completion.
- Runtime single-use gate remains defense in depth: exact readonly-field initialization succeeds once;
  repeated, writable, property, static, and dynamic targets are rejected by the internal object capability.
- Existing non-promotion baseline: `zr_vm_execution_member_access_fast_paths_test` crashes on both pure
  HEAD `739efc5` and the M2 overlay in the same pre-existing closure teardown case at
  `gc_mark.c:1099`; it is not counted as M2 pass evidence or included in the promotion matrix.

## Final Matrix

- Frozen source: detached `739efc5` plus 48 exact M2 paths; Linux and Windows snapshots match the
  working-tree bytes for every owned path (`mismatch=0`).
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228 each report real process exit `0` for the same matrix:
  focused 21/21, property 16/16, parser 75/75, literal 57/57, receiver 28/28, canonical 16/16,
  semantic query 27/27, type-layout 38/38, debug metadata 4/4, decorator 4/4, object fast path
  61/61, compiler integration 127/127, and AOT ownership contract 1/1.
- `classes_properties.zrp` source CLI smoke exits `0` and prints `40` on all three toolchains.
- `git diff --check` has no errors; the shared index is empty before exact staging; forbidden LSP,
  unrelated Syntax drafts, generated artifact, snapshot, build, and log paths are zero.

## Promotion Status

- Status: completed.
- Completion time: 2026-07-23 11:52 +08:00.
