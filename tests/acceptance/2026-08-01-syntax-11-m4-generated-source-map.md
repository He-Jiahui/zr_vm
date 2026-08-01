# Syntax 11 M4 generated source-map acceptance

Date: 2026-08-01

Scope: deterministic `.zri` projection of retained first-version
`GeneratedField` provenance. This record promotes Gate 11 M4 for its published
GeneratedField-only surface; it does not promote Gate 11 M5 or the root Syntax
redesign.

## Contract

- A generated member that retains `generated == 1`, a nonzero
  `originTargetSymbolId`, and a valid source-line range emits exactly one
  structured source-map row.
- Rows use ordinary compiled prototype/member order and therefore remain
  deterministic for the same compiled function.
- The row identifies the generated type/member, origin target SymbolId, and
  inclusive source-line range.
- A function with no generated provenance does not acquire an empty
  `GENERATED_SOURCE_MAPS (0)` section.
- The writer only projects retained semantic metadata. It does not introduce a
  source parser, generated-text compiler path, writable AST, or runtime
  decorator fallback.

## RED / GREEN

The existing generated-field artifact/reflection roundtrip was extended first
to write `generated_field_metadata_roundtrip.zri` and require:

```text
GENERATED_SOURCE_MAPS (1):
type=Meter member=generated originTargetSymbolId=...
sourceLineStart=11 sourceLineEnd=12
```

Against the pre-change writer, the focused GCC executable failed at the first
missing section assertion: `66 Tests 1 Failures`. After adding the isolated
source-map writer and the two orchestration calls, the same executable passed
`66 Tests 0 Failures`.

Review then identified that the public writer still called the legacy
prototype debug printer before the new bounded visitor. Three additional tests
were added for an omitted empty section, deterministic byte-stable ordering of
two rows, and malformed packed prototype payloads. The malformed-payload case
provided a second RED result: `69 Tests 1 Failures`, because the public writer
accepted an overflowing prototype header. After preflight validation was added,
the focused executable passed `69 Tests 0 Failures`.

## Implementation review

`writer_intermediate_generated_source_map.c` bounds-checks the packed prototype
layout before visiting members, validates metadata kinds and ranges, and emits
only complete records. It performs a count-only pass before writing the section
header so ordinary intermediate output remains unchanged when no record exists.
The public writer validates the encoded count, every packed record size, exact
payload consumption, and every nested function before opening the output file.
Malformed, truncated, overflowing, count-mismatched, or trailing payloads
therefore fail without invoking the legacy debug printer or leaving a partial
artifact. The large `writer_intermediate.c` file receives only an include, the
preflight call, and projection calls for root/nested functions.

The first-version `zr.compile.declaration` descriptor intentionally publishes
`GeneratedField` but not `GeneratedType`, `GeneratedMethod`, or
`GeneratedProperty`. Those future variants require separate reference-ledger
admission, production paths, and independent tests; their absence is not an M4
failure.

## Validation matrix

```text
target                         GCC       Clang     MSVC      MSVC ASan
zr_vm_compile_time_test       69 / 0    69 / 0    69 / 0    69 / 0
```

GCC and Clang used isolated WSL source/build snapshots. MSVC 19.44.35228 used
the isolated Windows source snapshot and Ninja build. The sanitizer replay used
the separate static `/Od /fsanitize=address` tree. Every executable returned
zero. The source-map, empty-section, stable-ordering, overflow, and truncated
payload assertions passed in all four runs. Executables were run sequentially
in isolated working directories because the fixtures intentionally use fixed
artifact names.

`git diff --check` reported no whitespace errors; line-ending messages were
only the repository's existing LF-to-CRLF checkout warnings.

## Decision

Gate 11 M4 is `proven` for the published first-version GeneratedField-only
contract. Gate 11 remains open at M5 for compiler sandbox/content-hash handoff,
persistent incremental cache integration, formatter projection, and remaining
consumer/reference-fixture acceptance.
