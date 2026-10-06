---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.c
  - tests/parser/test_ssa_source_range_identity.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
plan_sources:
  - .codex/plans/20261004-source-aot-handoff-design-2005.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/parser-and-semantics/ssa-source-script-entry-identity.md
tests:
  - tests/parser/test_ssa_source_range_identity.c
  - tests/parser/test_ssa_source_script_entry_identity.c
  - docs/acceptance/ssa-source-range-identity.md
doc_type: module-detail
status: source-range-direct-green
---

# Source SCRIPT and RETURN Range Identity

## Purpose and scope

This finite parser contract gives the actual SCRIPT root and RETURN statement
their source spans before compiler consumers inspect them. The earlier
[SCRIPT identity gate](ssa-source-script-entry-identity.md) preserved parser
positions that included a point RETURN range. This gate corrects the producer
positions without changing callable admission, canonical IDs, signature hashes,
or the pointer-free metadata schema.

The production change is limited to `parser.c`, `parser_statements.c`, and
`parser_state.c`. Existing lexer cursor and token-start fields remain the source
of position evidence. This work does not redesign all AST ranges.

## Range contract

Offsets count source bytes, and ranges use `[start, end)` with an exclusive end.
The fixture also compares source identity, line, and column; its literal
coordinate assertions use one-based lines and columns.

| Node or token | Start | Exclusive end |
| --- | --- | --- |
| Nonempty SCRIPT | Start of the first actual token, before optional module parsing | EOS position after scanning trailing trivia |
| Empty or trivia-only SCRIPT | EOS position | Same EOS position |
| RETURN with semicolon | Start of `return` | End of the semicolon captured before consumption |
| RETURN missing semicolon | Start of `return` | End of the accepted expression, accepted `ref`, or `return` token when neither was accepted |
| `referenceLocation` | Start of actual `ref` token | End of that token |
| Return expression | Its own expression range | Its own expression range |

Leading whitespace/comments fall before the SCRIPT start. Trailing trivia is
inside the nonempty SCRIPT span because its end is EOS. A RETURN includes its
internal whitespace and newlines through the semicolon; it excludes following
trivia and the following statement. `return;` receives the same statement-span
rule even though it has no expression. `return ref value;` keeps independent
`ref` and expression ranges.

`parse_script` captures `get_current_token_location` before parsing its optional
module declaration and merges that range with the token range at completion.
The legal-source fixture verifies completion at EOS. This does not establish a
complete SCRIPT span after every malformed-input recovery path.

## RETURN construction and recovery

`parse_return_statement` saves the `return` token range as both initial start
and fallback end. An accepted `ref` updates the fallback end while preserving
`referenceLocation`; a successfully parsed expression then supplies its own
end. When a semicolon is present, its token range is saved before
`consume_token` advances the lexer, and the final node merges the saved start
and end. The next token cannot become the end merely because parsing advanced.

When a semicolon is missing, the existing missing-semicolon diagnostic remains
and the current token is left available. For `return 9 var next = 8;`, the
direct statement fixture verifies one diagnostic at `var`, RETURN bytes
`[0, 8)` (`return 9`), and the still-current `var` bytes `[9, 12)`. It then calls
the variable declaration parser and verifies the real `next` pattern and `8`
initializer before EOS. The parser retains `hasError` and no fatal error.
This is a direct statement recovery proof; whole SCRIPT error recovery and
allocation-failure ownership need separate evidence.

## Shared token end and CRLF

The lexer has already read one following character when a token range is
requested. Its normalized newline cursor can represent both raw CRLF bytes.
`get_current_token_location` excludes the following character as before and
also removes the remaining raw CR byte when the actual source has the CRLF
pair at that boundary. The check reads the raw source bytes; it does not
reinterpret every normalized newline as a two-byte source sequence.

The helper continues to use cached token-start line information and source
span advancement for line/column positions. Existing source-text calibration
for identifiers and numeric/boolean tokens remains in place. No lexer field,
AST layout, or public API is added.

For `return 9;\r\n`, the semicolon lexer cursor has reached byte 11 and line 2,
but its actual token range is `[8, 9)`, line 1 columns 9 through 10. EOS is the
point at byte 11, line 2 column 1. The fixed-coordinate CRLF fixture for
`// leading\r\n  return\r\n    9;\r\n` independently verifies:

| Range | Byte span | Start line:column | End line:column |
| --- | --- | --- | --- |
| `return` | `[14, 20)` | 2:3 | 2:9 |
| Literal `9` | `[26, 27)` | 3:5 | 3:6 |
| Semicolon | `[27, 28)` | 3:6 | 3:7 |
| RETURN | `[14, 28)` | 2:3 | 3:7 |
| SCRIPT | `[14, 30)` | 2:3 | 4:1 |
| EOS | `[30, 30)` | 4:1 | 4:1 |

These literal coordinates supplement comparisons against actual token ranges,
so CRLF correctness is not inferred solely from a parser helper agreeing with
itself.

## Validation and remaining work

The frozen RED commit is `d2551f5953786cc5b7ef34ba852b376792f49e3d`:
11 cases, 9 failures and 2 passes; build exit 0 and focused CTest exit 8.
The historical frozen GREEN fixture has 13 cases, 0 failures and 0 ignored; build and
focused CTest both exit 0. Additional coverage fixes CRLF coordinates and
checks semicolon-before-CRLF and missing-semicolon recovery. Tests parse real
source and release their AST/parser/state owners.

The separate consumers regression passed 8/8 CTest entries: seven Unity suites
with 85 cases and one standalone dominator CFG test. The entries cover source
ExecBC/VM loops, straight-line CFG, callable returns, SCRIPT entry tokens,
callable identity, SCRIPT entry identity, typed binding, and dominator CFG.
The [acceptance record](../acceptance/ssa-source-range-identity.md) preserves
receipt hashes, exact suite names, environment, and limits.

Both historical runs used Windows clang-cl 19 x64 Debug UBSan against the then-current checkout
directly, with no source snapshot. No UBSan diagnostic was observed. The
[new multiline token gate](ssa-multiline-token-coordinates.md) preserves the
original 13 as a baseline and expands default execution to 17 cases for bare
CR, LF, CRLF and mixed newlines. Its separate evidence owns the newer source
and executable pins; these historical 13-case pins remain frozen. The full
47-item SSA plan remains **OPEN**. Retained canonical graphs, frame publication,
serialization, native/AOT consumers, full parser acceptance, other platforms,
and exhaustive OOM or rollback handling are not established by this finite gate.
Network, FFI, providers, external services, and security probes were not run.
