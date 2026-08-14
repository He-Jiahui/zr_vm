# Syntax 55 Status Records Revalidation

## Status

- Review date: 2026-08-14 (UTC+08:00)
- Status: `validated_pending_full_acceptance`
- Scope: the historical 55 milestone records under `docs/plans/syntax`
- Relationship to the current tree: `docs/plans/syntax` now contains 91 Markdown
  files recursively. The extra files are top-level designs, implementation plans,
  and later support records; they do not change the historical 55-record selector.

## Mechanical selector

The selector is the same one used by
`tests/acceptance/2026-07-26-syntax-55-status-records-review.md`:

1. Take direct child directories of `docs/plans/syntax` whose names start with a
   digit.
2. Recursively collect Markdown files.
3. Exclude `*-implementation-plan.md`.
4. Exclude the independent support record
   `05-property-unified-ast/m5-task4-property-import-bootstrap.md`.

The 2026-08-14 result is:

```text
TOTAL=55
COMPLETE=55
MISSING_STATUS=0
MISSING_TIME=0
01=5 02=6 03=5 04=7 05=6 06=2 07=1 10=5 12=15 13=3
```

Every selected record still states `completed` or `已完成` and includes a
completion time. Historical qualifiers such as
`completed_with_known_baseline_failures` remain part of the original record;
this review does not rewrite their claimed scope.

## Current contract review

- Production parser source has no literal parsing branch for the removed
  `%module/%import/%extern/%compileTime/%test/%owned/%borrow/%loan/%unique/`
  `%shared/%weak/%func` forms. Migration code and negative tests retain them as
  rejected input.
- Ownership control now has one source surface:
  `share(owner)`, `degrade(shared)`, `wake(weak)`, `intoGc(owner)`, and
  `drop(owner)`.
- Real member lookup runs before a removed-ownership-member migration diagnostic
  is considered. Same-name user object methods keep ordinary member semantics;
  the former `Module.share()` guard escape is rejected because its payload is a
  scoped plain view rather than an ownership operand.
- `.` and `?.` are receiver-target operations. Direct absent weak/nullable
  access raises `NullReferenceError`; optional access skips the complete guarded
  suffix and returns `null` or performs a void no-op.
- The 19 obsolete `tests/fixtures/scripts` files, 19 orphaned `tests/golden`
  files, and the stale exception-analysis note have no active CMake or script
  references and are removed by this work.
- Current generated AST, `.zri`, and `.zro` snapshots belong under each build
  directory's `tests_generated/`, not in a tracked orphan-golden tree.

## Fresh evidence so far

GCC 11.4, Clang 14, and MSVC Debug direct execution passed the expanded review
matrix:

| Target | Result |
| --- | --- |
| ownership intrinsic/member separation | 24/24 |
| removed percent syntax cutover | 7/7 |
| owner/borrow receiver | 7/7 |
| semantic facts | 14/14 |
| type inference | 122/122 |
| resource Unique/Drop | 20/20 |
| resource Shared/Weak | 11/11 |
| SemIR pipeline | 13/13 GCC/Clang; 12/12 MSVC |
| exception runtime | 8/8 |
| AOT ownership contracts | 1/1 |
| AOT ownership shared-library runtime | 2/2 GCC/Clang; ignored MSVC |
| AOT receiver guard C and LLVM | 2/2 GCC/Clang; ignored MSVC |

The added 24th ownership case proves `liveWeak?.add?.(bump())` evaluates its
argument exactly once on the live path and skips it entirely after weak expiry.
The runtime direct path continues to raise and catch the named
`NullReferenceError`.

Final code review found one Important issue and no Critical issues. The issue
was the semantic acceptance of consuming field/index projections even though
the compiler could only clear local bindings. Commit `2802fcb` rejects those
unlowerable projections, preserves non-consuming projection reads, and passes
GCC/Clang/MSVC ownership 23/23, semantic facts 13/13, and type inference 122/122.

The latest production-source audit finds no parser branch accepting the removed
percent-prefixed declarations, imports, ownership forms, extern/comptime forms,
or type qualifiers. The remaining percent spellings are rejected migration
input, diagnostics, historical records, or the ordinary modulo operators. Real
member lookup runs before the old ownership-member migration diagnostic, so
user methods named `wake`, `degrade`, `intoGc`, `drop`, or `share` remain normal
`GET_MEMBER`/call behavior.

The focused AOT replay found and corrected stale scalar provenance at two
boundaries: a copied result now materializes its frame slot whenever a later
generic consumer needs it, and a current-block different-kind overwrite blocks
fallback to a historical block-entry kind. The corrected typed-u64 AOT harness
also no longer keeps an unrooted raw function pointer across a safepoint;
Valgrind reports zero errors for its 25/25 run.

ASan+UBSan passes the provider suite 9/9. LeakSanitizer retains a known core
permanent-string/builtin-closure baseline, but reports no native registry or
`native_binding.c` allocation after global teardown.

The ownership/member-separation target also passes 24/24 in a separate GCC
Debug cache built with ASan+UBSan and frame pointers. Leak detection was disabled
to isolate executable memory-safety and undefined-behavior evidence; neither
sanitizer reported an error.

## Pending completion gates

- The prior Clang full build was 1460/1460 and CTest 124/126; only the two active
  LSP callable-value suites failed. This is useful historical evidence but does
  not satisfy the final stable-HEAD whole-repository gate.
- Syntax migration inventory is 9/10 until the repository golden is regenerated
  on the final stable tracked baseline; the scanner's other nine protocol and
  classification checks pass.
- Rebuild and rerun the affected LSP targets after the concurrent external
  callable-value fact work exact-commits its parser/LSP overlay.
- Run full GCC, Clang, and MSVC CTest matrices and CLI smoke.
- Regenerate and verify the repository migration-inventory golden.
- Revalidate AOT C/LLVM fixtures and generated artifacts.
- Complete the requirement-by-requirement review, exact-path staging audit, and
  generated build/log cleanup.

Only after these gates pass may this record be promoted to `completed` and the
55/55 leaf set be described as freshly accepted on the final commit baseline.
