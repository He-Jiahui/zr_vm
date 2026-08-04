---
scope:
  - docs/plans/syntax canonical leaf status records
  - production parser one-shot breaking cutover
  - explicit legacy migration frontend
  - syntax migration inventory
status: accepted-in-scope
last_verified: 2026-08-04
---

# Syntax breaking cutover final review

## Decision

The canonical selector still yields `TOTAL=55 COMPLETE=55 MISSING=0` with
directory counts 01=5, 02=6, 03=5, 04=7, 05=6, 06=2, 07=1, 10=5, 12=15,
and 13=3. This confirms the historical leaf records in their declared scope;
it does not promote the root Syntax redesign or any explicitly open upper gate.

The production parser has one current grammar. `%module`, `%import`, `%extern`,
`%compileTime`, `%test`, `%owned`, `%type`, `%func`, ownership `%` forms, old
parameter markers, and `$` construction produce fatal removed-syntax
diagnostics and no AST. `%` and `%=` arithmetic remain valid. Unknown
`%identifier` input remains an ordinary syntax error rather than an implicit
migration rule.

The unused standalone `$` prototype-reference parser function and declaration
were deleted. The retained prototype-reference AST node is created only by
canonical typed construction parsing and is not reachable from `$` source.

## Migration boundary

`ZrParser_LegacyMigration_PlanSource` remains the explicit migration-only
frontend. Complete `$(target)(arguments)` input now receives a review-only
candidate:

```zr
reflection.requireConstructible(target).createInstance(...[arguments])
```

The edit covers the full balanced call, is marked
`ZR_DIAGNOSTIC_FIX_MAYBE_INCORRECT`, is not applied by
`ApplyMachineEdits`, and replans to no legacy item after manual adoption.
Malformed target or argument groups are blocked. No runtime type expression is
lowered to a static construct expression.

## Fresh evidence

- WSL GCC `zr_vm_percent_syntax_cutover_test`: 6/6.
- WSL GCC `zr_vm_legacy_migration_test`: 12/12.
- Canonical leaf selector: 55/55 complete, zero missing markers.
- Inventory protocol: 9/9 against a staged-index snapshot.
- Repository inventory: 651 `requiresReview`, zero machine-applicable,
  maybe-incorrect, blocked, target-not-promoted, or unknown findings; six
  allowlisted negative examples.
- Production parser scan: zero `%`-prefixed legacy keyword literals; remaining
  legacy names are diagnostic routing, migration code, negative fixtures, or
  historical documentation.

The root Syntax redesign remains open where the upper-gate ledger says it is
open. That limitation is intentional and prevents a leaf-status census from
being misreported as repository-wide completion.
