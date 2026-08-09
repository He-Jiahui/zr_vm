# LSP L8 Canonical Owner Type Token Identity

## Scope

Verify that LSP semantic tokens consume canonical type-reference facts for
owner types, including generic owner type base identifiers, without inferring
ownership from the identifier spelling.

## Baseline

Before this slice, semantic tokens used a local `Unique`/`Shared`/`Weak`
spelling table. A local variable named `Unique` was therefore classified as a
class token. The first parser fact for `Unique<Socket>` also covered only the
generic suffix instead of the base identifier.

## Test Inventory

- `tests/parser/test_semantic_query.c` checks a resolved
  `ZR_SEMANTIC_REFERENCE_TYPE` fact and `CanonicalTypeAt` for `Unique<Socket>`.
- `tests/language_server/test_lsp_interface.c` checks that
  `Unique<Socket>` is a `class` token while `var Unique: int` and its use are
  `variable` tokens.

## Tooling Evidence

- GCC focused parser semantic query: 28/28, real exit zero.
- Clang focused parser semantic query: 28/28, real exit zero.
- MSVC focused parser semantic query: 28/28, real exit zero.
- GCC, Clang, and MSVC focused LSP interface executables: real exit zero.

## Results

Resolved generic owner type uses now publish a canonical type-reference fact
with an exact base-identifier range. The token consumer queries
`CanonicalTypeAt`, and only the canonical owner type kind maps to `class`. It
may ask the parser resolver to materialize the same fact, but has no ownership
spelling table or fallback.

## Acceptance Decision

Accepted on 2026-08-10 07:49 +08:00 as an independent L8 canonical token
contract. This is not acceptance of full L8: remaining local fallback removal,
provider/project coverage, and the full protocol matrix remain required.
