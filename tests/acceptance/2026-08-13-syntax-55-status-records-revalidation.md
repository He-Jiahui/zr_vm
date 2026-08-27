# Syntax 55 Status Records Revalidation

## Status

- Review date: 2026-08-26 (UTC+08:00)
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

The 2026-08-23 result is:

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
  is considered. Same-name user object and module methods keep ordinary member
  semantics and never lower to ownership opcodes; only the former implicit
  module-prototype `.share()` ownership escape is rejected because its payload
  is a scoped plain view rather than an ownership operand.
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

## 2026-08-25 selector and migration preflight

The canonical selector was rerun from the current worktree using the documented
direct-child, implementation-plan, and independent-support-record exclusions:

```text
TOTAL=55
COMPLETE=55
MISSING_STATUS=0
MISSING_TIME=0
01=5 02=6 03=5 04=7 05=6 06=2 07=1 10=5 12=15 13=3
```

Production parser C/H again has zero matches for the removed percent-prefixed
keyword branches. Test registration review has zero disabled `#if 0` blocks and
zero commented `RUN_TEST` calls. The Windows migration-inventory protocol ran
all ten tests: nine passed, while only the deterministic repository-inventory
comparison failed because its tracked golden is concurrently modified against a
still-dirty repository baseline. Classification, UTF-8/LF output, current-syntax
coverage, and the absence of machine-applicable LSP legacy fixtures all passed.
The golden is not regenerated or staged until its current owner releases it and
the final tracked baseline is stable.

## 2026-08-25 legacy test-suite classification

A repository test-source search reclassified every remaining occurrence of the
removed percent surface. Executable `.zr` files containing those forms are
limited to the explicit `syntax_reference_v1/negative` fixture and the
`syntax_migration_frontend` or `syntax_migration_inventory` inputs. In the
inventory's `current_forms.zr` the text occurs only inside a string literal; in
`ignored_forms.zr` it occurs only inside comments. Neither file is a positive
parser-compatibility case.

The few occurrences in general parser or LSP runners are also current
assertions rather than stale success paths:

- `test_parser.c`, `test_compiler_features.c`, and `test_lsp_interface.c`
  require `%unique` or field-scoped percent ownership syntax to fail with the
  directed `legacy_syntax_removed` contract;
- the LSP completion case requires `import` and rejects `%import` as a
  completion label;
- the formatting and CompileTool projection cases require canonical output and
  explicitly fail if `%compileTime`, `%func`, or `%import` is emitted;
- `test_legacy_migration.c`, `test_percent_syntax_cutover.c`, the migration CLI
  smoke, and the migration inventory deliberately retain old spellings as
  inputs to rejection, classification, or structured rewrite tests.

Therefore no current positive test suite depends on accepting the removed
percent language. These negative and migration fixtures are retained because
deleting them would remove proof of the one-time breaking cutover rather than
modernize a stale success expectation.

The current first-party C/H test tree contains 165 `TEST_IGNORE_MESSAGE` call
sites. A structural scan found a nearby explicit `#if` or `#elif` capability
branch for all 165 (`WITHOUT_NEAR_IF=0`). Their messages and guards cover Unix
shared-library/private-symbol execution or the Windows static-CRT `errno`
boundary. The one increase from the earlier 164-site audit is the concurrent
readonly-aggregate ExecIR test, which is guarded because its private validation
symbols are not exported by the Windows parser DLL. No ignore is used to hide a
portable current-language failure.

## 2026-08-26 test-runner and inventory review

A broader first-party custom-runner audit found six executables that could
print an internal failure and still return zero: incremental parser,
instructions, symbol table, reference tracker, LSP language feature matrix, and
LSP project features. The first five are corrected by `9f784b1`, `11f5f97`, and
`da2fd81`. Controlled incremental-parser and Unity failures now return one;
restored GCC/Clang/MSVC runs return zero with no failure markers. The instruction
suite reports `95 Tests / 0 Failures / 0 Ignored`, and the language feature
matrix reports `0 failure(s)` on all three toolchains.

`test_lsp_project_features.c` remains frozen because it is an active L8
provider/binary callable test path. It still needs the same counted `TEST_FAIL`
and nonzero-return repair after that exact path is released. Consequently the
final 134-test CTest graph is not yet accepted even if an intermediate run
reports green.

The complete Windows migration-inventory protocol was rerun after these test
changes. Nine of ten cases pass in 118.747 seconds; the sole failure remains the
deterministic repository golden comparison. Protocol classification, embedded
source scanning, UTF-8/LF output, current syntax reference coverage, and the
absence of machine-applicable LSP legacy fixtures all pass. The golden is not
regenerated on the still-moving tracked baseline.

The canonical selector was rerun again on 2026-08-26 and remains unchanged:

```text
TOTAL=55
COMPLETE=55
MISSING_STATUS=0
MISSING_TIME=0
01=5 02=6 03=5 04=7 05=6 06=2 07=1 10=5 12=15 13=3
```

