# Syntax 11 M5 persistent cache snapshot foundation acceptance

Date: 2026-08-02

Scope: deterministic serialization and atomic restoration of the in-memory
CompileTime result cache. This record does not claim project/CLI disk
persistence, clean/incremental artifact equality, or Gate 11 M5 promotion.

## Accepted contract

- Cache entries retain and compare the complete canonical 32-byte digest.
- Export uses versioned `ZRCCV005` magic, a fixed-width header and records,
  big-endian integer fields, and complete-digest ordering. The header carries a
  full SHA-256 over the canonical prefix and records, so equivalent caches
  produce identical bytes regardless of insertion order and any payload bit
  change is rejected.
- Null, bool, signed integer, unsigned integer, and floating native scalar
  values are canonicalized before storage. Negative signed values roundtrip
  without relying on implementation-defined unsigned-to-signed conversion.
- Import requires exact magic, byte count, full snapshot digest, supported
  value type/payload, and strictly increasing entry-digest order. Truncated,
  corrupt, duplicated, unsorted, payload-mutated, or trailing data fails closed.
- Import validates the complete snapshot before allocating replacement storage.
  Failure leaves the existing cache unchanged; success replaces it atomically.
- Snapshot memory is owned by the parser allocator and released through the
  matching public free API.

## TDD and review evidence

The initial strict GCC compile failed because
`zr_vm_parser/comptime_cache.h` and the snapshot APIs did not exist. The first
GREEN added deterministic export/import and passed the focused runtime
contract. Review then found implementation-defined signed payload decoding,
missing cache-array layout validation, and missing whole-snapshot integrity.
The corrected implementation decodes negative values arithmetically, validates
the array contract, authenticates the complete snapshot, and extends the test
with insertion-order independence, negative roundtrip, corrupt magic,
payload-bit mutation, truncation, trailing-byte rejection, and failed-import
transaction preservation.

The final direct WSL GCC 11.4 exact-object replay passes:

```text
test_comptime_cache_snapshot_is_atomic_and_byte_stable:PASS
14 Tests 0 Failures 0 Ignored
```

The 14-test executable also preserves the existing evaluator budget,
diagnostic, cache-key, CompileTool artifact-resolution, and runtime-isolation
cases. Production and test sources compile under strict `-Wall -Wextra
-Wpedantic -Wstrict-prototypes -Wmissing-prototypes` flags.

Final review found that the first snapshot test used three non-exported
internal cache helpers and therefore failed to link under an MSVC shared-parser
build. The test now constructs public cache entries directly and exercises only
the public Export/Import/Free boundary. Fresh MSVC 19.44 Debug shared-library
build and execution pass 14/14; the final isolated WSL GCC replay also passes
14/14.

## Follow-up

The same-day CLI follow-up is recorded in
`2026-08-02-syntax-11-m5-cli-persistent-comptime-cache.md`. It closes ordinary
project load/store, atomic on-disk replacement, corrupt-cache repair, cache-hit
evidence, and byte-identical `.zro` output. External CompileTool provider
activation and the remaining consumers are still open.

Gate 11 M5 therefore remains `indirect`.
