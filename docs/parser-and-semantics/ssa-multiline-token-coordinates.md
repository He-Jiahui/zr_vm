---
related_code:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - tests/parser/test_ssa_source_range_identity.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
plan_sources:
  - .codex/plans/20261005-ssa-multiline-token-coordinates.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_ssa_source_range_identity.c
  - tests/acceptance/ssa-multiline-token-coordinates.md
  - docs/acceptance/ssa-multiline-token-coordinates.md
doc_type: module-detail
status: finite-multiline-token-coordinate-green
---

# Multiline Token Coordinates

## Contract and producer repair

Token ranges count raw source bytes and use exclusive ends. Lines and columns
are one-based. Within a token span, bare CR, LF and CRLF each advance one line;
CRLF retains two source bytes. The raw line start lies after the complete
newline sequence, so subsequent columns follow the original source bytes.

The actual lexer already applies these newline rules. The previous
`file_position_advance_over_span` loop in `parser_state.c` counted only LF.
A legal multiline template containing bare CR therefore had correct byte
offsets but an incorrect token end line and column. The repair changes only
that existing loop: count CR once, consume a following LF only when it lies
inside the span, and update the raw line start after the sequence. LF alone
continues to advance one line.

The loop's `index + 1 < endOffset` condition prevents reading a following
pre-read LF outside the span. A CR at the final span byte is supported by this
static bound argument; the four new fixtures do not independently exercise
that boundary dynamically. Offset clamping, cached token-start positions,
byte offsets, token-end pre-read calibration, lexer behavior and public APIs
retain their existing implementation.

## Independent source evidence

The existing [SCRIPT/RETURN range gate](ssa-source-range-identity.md) remains a
historical frozen 13-case baseline. The same fixture now defaults to 17 cases;
`--baseline-only` selects the original 13 without changing their assertions.
Four added cases independently parse the same legal source that they scan.
They establish SCRIPT → RETURN → TEMPLATE_STRING_LITERAL shape, nonempty
template segments, EOS and zero diagnostics before coordinate assertions.

A separate parser state scans RETURN, template, semicolon and EOS with actual
lexer lookahead and Next calls. Lookahead preserves the current RETURN and
template ranges. Literal expected bytes, line and column values are supplied
directly; the token, semicolon and EOS spans are checked against raw source.
The common RETURN range is `[0, 6)`, from 1:1 to 1:7.

| Case | Template bytes and start → end | Semicolon bytes and start → end | EOS point |
| --- | --- | --- | --- |
| bare CR | `[7, 14)`, 1:8 → 2:4 | `[14, 15)`, 2:4 → 2:5 | byte 16, 3:1 |
| LF | `[7, 14)`, 1:8 → 2:4 | `[14, 15)`, 2:4 → 2:5 | byte 16, 3:1 |
| CRLF | `[7, 15)`, 1:8 → 2:4 | `[15, 16)`, 2:4 → 2:5 | byte 18, 3:1 |
| mixed internal CR/CRLF/LF, following CRLF | `[7, 17)`, 1:8 → 4:3 | `[19, 20)`, 5:1 → 5:2 | byte 21, 6:1 |

Existing global teardown owners release both parser states and the AST even
when a Unity coordinate assertion fails. The [test guide](../../tests/acceptance/ssa-multiline-token-coordinates.md)
records the actual sources and execution modes.

## Acceptance and limits

The [acceptance record](../acceptance/ssa-multiline-token-coordinates.md)
preserves the real RED commit and receipts. RED passed the original 13 and
failed exactly the bare-CR and mixed token end lines. GREEN passed all 17 and
the concrete consumer regressions: 13 CTest entries, 12 Unity suites with 170
cases, and a separate standalone dominator test. No failures, ignored cases
or UBSan diagnostics were observed on Windows x64 clang-cl 19 Debug UBSan.
MSVC compiled the changed translation unit only; its full matrix remains OPEN.

Template AST ranges remain **OPEN**. `parser_literals.c` captures `literalLoc`
before dispatch but advances Next before constructing a template literal;
that producer needs its own evidence and repair. AST shape acceptance here
does not establish template AST coordinates. Offset-cache CR handling,
diagnostic snippet CR handling, full parser/OOM acceptance, Linux GCC/Clang
(existing WSL E_ACCESSDENIED), descriptor/native execution, retention and the
full SSA47 plan remain **OPEN**.

This gate exercises local parsing and scanning. The existing consumers include
local VM/Oracle behavior, without adding network/import, FFI, provider,
capability or security tests.

## Subsequent template AST root gate

The OPEN template AST range statement above records this frozen token-only
acceptance boundary. The independent [template AST root gate](template-literal-source-range.md)
now has actual finite GREEN evidence for six template AST root cases, after
its own corrected RED. This successor does not change the historical token
receipts, hashes or coverage claims. Other OPEN gates remain OPEN.
