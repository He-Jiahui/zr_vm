# Syntax 05 M5 property consumers/reflection/migration acceptance

## Status

Completed at 2026-07-24 08:16 +08:00. The frozen GCC/Clang/MSVC snapshot, exact ownership audit,
and milestone commit gate all succeeded.

## Acceptance contract

- Parser consumers expose PropertyAt/PropertyBySymbolId from one owned visible property contract;
  accessor callable TypeIds remain authoritative for receiver and reference effects.
- The canonical artifact schema uses a fixed 48-byte PropertyDef with exact linked tokens. The
  current `.zro` v34 bridge consumes byte-stable compiled property/accessor rows with shared
  property identity and does not claim an absent nested token table.
- Reflection joins the visible property and accessors by identity/role, keeps fields separate, and
  never promotes a legacy-looking ordinary method by name.
- LSP hover, completion, definition, rename, tokens, contextual `value`, migration, and refactor
  actions consume canonical parser/query facts for source and binary modules.
- Legacy migration emits an exact structured replacement only for a safe single accessor or an
  adjacent fully matching pair. Ambiguous, stale, invalid, binary-only, or reference-property cases
  remain unavailable.
- Stress binds 128 properties/256 accessors. Incremental analysis preserves unrelated property
  identities across body and contract edits and invalidates the exact changed TypeId.
- AOT stripping retains concrete property accessors only through structured compiled-row identity;
  an unrelated unused method remains removable.

## Evidence

- On one frozen overlay, GCC, Clang, and MSVC each pass property M1-M5 at 16/16, 21/21, 22/22,
  23/23, and 9/9; semantic query at 27/27; artifact schema at 14/14; and AOT stripping/annotation at
  10/10, 12/12, and 3/3. Every claimed process exits zero.
- All three toolchains pass LSP interface, project, UTF-16, source-contract, and local-semantic-query
  targets. MSVC ASan first reproduced a lazy-import `typePrototypes` reallocation use-after-free;
  stable prototype facts fixed it, after which ASan passes 3/3 and ordinary MSVC Debug passes 10/10.
- GCC, Clang, and MSVC stdio/CLI smoke processes all exit zero; each CLI emits exactly `40`.
- Clean `5ef5d9b` reproduces the unrelated module-system, metadata-token, reflection-token, and
  local-semantic-hover failures/waits. They retain the same final-overlay shape and are recorded as
  pre-existing baseline, not as green M5 evidence. A concurrent GCC/Clang fixture-writing run was
  discarded in full; only sequential replay is accepted.
- The 70 exact M5 paths are SHA-256 identical between the frozen snapshot and the working tree.
  `git diff --check` passes, forbidden Syntax/build/generated paths are zero, and the shared index is
  empty before exact staging.
