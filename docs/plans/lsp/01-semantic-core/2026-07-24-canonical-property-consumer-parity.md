# Canonical Property Consumer Parity

## Status

- Completed: 2026-07-24 08:16 +08:00
- Scope: LSP 05 L5 property consumer slice
- Baseline: `4bdaad6281842e06cdfadb7ad23a66201c8bac7b`
- Follow-up support: `3c4c172a0ff070a38b52a098d0989706690cd536`

## Contract

The language server consumes `ZrParser_SemanticQuery_PropertyAt` and
`PropertyBySymbolId` as the only source of property identity. Source and
compiled-provider symbols retain the same PropertySymbolId, TypeId, accessor
SymbolIds, access, receiver effect, reference access, declaration range, and
selection range.

Hover, completion, definition, prepare-rename, semantic tokens, migration
actions, and incremental updates join this structured contract. A current
source or compiled property never falls back to `__get_*`, `__set_*`, AST
pairing, or property-name reconstruction. Missing, duplicate, malformed, or
ambiguous identity remains unavailable.

## Evidence

- The frozen [Syntax 05 M5 record](../../syntax/05-property-unified-ast/m5-property-consumers-reflection-migration.md)
  proves source and `.zro` property hover,
  unique completion, definition, rename, contextual setter `value`, migration
  and refactor actions, incremental identity preservation, and metadata
  stripping from the same canonical PropertyQuery contract.
- Its frozen GCC, Clang, and MSVC gates passed the property M1-M5 slices
  (16/16, 21/21, 22/22, 23/23, and 9/9), semantic query 27/27, artifact
  schema 14/14, AOT gates, and stdio/CLI smoke with real exit zero.
- The later import-bootstrap support commit adds fail-closed empty imported
  placeholder handling. GCC, Clang, and MSVC focused runs each passed 11/11
  with real exit zero.
- On 2026-08-10, both commits were confirmed ancestors of the current LSP
  baseline before this historical LSP status record was added.

## Open Scope

This closes only the property source/binary consumer slice of L5. It does not
claim the broader L5 ownership/resource/pool presentation work, L8 fallback
deletion, or the overall semantic-inference plan is complete.
