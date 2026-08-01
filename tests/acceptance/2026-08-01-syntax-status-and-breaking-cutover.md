---
scope:
  - docs/plans/syntax leaf status records
  - production parser breaking cutover
  - syntax migration inventory
  - typed metadata and test harness consumers
status: accepted-in-scope
last_verified: 2026-08-01
---

# Syntax status records and breaking cutover acceptance

## Decision

The 55 leaf status records under `docs/plans/syntax` are complete in their
declared scope, and the one-time breaking parser cutover is accepted. This is
not a root Syntax redesign completion claim. The syntax-reference manifest
still contains 13 `design-pending` entries, so 07B and the dependent root
promotion remain open.

The production parser does not preserve the removed percent-prefixed forms as
legacy semantics. It recognizes their spellings only long enough to emit the
structured `legacy_syntax_removed` diagnostic and returns no legacy AST or
lowering. This applies to `%module`, `%compileTime`, `%extern`, `%test`,
`%owned`, `%import`, ownership forms such as `%borrow/%loan/%unique/%shared`,
`%func`, and removed percent type qualifiers. The ordinary remainder operators
`%` and `%=` remain valid. A separate `%` delimiter in intermediate declaration
text is an internal artifact format and is not source syntax.

## Status record census

The recursive leaf selection produces exactly 55 records and every selected
record has a completion marker. Raw historical spellings are intentionally not
normalized:

| Status spelling | Count |
|---|---:|
| Chinese `已完成` | 20 |
| Chinese `已完成（M2 晋级门）` | 1 |
| Chinese `已完成（M3 晋级门）` | 1 |
| Chinese `已完成（M4 单 mutator runtime/bridge 晋级门）` | 1 |
| Plain `completed` | 16 |
| Backticked `completed` | 12 |
| Backticked M4-qualified `completed` | 1 |
| Three distinct `completed_with_known_*` qualifiers | 3 |
| **Total** | **55** |

The 29-entry syntax-reference coverage manifest remains deliberately split as
15 `current`, 1 `negative`, and 13 `design-pending`. Therefore leaf completion
must not be promoted into a false root completion statement.

## Review closure

This pass closed the following correctness gaps before acceptance:

- official provider registration accepts only the identical canonical
  descriptor pointer and rejects a distinct descriptor with equal fields;
- type inference fixtures use the imported canonical declaration alias instead
  of the removed compile-time declaration Patch spelling;
- corrupt TestManifest count decoding no longer reaches invalid cleanup;
- LSP CompileTool projection carries and enforces `providerPhase` and
  `publicContractHash`;
- TestManifest storage uses the existing function ABI extension slots, avoiding
  an appended-structure ABI and generic AOT invalid-free regression;
- ordinary, lambda, meta, and class-member parameters receive declaration-range
  canonical symbol identities, including distinct identities for repeated
  parameter names in different declarations.

No remaining P1/P2 issue was found in the staged Syntax scope. The three full
CTest failures listed below are concurrent Debug ownership, not accepted as
Syntax regressions and not waived as a globally clean suite.

## Migration inventory

The final inventory was generated from files exported from the staged Git
index, excluding unrelated unstaged work. It is deterministic at 890 scanned
files, 420 structured exclusions, 3 allowlisted negative examples, and 645
findings. All 645 findings require semantic review; none is parser acceptance:

| Classification | Count |
|---|---:|
| `machineApplicable` | 0 |
| `maybeIncorrect` | 0 |
| `blocked` | 0 |
| `targetNotPromoted` | 0 |
| `unknown` | 0 |
| `requiresReview` | 645 |

The target-plan distribution is 04=1, 05=4, 06A=615, and 14=25. The three
allowlisted negative spellings are `%future` in the percent cutover test and
`%mutex/%atomic` in task runtime negative coverage.

## Fresh validation

Both isolated staged-index builds completed successfully on Ubuntu 22.04:

| Toolchain | Configure/build | Registered CTest | Full CTest | Syntax-focused CTest | Focused executables |
|---|---|---:|---:|---:|---:|
| GCC 11.4 | pass | 124 | 121/124 | 7/7 | 10/10 |
| Clang 14 | pass | 124 | 121/124 | 7/7 | 10/10 |

The seven focused CTests are `percent_test_migration`,
`percent_syntax_cutover`, `syntax_reference_v1`, `testing_reference`,
`cli_syntax_migration`, `legacy_migration`, and
`official_provider_convergence`. The direct focused executables cover test role
binding, TestManifest roundtrip, testing assertions/runner, compile time,
attribute/comptime/declaration transforms, and type inference.

The same three externally owned Debug groups failed under both compilers:
`debug_truncation`, `debug_variable_child_shape`, and `debug_library`. Their
source changes are outside this staged commit and were left untouched. Thus the
accurate result is: Syntax scope accepted with relevant gates green, while the
repository-wide suite is 121/124 rather than globally clean.

## Final boundary

Removed syntax may remain in migration code, negative fixtures, diagnostics,
and historical plans. Such text is evidence for rejection and migration; it is
not a compatibility path. Production `%` keyword dual-track parsing must not be
restored. Root promotion remains blocked on the 13 owner-gated reference entries
and the open upper-gate work recorded in the Syntax upper-gates ledger.
