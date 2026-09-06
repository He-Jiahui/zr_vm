---
related_code:
  - zr_vm_language_server/stdio/stdio_frame_reader.h
  - zr_vm_language_server/stdio/stdio_frame_reader.c
  - tests/language_server/stdio_protocol_conformance.js
  - zr_vm_language_server/stdio/stdio_transport.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
doc_type: plan-record
---

# Plan 01 Task 3 Sub01: Header Exactness

## Scope

Started: 2026-09-07 02:18 +08:00

Completed: 2026-09-07 02:29 +08:00

This submilestone hardens the existing bounded frame reader at the byte and
`Content-Type` parameter boundaries. It does not claim completion of the Task 3
parent limits, transport or lifecycle gates.

## RED

The new conformance cases were run against the previous MSVC Debug binary:

```text
NUL in content length: expected MALFORMED_HEADER, stderr=
```

The old reader exited non-zero without the required malformed-header signal
because the embedded NUL truncated the C string used for header parsing. A
second probe showed that a header containing both `charset=utf-8` and
`charset=utf-16` was accepted because the parser returned after the first
charset parameter.

## Implementation

`ZrLanguageServer_StdioFrameReader_Read` now rejects a NUL byte immediately
while reading the header block, before storing or searching the line as a C
string. `frame_reader_content_type_is_utf8` walks every semicolon-delimited
parameter. Explicit `charset` values accept only `utf-8`/`utf8` (with the
existing quoted-value form); any other or conflicting explicit value is
malformed. Unknown parameters remain ignored and still count through the
existing header-size accounting.

## Validation Evidence

Toolchain builds:

- GCC 11.4.0 Debug: `.codex/build-lsp-opt-gcc`
- Clang 14.0.0 ASan/UBSan with `-no-pie`: `.codex/lsp-optimize-validation/clang-asan-current`
- MSVC 19.44.35228.0 Debug: `.codex/lsp-optimize-validation/msvc`

The 30-case protocol conformance driver passed in all three configurations,
including both new malformed-header cases. The GCC CTest and Clang CTest
`language_server_stdio_protocol_conformance` tests each passed `1/1`; the
Clang run emitted no ASan/UBSan diagnostics. The rebuilt MSVC executable passed
the same driver directly with all `30/30` cases.

## Acceptance Decision

Accepted for Plan 01 Task 3 Sub01. Header input can no longer be silently
truncated at an embedded NUL, and all explicit charset parameters are checked.
The Task 3 parent remains pending for its complete limit, failure-classification,
transport and lifecycle acceptance.
