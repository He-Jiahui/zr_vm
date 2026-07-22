# Syntax 05 M1 Unified Property AST/Symbol Acceptance

## Scope

- One contextual property grammar for class, struct, resource class, and interface.
- One visible PropertySymbol with linked getter/setter/init accessor symbols.
- Legacy source property nodes rejected as semantic inputs.
- Structured serialization/reflection of visible property identity and metadata.

## TDD Evidence

- Parser RED: unified AST kinds and structures were absent.
- Symbol RED: visible property/accessor links were absent.
- Runtime consumer regression: decorator import exposed a visible property as a method before the
  hidden compatibility property entry; structured property reflection made the visible property the
  canonical first entry.
- Parent regression: imported source prototypes initially dropped serialized `propertyIdentity` and
  `accessorRole`, so property reads appeared to have no getter. The import bridge now preserves those
  facts and resolves remapped accessors by identity plus role only.
- Review regressions: ordinary `__get_*` methods cannot create phantom reflected properties; absent
  getter/setter contracts fail before generic member lowering; missing property close recovery keeps
  the following member and reports one non-overlapping diagnostic.

## Final Matrix

All rows used the same `HEAD=498c791 + 70 exact paths` frozen snapshot and returned real process
exit code zero.

| Toolchain | Property | Parser | Receiver | Canonical | Query | Compiler | Debug | Decorator | CLI |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| GCC 11.4 | 16/16 | 75/75 | 28/28 | 16/16 | 27/27 | 127/127 | 4/4 | 4/4 | exit 0, `40` |
| Clang 14.0 | 16/16 | 75/75 | 28/28 | 16/16 | 27/27 | 127/127 | 4/4 | 4/4 | exit 0, `40` |
| MSVC 19.44 | 16/16 | 75/75 | 28/28 | 16/16 | 27/27 | 127/127 | 4/4 | 4/4 | exit 0, `40` |

Snapshot SHA-256 comparison reported mismatch=0. `git diff --check` reported no whitespace
errors; line-ending notices are repository policy warnings rather than diff failures.

## Promotion Status

- Status: completed.
- Completion time: 2026-07-23 07:12 +08:00.
- Exact milestone commit remains the final repository operation; the implementation and promotion
  evidence are complete.
