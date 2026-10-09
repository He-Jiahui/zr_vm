---
related_code:
  - tests/parser/test_ssa_source_returned_retention.c
  - tests/parser/support/ssa_source_returned_retention_fixture.c
  - tests/parser/support/ssa_source_returned_retention_fixture.h
  - tests/parser/support/ssa_source_returned_descriptor_shim.h
  - tests/parser/support/ssa_source_returned_owner_shim.h
  - tests/cmake/ssa-source-returned-retention.cmake
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_retention.c
implementation_files:
  - tests/parser/test_ssa_source_returned_retention.c
  - tests/parser/support/ssa_source_returned_retention_fixture.c
  - tests/parser/support/ssa_source_returned_retention_fixture.h
  - tests/cmake/ssa-source-returned-retention.cmake
plan_sources:
  - .codex/plans/20261008-ssa-function-retention-next-finite-v4.md
  - .codex/plans/20261009-ssa-source-returned-retention-source-tests-design.md
tests:
  - tests/parser/test_ssa_source_returned_retention.c
  - docs/acceptance/ssa-source-returned-retention.md
doc_type: test-documentation
status: finite-green-accepted
---

# Real returned source retention tests

## Construction and ownership

`tests/parser/test_ssa_source_returned_retention.c` runs exactly 25 default
Unity cases. Its dedicated fixture constructs real Global/State objects,
compiles through public source entrypoints and inspects returned Functions
after compiler/AST cleanup. No manual CompilerState fixture stands in for the
returned route. Optional host callbacks and external loader userdata are NULL;
source names are bounded relative basenames. GC stopping/root registration is
scoped to the fixture and restored at actual Global destruction.

A bounded VM ledger checks exact original allocation size and flag, including
final shutdown balance. Separate bounded CRT ledgers intercept only actual
descriptor calloc/free and Core arena malloc/free translation units. No wrapper
allocates bookkeeping, asserts or prints. Faults arm after name construction
and disarm before assertions/retry/teardown. Numeric record/free witnesses
survive release without comparing or reading freed pointers.

Descriptor partial release is checked immediately after the real wrapper
returns, before fallback teardown. If production Function/Global cleanup leaves
a CRT allocation, the fixture records an error before recovering the storage;
recovery cannot turn a leak into a pass. Failed late compilation can leave
module summaries, so immediate equality of VM live counts is not required;
exact VM balance at final shutdown is required.

## Exact inventory

| Cases | Purpose |
| --- | --- |
| 1–6 | Ordinary NONE9, zero-options8, NULL-options9, legal bool, legal float and incomplete-expression parse refusal prerequisites |
| 7–8 | REQUIRE9/8 returns independent metadata after real wrapper cleanup |
| 9–11 | Unknown enum, NULL state/source and zero length; case11 also checks original invalid legacy cache reset |
| 12 | Actual PARSE/PARSE_FAILED and bounded positive callback range with no lower record |
| 13–14 | Exact real bridge CONSTANT/UNSUPPORTED for bool/float, complete copied lower payload and no fabricated range |
| 15 | Optional NULL diagnostic preserves retention behavior |
| 16–17 | Two independent live owners; explicit free and actual Global shutdown release each once |
| 18 | Exactly16 alternating compile/free calls; bounded repetition, no race/stress claim |
| 19–24 | Six actual descriptor allocation sites fail once, exact byte OOM, partial cleanup before diagnostic/retry, owner allocator unhit |
| 25 | Actual final owner-arena malloc OOM, temporary producers cleaned, disarmed clean retry |

I64 prerequisites require actual finalized identity/metadata. Bool/float require
legal returned Functions and their actual native constant tags; their ordinary
profile does not promise I64 identity or SCRIPT-entry metadata.

Returned metadata assertions cover owner association, module/entry token,
hash/generation, noargsI64 ABI, literal bits, source maps, compacted frame/layout
and value slots, and present-empty logical state maps. Diagnostics compare all
public scalar fields and both lower payloads, including absent-field zeros.
Parse coordinates are compared with the observed callback record and bounded
against the actual input; exact lexical token positions are not claimed.

## Actual build route

Both normal and focused direct registrations use a dedicated CMake module.
The current native route compiles the actual E:/Git/zr_vm checkout, reuses
E:/cargo-targets/zr_vm/build and puts logs/maps/receipts under the configured
E report root. No source/header snapshot or worktree is created.

The selected executable links explicit static Parser/Core/Library/Common,
their required vendors and Unity. Private descriptor and arena OBJECTs precede
ordinary archives; forced includes occur only on those two objects. Current
source, headers, graph, settings and artifacts are pinned before/after. CTest
selects only `^ssa_source_returned_retention$`, timeout120, assertions/UBSan,
profiles0, output/CoreBootstrap/project trace unset. No ignored, WILL_FAIL,
inverted-pass or no-tests fallback is allowed.

RED requires the first six actual prerequisites to PASS and genuine REQUIRE
failures; infrastructure, sanitizer or ledger failures are not accepted RED.
Unused producer symbols can be dead stripped by the refusal stub: RED proves
the actual two private compile/link inputs and live options/helper symbols.
GREEN additionally requires BuildDescriptor/CloneOwnedModule to resolve from
the corresponding private objects. Finite MSVC compile-only validation covers
six current units: helper, compiler wrapper, test, fixture, descriptor and arena.
It does not establish whole-project MSVC or Linux acceptance.

## Evidence

The accepted current replay uses returned GREEN r3 (25PASS/0fail/0ignore) and
dependency r6 (58PASS across four CTest tests), with independent Root raw/map
and current input checks. The refreshed six-unit MSVC GREEN r4 compile gate is the
finite Windows compatibility evidence. Two inherited unused static Parser
diagnostics are disclosed from dependency r3/r5; incremental nonrepeat is not
a warning-free dependency graph claim. Exact receipts are in the acceptance
record below. These gates compile and inspect metadata without executing the
returned Function or generated native code.

Actual finite receipt results are recorded in the
[acceptance record](../acceptance/ssa-source-returned-retention.md).

Historical C r3: 25 cases, 10PASS/15 functional FAIL/0 ignored; the first six
prerequisites pass. Six REQUIRE-return assertions observe the safe NULL refusal,
two non-I64 cases observe COMPILE rather than the expected CONSTANT stage, and
seven fault cases observe zero producer hits. These failures stop before their
GREEN-only cleanup/retry/metadata assertions, so those assertions were not
credited by RED. Configure/build exit0, CTest8; the historical C build and six finite MSVC units
have zero warnings, with no strict ledger or UBSan diagnostic.

D GREEN r2 observed 25PASS/0FAIL/0ignored, including returned metadata,
independent owner lifetimes and all seven allocation-failure cleanup/retry cases.
Configure7732, build34468 and CTest39472 exited0, with no ledger or UBSan
diagnostic. Fresh six-unit MSVC GREEN r3 also exited0 with zero warnings.

Dependency r4 observed 58PASS across four CTest gates: Function12, scalar
constant24, descriptor16 and metadata lifecycle6. Its incremental build emitted
zero warnings; the preceding dependency r3 build has an inherited unused static
`parser_types` warning, so this does not establish a warning-free dependency graph.

Those earlier r2/r4 observations did not establish current provenance. Root's
later proof refused after foreign source changes. The accepted r3/r6 replay
above supersedes that provenance failure; all historical receipts are preserved.
The finite commit follows current source/header/artifact admission, with its
exact transaction recorded in the active session note.
