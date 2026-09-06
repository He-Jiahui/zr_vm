---
related_code:
  - zr_vm_language_server/stdio/stdio_lsp_parse.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - tests/language_server/test_stdio_lsp_parse.c
  - tests/cmake/zr_vm_lsp_stdio_parse_tests.cmake
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_lsp_parse
  - language_server_stdio_protocol_conformance
doc_type: plan-record
---

# Plan 01 Task 2 Sub01: Strict Numeric Parsing

## Scope

Started: 2026-09-07 01:05 +08:00

Completed: 2026-09-07 01:34 +08:00

This subitem closes the numeric validation contract for `parse_size_value`,
`parse_position` and `parse_range`. It does not claim completion of the JSON-RPC
envelope or handler status portions of Plan 01 Task 2.

## Root Cause

`TZrSize` is `size_t` and `ZR_MAX_SIZE` is `SIZE_MAX`. On a 64-bit host,
`(double)SIZE_MAX` rounds to `2^64`. The old inclusive comparison therefore
accepted JSON number `2^64` and converted it to `TZrSize`; Clang UBSan reported
the conversion as undefined behavior. The parser also needed an independently
verifiable matrix for fractions, non-finite values, wrong JSON types, position
overflow and reversed ranges.

## RED

The new focused test was first run against the old implementation:

```text
ctest --test-dir .codex/lsp-optimize-validation/clang-asan-current
  -R ^language_server_stdio_lsp_parse$
  runtime error: 1.84467e+19 is outside the range of representable values
  of type 'unsigned long' at stdio_lsp_parse.c:26
```

The first test harness attempt used cJSON's number constructor for NaN and
exposed a separate cJSON integer-cache cast. The harness was corrected to set
`valuedouble` after constructing a finite number, so NaN and infinities reach
the parser under test.

## Implementation

The size parser now computes the exclusive `2^N` boundary as
`(double)(ZR_MAX_SIZE / 2 + 1) * 2.0` before conversion. This is exact for the
supported 32-bit and 64-bit `size_t` widths and leaves the final round-trip
check in place for representable integral values. Position validation retains
the finite, non-negative, integral `INT32_MAX` check. The new test target is
kept in a dedicated CMake fragment and uses real cJSON objects plus malformed
object inputs.

## Validation Evidence

Toolchain versions:

- GCC 11.4.0, Debug shared build: `.codex/build-lsp-opt-gcc`
- Clang 14.0.0, ASan + UBSan, `-no-pie`: `.codex/lsp-optimize-validation/clang-asan-current`
- MSVC 19.44.35228.0, Debug: `.codex/lsp-optimize-validation/msvc`

Focused commands and results:

```text
cmake --build .codex/build-lsp-opt-gcc
  target zr_vm_language_server_stdio_lsp_parse_test: passed
ctest --test-dir .codex/build-lsp-opt-gcc
  -R ^language_server_stdio_lsp_parse$
  1/1 passed

ctest --test-dir .codex/lsp-optimize-validation/clang-asan-current
  -R ^language_server_stdio_lsp_parse$
  1/1 passed; no ASan/UBSan diagnostics

ctest --test-dir .codex/lsp-optimize-validation/msvc -C Debug
  -R ^language_server_stdio_lsp_parse$
  1/1 passed
```

The matrix includes exact zero and signed zero, `INT32_MAX`, the largest
representable `TZrSize` value where the host `double` can represent it, the
rounded `2^N` exclusive boundary, fractions, negative values, NaN,
infinities, `DBL_MAX`, wrong JSON types, missing position members and reversed
range endpoints. Existing protocol conformance remains a parent-layer check;
the focused parser test is the acceptance evidence for this subitem.

## Acceptance Decision

Accepted for Plan 01 Task 2 Sub01. The parser no longer performs the reported
out-of-range conversion, and the focused test passes under GCC, Clang
ASan/UBSan and MSVC. Plan 01 Task 2 and Plan 01 as a whole remain pending for
their other envelope, frame, cancellation and teardown requirements.