Content review found one stale current-contract sentence in the completed 04-M3
record: it still said Weak direct calls were rejected in favor of an explicit
`upgrade`, and described `share` through the former `OWN_CONSTRUCT + SHARE`
route. The record now reflects the implemented chain-level Weak receiver guard,
named direct-access error, dedicated `OwnershipIntrinsicFact`, and canonical
PlaceId contract. The old `%upgrade` text that remains in the 06 implementation
plan is an intentional migration input, and M7's historical ownership opcodes
remain explicitly scoped to artifact-reader compatibility rather than source
semantics.

The completed Syntax 05 M3 record was also refreshed after the focused optional
access review. It now records whole-suffix skipping for computed indices and
property getters, single receiver evaluation, hidden Weak owner lifetime, and
the direct-access `NullReferenceError` boundary. This is a current-contract
documentation correction, not a change to the canonical 55-record selector or
to any historical migration fixture.

The Copilot-reported Windows unresolved externals were also checked against the
current CMake graph rather than addressed speculatively. The debug expression
diagnostics target already links `zr_vm_debug_shared` and its network dependency,
while every language-server support target, including the LSP interface target,
passes through `zr_vm_link_language_server`. A fresh short-path MSVC 19.44 shared
Debug cache configured and linked both executables without a CMake change.
Direct execution returned zero with debug diagnostics `56 Tests / 0 Failures /
0 Ignored` and the complete LSP interface suite passing. This closes the two
reported linker failures on the current shared-library configuration; it does
not replace the final static three-toolchain CTest gate.

## 2026-08-27 stale-runner and language-surface follow-up

The canonical selector was executed again and remains exactly:

```text
TOTAL=55
COMPLETE=55
MISSING_STATUS=0
MISSING_TIME=0
01=5 02=6 03=5 04=7 05=6 06=2 07=1 10=5 12=15 13=3
```

Two previously restored parser runners were still capable of printing failures
while returning a green Unity result. Controlled failure propagation exposed
instruction execution at 31/3 and lexer/parser/compiler execution at 14/3.
Their stale top-level opcode assumptions were corrected to the current typed,
optimized, and nested-function contracts. GCC, Clang, and MSVC now directly
pass 31/31 and 14/14. Five additional clean legacy runners now propagate custom
failures into Unity and pass on all three toolchains: exceptions 8/8, named
arguments 10/10, instructions 95/95, meta 41/41, and module system 78/78.
The permanent `test_log_failure_contract` CTest also passes 1/1 on all three: it
requires an intentional probe to return exactly one Unity failure while still
printing `cleanup_reached=1` after the failure macro.

The root parser runner has now joined the truthful set. A controlled early
return after its private failure macro reproduced `74 Tests / 0 Failures /
0 Ignored`, exit zero, despite an explicit failure marker. The runner now
delegates to the probed shared harness without changing its 31 call sites.
GCC, Clang, and MSVC each directly pass the normal runner at 74/74, while the
same bounded intentional probe reports 74/1 and exits 1 on all three.

The remaining non-truthful custom runner set is intentionally not declared
clean yet. `test_semir_pipeline.c` and `test_type_inference.c` remain frozen by
the concurrent L8 parser exact-test window, while the already recorded LSP
project-feature runner is frozen by the same callable-value work. They must be
repaired and directly replayed after release before final graph acceptance.

The production-language scan independently returns zero occurrences for all 12
removed percent spellings and zero ownership-member lowering classifier
references. The lexer reserves exactly the five ownership intrinsic names, and
ordinary member lookup has priority over the post-failure migration diagnostic.
Thus compatibility text for `upgrade`, `weak`, or an old member call is not an
executable second language. Fixed pre-L8 GCC, Clang, and MSVC CLI smokes each
printed `hello world` and exited zero; the final stable-HEAD replay remains open.

The canonical count now has an executable verifier at
`scripts/syntax_status_records.py`, with focused tests in
`tests/scripts/test_syntax_status_records.py`. The TDD RED was the missing
verifier module. GREEN directly passed four tests covering the exact numbered
directory selector, both exclusions, English/Chinese/qualified completion
markers, required completion times, the real repository, and a negative drift
case. Direct CLI execution returned zero with:

```text
TOTAL=55
COMPLETE=55
MISSING_STATUS=0
NON_COMPLETE=0
MISSING_TIME=0
01=5 02=6 03=5 04=7 05=6 06=2 07=1 10=5 12=15 13=3
```

This removes the manual recount as a future source of false confidence. It does
not replace the pending stable-HEAD compiler, artifact, migration-inventory, or
three-toolchain gates below.

## Pending completion gates

- Clean detached GCC 11.4, Clang 14, and MSVC 19.44 Debug builds at intermediate
  baseline `0a46151` each passed 133/133 registered CTests, and all three CLI
  smokes printed `hello world` with exit code zero. This is a fully green
  pre-L8 baseline, not the final stable-HEAD claim, because the external
  callable-value parser/LSP overlay remains unreleased.
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
