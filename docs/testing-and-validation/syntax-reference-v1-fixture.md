---
related_code:
  - tests/CMakeLists.txt
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/syntax_reference_v1/syntax_reference_v1.zrp
  - tests/fixtures/projects/syntax_reference_v1/golden/coverage.json
  - tests/fixtures/projects/syntax_reference_v1/golden/diagnostics.json
  - tests/fixtures/projects/syntax_reference_v1/golden/provider-locator.json
  - tests/fixtures/projects/syntax_reference_v1/src/host.zr
  - tests/fixtures/projects/syntax_reference_v1/src/host.min.zr
  - tests/fixtures/projects/syntax_reference_v1/native/syntax_reference_native.c
implementation_files:
  - tests/CMakeLists.txt
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/syntax_reference_v1/syntax_reference_v1.zrp
  - tests/fixtures/projects/syntax_reference_v1/golden/coverage.json
plan_sources:
  - user: execute docs/plans/syntax milestones with a completion record and one commit per milestone
  - docs/plans/syntax/2026-07-19-07-comprehensive-syntax-reference-fixture-design.md
  - docs/plans/syntax/07-comprehensive-syntax-reference-fixture/m1-reference-fixture-manifest-implementation-plan.md
tests:
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/syntax_reference_v1/golden/coverage.json
  - tests/fixtures/projects/syntax_reference_v1/golden/diagnostics.json
  - tests/fixtures/projects/syntax_reference_v1/golden/provider-locator.json
  - tests/acceptance/2026-07-24-syntax-07a-m1-reference-fixture-manifest.md
doc_type: testing-guide
---

# Syntax Reference V1 Fixture

## Purpose

`syntax_reference_v1` is a single repository fixture that gives the syntax redesign one stable inventory
of source, diagnostics, provider, artifact, and consumer expectations. 07A deliberately makes this an
auditable skeleton first: it proves discovery and provenance rules without claiming that still-pending
expression-arrow, reflection, pooling, native provider, compile-time, async, iterator, or test-harness
semantics execute.

## Collection Model

`golden/coverage.json` is the source of truth. It has three disjoint collections:

- `current` contains only files that 07A may use as stable source slots. The fixture test currently
  parses every listed source and checks that each advertised feature has syntax evidence. Only the compact
  `host.zr` equivalence source is compiled and emitted by this discovery milestone.
- `negative` contains inputs with a named primary diagnostic, never a pending positive case disguised as
  a failure fixture.
- `design-pending` reserves the source location and the downstream consumer contract. Every entry names
  `ownerPlan`, `ownerGate`, and `expectAfterPromotion`; the 07A target does not compile it.

The feature id is stable and unique. Later 07B changes the status only after the named owner gate has
independent evidence; it must not replace the id, guess a new source spelling, or add a consumer fallback.
Each feature also records its owning `collection`; the fixture test rejects a feature whose source is absent
from that collection and rejects overlapping collection file lists.

## Provider And Locator Boundary

The fixture has both a Workspace file `src/engine/render.zr` and a pending RegisteredNative entry
`native:engine.render`. They intentionally share a display name while preserving different module-domain
identity. `golden/provider-locator.json` records those identities, not a filesystem resolution result.

The generated `file:` source uses `${SYNTAX_REFERENCE_FILE_URI}`. The focused fixture target renders the
actual local source path as a canonical `file:` URI at runtime and verifies that it is not a placeholder or
a backslash path; the URI formatter also covers POSIX, drive-letter, and UNC authority paths. Checked-in
goldens retain only the placeholder and reject `C:/`, backslash paths, and `/mnt/` so a developer-machine
path never becomes public test data.

## Formatting Proof

`host.zr` and `host.min.zr` are semantically identical versions of one current function. The test writes
both syntax trees and intermediate files. The `.zrs` files must have identical byte fingerprints. The
SemIR fingerprint ignores exactly `START_LINE` and `END_LINE`, because those are source-map positions
whose values legitimately differ after minification; instruction, type, effect, and control-flow data
remain part of the hash.

## Test Coverage

`zr_vm_syntax_reference_v1_test` verifies feature inventory cardinality, collection status and pending
gate metadata, per-feature source ownership, current source syntax evidence and parser acceptance, required
skeleton paths, the pending anonymous expression-arrow target syntax, provider/locator hygiene, and
formatted/minified AST/SemIR equivalence. The target is also registered as the focused
`syntax_reference_v1` CTest entry. Toolchain matrix and acceptance evidence are recorded with the Syntax
07A milestone record.

## Scope Limits

This fixture does not register a native module, emit a `.zrm`, run a project suite, write binary/AOT
goldens, or request LSP output. Those are 07B responsibilities after the relevant owner plans pass their
own promotion gates.
