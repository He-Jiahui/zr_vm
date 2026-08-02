# Syntax 11 M5 CLI persistent comptime cache acceptance

## Scope

This slice connects the deterministic comptime cache v5 snapshot to ordinary
project compilation. It does not activate an external CompileTool provider and
does not promote Gate 11 M5 as a whole.

## Contract

- The project binary root owns one `.zr_comptime_cache` file.
- The CLI reads the complete file before source compilation and passes the
  bytes to `ZrParser_Source_CompileWithComptimeCache`.
- Snapshot import is fail-closed. A missing cache starts empty; malformed bytes
  are counted as rejected and compilation continues from an empty cache.
- Every successfully compiled source contributes to one merged in-memory
  snapshot. The CLI exports it only after the project compile succeeds.
- Each cache key includes the complete current-module source SHA-256. A
  same-length function-body or constant edit therefore misses instead of
  reusing a stale scalar result.
- Persistence uses a sibling temporary file followed by atomic replacement.
  Failed write/replace leaves the previous cache untouched.
- Cache ownership is explicit: file bytes, parser snapshot bytes, and the
  compile record's next snapshot are each freed by the layer that allocated
  them.

## Focused evidence

`zr_vm_cli_project_incremental_test` passes 12/12 under WSL GCC 11.4. Its
persistent-cache case performs four forced source recompilations:

1. no cache file: one compile, zero hits, at least one miss;
2. valid saved snapshot: one compile, at least one hit, zero misses;
3. same-length semantic source edit: one compile, zero hits, at least one miss;
4. restored source plus deliberately corrupted snapshot: one compile, zero
   hits, at least one miss, and one rejected input.

The case removes the `.zro` before each replay so ordinary incremental skipping
cannot masquerade as a comptime cache hit. The first and cache-hit artifacts are
byte-identical; the semantic-edit artifact differs because the source identity
changed; restoring the source after corruption reproduces the first artifact
byte-for-byte. It also verifies that the repaired cache starts with `ZRCCV005`
after the corrupt-input build.

Adjacent fresh evidence in the same isolated build:

- comptime runtime contract: 14/14, including deterministic snapshot import,
  full-digest identity, and CompileTool artifact resolver ownership;
- project manifest v2: 10/10, including atomic project-owned lock admission;
- LSP advanced editor features: zero failures, including formatter fail-closed
  behavior for removed syntax.

## Remaining Gate 11 M5 boundary

The cache persistence requirement is closed for the project CLI. Gate 11 M5
remains indirect until ordinary compile-only import activates and validates an
external provider graph and the remaining artifact/reflection consumers have
their independent acceptance evidence.
