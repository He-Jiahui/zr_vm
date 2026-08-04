---
scope: Syntax 08 M3 artifact metadata graph and preservation
status: proven
date: 2026-08-04
toolchains:
  - WSL GCC 11.4
  - WSL Clang 14.0
  - MSVC 19.44 x64 Debug
---

# Syntax 08 M3 artifact metadata graph acceptance

## Reopened defects

The production artifact schema carried TypeDef, MemberDef, PropertyDef, Contract, and Layout rows,
but it had no explicit reflection preservation state, retained metadata record schema, immutable
metadata hashes, or composite GC/ownership/reference layout map. Member owners were shape-checked
without referential closure, and source/native/binary reflection projections had no common row to
compare. A stripped table could therefore be confused with an empty declaration.

The initial RED tests failed at link time on the deliberately missing metadata hash and projection
APIs. After those were implemented, review exposed and covered a second defect: a PropertyDef could
link an accessor owned by another TypeDef because only token existence was checked.

## Implemented contract

- Advanced the canonical schema from v3 to v4 as a breaking recompile boundary.
- Added fixed 64-byte MetadataState and 40-byte MetadataRecord rows, metadata-blob and layout-map
  heaps, interface/abstract/enum TypeDef flags, and explicit preservation/category/retention enums.
- Defined `IdentityOnly`, `Members`, and `Full` retained states with exact count and generation
  closure. An absent state table means reflection metadata was not published.
- Added canonical little-endian state and record hashes. Record hashes include the exact payload;
  neither hash depends on native structure padding.
- Enforced TypeDef/member/property/accessor ownership, constructor, contract, layout, generation,
  record payload, and retained-count links in both writer input and decoded binary views.
- Added layout-map v1 with bounded, sorted GC/ownership/reference offsets and GC scan consistency.
- Added one source/native state projection. Native prototype category, backing-array shape, exact
  descriptor member/enum count, and projected property count must match; class-as-struct and erased
  retained metadata are rejected.
- Registered the artifact executable as a CTest so acceptance cannot silently skip it.

## Negative matrix

Direct assertions reject identity-only state with retained tables, erased/full and forged category,
stale state hash, stale record payload hash, dangling record owner, dangling MemberDef owner,
cross-TypeDef property accessor, corrupt layout offset, oversized table/count, unknown mandatory
section, truncation, and illegal token/table shapes. Each relevant writer and reader path is covered.

## Three-toolchain matrix

| Suite | GCC | Clang | MSVC |
|---|---:|---:|---:|
| artifact schema and metadata graph | 25 | 25 | 25 |
| reflection type surface | 19 | 19 | 19 |
| reflection token resolve | 30 | 30 | 30 |
| reflection method invoke | 6 | 6 | 6 |
| **Total** | **80** | **80** | **80** |

All assertions passed. MSVC additionally identified two short-circuit-protected local rows as
possibly uninitialized; both are now explicitly zero-initialized and the warning no longer appears
for the metadata graph module.

## Review result

M3 now has one fixed-width retained metadata contract across source projection, native descriptor
projection, `.zri`, and `.zro`. Validation is token/hash/generation based and has no member-name or
type-name repair path. The schema records immutable expanded metadata only; compile-time attribute
and patch semantics remain owned by Syntax 11.

Syntax 08 M3 is promoted. Syntax 08 M4-M5 remain open for runtime construction/cache/AOT/native
module integration and LSP/migration/stress acceptance.
