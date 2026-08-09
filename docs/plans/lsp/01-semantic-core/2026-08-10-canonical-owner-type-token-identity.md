# Canonical Owner Type Token Identity

## Status

- Completed: 2026-08-10 07:49 +08:00
- Scope: LSP 08 independent canonical owner type token contract
- Baseline: `7ab732e`

## Contract

The parser publishes `ZR_SEMANTIC_REFERENCE_TYPE` for resolved generic owner
type uses. A generic type fact retains the generic AST identity but projects
its range to the base type identifier, so `Unique<Socket>` resolves at `Unique`.
`CanonicalTypeAt` therefore returns the exact canonical owner type without an
LSP spelling, AST-pairing, or member-name fallback.

The semantic-token consumer first queries this canonical fact. Only an exact
`ZR_CANONICAL_TYPE_OWNER` result receives the `class` token type. If a query is
not materialized yet, the consumer asks the parser resolver to publish the
same fact and queries again. The local `Unique` identifier in a declaration
such as `var Unique: int` remains a `variable`; `Unique<Socket>` is the only
owner-type token classified as `class`.

The former local `Unique`/`Shared`/`Weak` spelling table was deleted. The
consumer cannot infer ownership presentation from a name.

## Evidence

- RED first demonstrated that a same-named local variable was incorrectly
  classified as `class`; a second RED showed that the prior generic fact range
  started at `<Socket>` rather than the base type identifier.
- `tests/parser/test_semantic_query.c` now verifies that
  `CanonicalTypeAt` reads a resolved type-reference fact at the exact base
  identifier range.
- `tests/language_server/test_lsp_interface.c` verifies that the owner type
  receives `class` while the local name remains `variable`.
- GCC, Clang, and MSVC each passed the focused parser semantic-query suite
  (28/28) and the LSP interface test executable with real exit zero.

## Open Scope

This accepts one independently verifiable L8 contract only. Remaining L8
local type and ownership fallback deletion, provider/project coverage, and
the full protocol matrix are still open. It does not mark L8 or the overall
semantic-inference plan complete.
