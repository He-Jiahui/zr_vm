---
scope:
  - Syntax 06B repository promotion and cleanup
  - production parser breaking cutover
  - repository migration inventory v3
  - removed source-intermediate AST path
status: accepted
last_verified: 2026-08-05
---

# Syntax 06B repository promotion acceptance

## Decision

Syntax 06B M4/M5 is accepted. The production frontend has one current grammar.
Known removed spellings remain recognizable only long enough to produce the
fatal `legacy_syntax_removed` diagnostic; the parser returns no AST for them.
Unknown `%identifier` input remains an ordinary syntax error, while `%` and
`%=` remain arithmetic operators.

The review found one real compatibility path beyond the previously tested
percent directives: user source beginning with `intermediate` still entered a
parser that consumed a percent-delimited closure list and constructed five
legacy AST node kinds. That path is now rejected before AST construction. Its
parser helpers, AST payloads, project-import traversal, semantic-query branch,
and syntax-writer names were deleted. The numeric AST slots remain reserved so
later serialized node ids do not move; wire value 13 is explicitly rejected by
the public-contract query.

The full CLI build then found and removed a stale import-walker consumer of the
deleted AST nodes. This was a product build failure, not a test-only issue, and
is part of the accepted cleanup.

## Repository inventory

Scanner v3 reports:

- 899 scanned current candidates and 452 explicit exclusions, including the
  current language specification rather than exempting it as historical;
- zero `findings`, zero `unknown`, and zero findings in every migration
  classification;
- 14 stable allowlisted negative/migration inputs;
- 598 reviewed current semantic candidates: 539 ordinary capitalized calls and
  59 `new` expressions, recorded separately instead of being mislabeled as
  unresolved text migrations;
- 17 files under `tests/fixtures/scripts` explicitly classified as historical
  legacy parser fixtures, with a local README defining that boundary.

The allowlist identity is `file + column + legacy form`, so adding an unrelated
line does not silently invalidate or retarget a negative fixture. The complete
Python protocol suite passes 9/9 and compares the repository JSON byte for
byte.

## Fresh validation

- WSL GCC 11.4: percent cutover 6/6, parser 74/74, syntax reference 13/13,
  semantic query 27/27.
- WSL Clang 14.0: percent cutover 6/6, parser 74/74, semantic query 27/27,
  syntax reference 13/13, and CLI startup passed after an affected rebuild.
- MSVC 19.44 Debug: the same 6/6, 74/74, 27/27, and 13/13 matrix plus CLI
  startup passed after an affected rebuild.
- GCC, Clang, and MSVC product CLI targets link successfully after removed-AST
  consumer cleanup.
- The complete 44-case projects suite passes, including current-reference
  interpreter and binary-first cases.
- Canonical leaf recount remains `TOTAL=55 MISSING=0`, with directory counts
  01=5, 02=6, 03=5, 04=7, 05=6, 06=2, 07=1, 10=5, 12=15, 13=3.

## Review conclusion

No production parser path accepts or lowers the removed percent directives,
old ownership type spellings, old function forms, dollar construction, old
generator output, or user-authored intermediate instructions. Diagnostic
recognizers and the explicit migration frontend are intentional retained
boundaries, not compatibility grammar.
