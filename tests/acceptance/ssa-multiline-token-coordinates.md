---
related_code:
  - tests/parser/test_ssa_source_range_identity.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - tests/parser/test_ssa_source_range_identity.c
plan_sources:
  - .codex/plans/20261005-ssa-multiline-token-coordinates.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_ssa_source_range_identity.c
  - docs/acceptance/ssa-multiline-token-coordinates.md
doc_type: testing-guide
status: finite-multiline-token-coordinate-green
---

# Multiline Token Coordinate Test Guide

## Fixture and independent assertions

Target `zr_vm_ssa_source_range_identity_test` and CTest
`ssa_source_range_identity` reuse the existing fixture and registration.
Default execution runs 17 cases; `--baseline-only` runs the unchanged original
13. There are four added `test_multiline_template_token_coordinates_*` cases:

| Suffix | Exact source in escaped notation |
| --- | --- |
| `bare_cr` | ``return `ab\rcd`;\r`` |
| `lf` | ``return `ab\ncd`;\n`` |
| `crlf` | ``return `ab\r\ncd`;\r\n`` |
| `mixed_newlines` | ``return `a\rb\r\nc\nd`\r\n;\n`` |

Each source is first actually parsed: no ordinary/fatal error, zero diagnostics,
EOS, one RETURN in SCRIPT, TEMPLATE_STRING_LITERAL expression and nonempty
segments. A separately owned state scans the same source using lookahead/Next.
RETURN and template coordinates survive lookahead; raw token, semicolon and
EOS slices and fixed byte/line/column expectations are asserted. The expected
coordinates are [listed in the module contract](../../docs/parser-and-semantics/ssa-multiline-token-coordinates.md).
The AST check establishes shape only. Template AST locations are not asserted.
Both state slots and the AST use the fixture's existing global teardown owners.

## Actual RED and GREEN

At RED commit `44e5ab10240a1720c842bcf95d91fe5f3f42823f`, configure/build and
the 13-case baseline exited 0. Full CTest exited 8: 17 cases, two failures,
zero ignored. Bare CR end line was Expected 2 / Was 1; mixed was Expected 4 /
Was 3. LF/CRLF controls and the parse/scan/semicolon/EOS prerequisites passed.
No UBSan diagnostic was observed.

After the bounded producer repair, configure/build/CTest exited 0: 17 range
cases passed alongside 153 Unity consumer cases and standalone dominator CFG.
The [acceptance record](../../docs/acceptance/ssa-multiline-token-coordinates.md)
owns receipt hashes, log and executable identities, suite counts and the MSVC
argument recovery. Tests compiled the direct checkout with no source snapshot.
The local environment is Windows x64 clang-cl 19 Debug UBSan, with
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.

## Reproduction boundary

Use the configured direct driver at
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`.
The GREEN receipt retains exact configure, build targets and CTest arguments.
The fixture executable accepts `--baseline-only` for the historical baseline;
ordinary execution and focused CTest include all 17 cases.

Span-final CR has a static bounded-read argument, with no independent dynamic
fixture here. WSL/Linux GCC/Clang remains OPEN from existing E_ACCESSDENIED;
full MSVC acceptance remains OPEN despite the changed TU compile-only success.
Template AST coordinates, offset-cache/snippet CR handling, full parser/OOM,
descriptor/native execution, retention and full SSA47 remain OPEN. This gate
adds local parse/scan evidence and no network, FFI, provider or security tests.
