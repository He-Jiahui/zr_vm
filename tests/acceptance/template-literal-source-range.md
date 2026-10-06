---
related_code:
  - zr_vm_core/include/zr_vm_core/gc.h
  - tests/parser/test_template_literal_source_range.c
  - tests/parser/test_expression_fragment_parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-execbc-vm.cmake
implementation_files:
  - tests/parser/test_template_literal_source_range.c
plan_sources:
  - .codex/plans/20261005-ssa-template-ast-ranges.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_template_literal_source_range.c
  - docs/acceptance/template-literal-source-range.md
doc_type: testing-guide
status: finite-template-ast-root-range-green
---

# Template Literal Source Range Test Guide

## Registration and fixed expectations

Target `zr_vm_template_literal_source_range_test` and CTest
`template_literal_source_range` compile the direct checkout. Default execution
runs six cases; `--prerequisites-only` runs those same cases, omitting only the
final AST root coordinate check. The normal CTest has no `WILL_FAIL`.

| Case | Exact source in escaped notation | Root bytes | Start → exclusive end |
| --- | --- | --- | --- |
| single line suffix | ``return `x`;return 9;`` | `[7, 10)` | 1:8 → 1:11 |
| LF suffix | ``return `ab\ncd`;\nreturn 9;`` | `[7, 14)` | 1:8 → 2:4 |
| CRLF suffix | ``return `ab\r\ncd`;\r\nreturn 9;`` | `[7, 15)` | 1:8 → 2:4 |
| bare CR suffix | ``return `ab\rcd`;\rreturn 9;`` | `[7, 14)` | 1:8 → 2:4 |
| EOF simple fragment | `` `x` `` | `[0, 3)` | 1:1 → 1:4 |
| EOF interpolated fragment | `` `a${1}b` `` | `[0, 8)` | 1:1 → 1:9 |

The first four use `ZrParser_ParseTopLevelStatementWithState` twice. They check
two RETURN nodes, template type/nonempty segments and integer 9 in the second
statement. The last two use `ZrParser_ParseExpressionWithState`, checking one
string segment `x`, or exactly three segments: string `a`, an interpolated
integer expression 1, and string `b`.

All cases check no ordinary/fatal parser error, no lexical error, zero callback
diagnostics and EOS. A separately owned parser scans the original source using
actual Next calls and verifies the raw token bytes and fixed coordinates.
Expected root coordinates are literal constants, not derived from observed
AST or scanner coordinates. Segment child ranges are not asserted.

## Teardown and prerequisite correction

Fixture global owners retain ASTs, both parser initialization slots, source
name and runtime state across Unity assertion failure. Teardown frees ASTs,
then both parser states, then unroots the source name through
`UnignoreObject(g_state->global, ...)`, then destroys the runtime state.
`IgnoreObject` requires `SZrState *` and receives `g_state`; its distinct
`UnignoreObject` interface requires `SZrGlobalState *`.

The first uncommitted fixture used the wrong IgnoreObject owner and halted in
UBSan before the range gate. That failure is retained as a prerequisite
failure, not AST RED. After the one-line owner correction, prerequisites
passed 6/6, range/expression-fragment consumers passed 32 cases, and full
six-case CTest failed each final AST start offset. The
[acceptance record](../../docs/acceptance/template-literal-source-range.md)
owns exact receipts, source pins and actual GREEN evidence: six template,
15 fragment and 17 token cases passed, with zero failures or ignored cases.

## Reproduction and limits

The direct configured build is
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/metadata-guards-direct-v2`.
The executable is `bin/zr_vm_template_literal_source_range_test.exe`.
Run it with `--prerequisites-only`, then run focused CTest with
`-R ^template_literal_source_range$ -V --no-tests=error` for the full gate.
The RED receipt owns exact configure/build arguments: Windows x64 clang-cl 19
Debug assertions and UBSan, `halt_on_error=1:print_stacktrace=1`.
No source snapshot was created.

Segment coordinates, OOM/GC stress, Linux/full MSVC matrices, offset-cache and
snippet handling, source execution, descriptor/native/retention acceptance
and SSA47 remain OPEN. The new cases supply local parse/scan evidence only.

## Separate host AOT target successor

The finite template AST root gate completed at
`a986da50b7d68ada4b17e175eaed99d99c40f9b0`. Its historical proof above remains
frozen. The independent [host no-argument I64 AOT target gate](ssa-host-aot-target.md)
consumes canonical callable and actual host-row identity; its separate finite
48-case Windows gate passed after actual RED. AST root acceptance does not establish the
target contract, descriptor binding, backend execution or retention.
