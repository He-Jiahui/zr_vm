---
related_code:
  - zr_vm_language_server/stdio/stdio_lsp_parse.c
  - tests/language_server/test_stdio_lsp_parse.c
  - tests/cmake/zr_vm_lsp_stdio_parse_tests.cmake
implementation_files:
  - zr_vm_language_server/stdio/stdio_lsp_parse.c
plan_sources:
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_lsp_parse
doc_type: acceptance-record
---

# LSP Stdio Numeric Parsing

## Scope

The stdio parser now rejects non-integral, negative, non-finite and
out-of-range JSON numbers before converting them to `TZrSize` or `TZrInt32`.
Ranges also reject reversed endpoints. The affected layers are the stdio LSP
parser and its focused C test target.

## Baseline

On 64-bit hosts the previous `ZR_MAX_SIZE` comparison converted `SIZE_MAX` to
`double`, rounded it to `2^64`, and allowed `2^64` to reach an unsigned integer
conversion. Clang UBSan reproduced the undefined behavior. The existing JS
protocol test covered only fractional, negative and 32-bit position examples.

## Test Inventory

- `language_server_stdio_lsp_parse`: exact zero and signed zero, position and
  size upper boundaries, rounded `2^N` size boundary, fractions, negatives,
  NaN, infinities, `DBL_MAX`, wrong JSON types, missing fields and reversed
  ranges.
- `language_server_stdio_protocol_conformance`: retained as the parent
  protocol check for invalid position and range responses.

## Tooling Evidence

- GCC 11.4.0 Debug shared build in `.codex/build-lsp-opt-gcc`.
- Clang 14.0.0 with ASan and UBSan in
  `.codex/lsp-optimize-validation/clang-asan-current`.
- MSVC 19.44.35228.0 Debug build in `.codex/lsp-optimize-validation/msvc`.

The focused CTest selection passed `1/1` under each toolchain. The pre-fix
Clang UBSan run failed at the old `TZrSize` conversion; the post-fix run passed
without sanitizer diagnostics.

## Results

The production parser computes an exact exclusive `2^N` size bound before the
cast, then retains its integral round-trip check. Position parsing continues to
use the finite `INT32_MAX` bound. The focused regression is registered through
`tests/cmake/zr_vm_lsp_stdio_parse_tests.cmake`.

## Acceptance Decision

Accepted for the Plan 01 Task 2 numeric parsing subitem. The broader stdio
protocol, frame, cancellation and teardown gates remain open.
