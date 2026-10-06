---
related_code:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - tests/parser/test_template_literal_source_range.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
plan_sources:
  - .codex/plans/20261005-ssa-template-ast-ranges.md
  - .codex/plans/20261005-ssa-multiline-token-coordinates.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_template_literal_source_range.c
  - tests/parser/test_expression_fragment_parser.c
  - tests/parser/test_ssa_source_range_identity.c
  - tests/acceptance/template-literal-source-range.md
doc_type: module-detail
status: finite-template-ast-root-range-green
---

# Template Literal Source Range

## Contract and producer

A template AST root must retain the source identity and complete raw byte
range of its original template token, including both backticks. Offsets are
zero-based with an exclusive end; lines and columns are one-based. LF, CRLF
and bare CR each advance one line; CRLF still occupies two source bytes.
The root excludes the following semicolon, statement and EOS position.

`parse_literal` captures `literalLoc` before dispatch. The template branch
saves the token string value, advances `ZrParser_Lexer_Next`, then calls
`parse_template_string_literal` and `create_template_string_literal_node`.
At the observed RED producer the constructor uses the advanced current
location and the branch does not restore the saved range. The bounded
repair assigns `literalLoc` to the nonnull returned root before
returning. Actual production GREEN passed the finite gate.

This assignment changes no public API, advance order, allocation, ownership
or segment construction. It does not establish segment child coordinates.
The producer retains one responsibility; this repair requires no file split.

## Independent six-case evidence

The [test guide](../../tests/acceptance/template-literal-source-range.md)
records six parse-only cases. Four parse two top-level statements and verify
that the second remains `return 9;`. Two parse expression fragments at EOF
and verify their segment contents. All independently scan the original raw
token and compare hardcoded source identity, bytes, offsets, lines and columns
before checking the actual template AST root against those fixed constants.

`--prerequisites-only` runs the same six cases and omits only the final AST
root coordinate assertion. It retains AST shape, segment content where
asserted, raw token coordinates, no diagnostics and EOS checks. The
[acceptance record](../acceptance/template-literal-source-range.md) preserves
the initial GC-rooting prerequisite failure and subsequent genuine RED.
Corrected prerequisites passed 6/6; full execution then failed all six AST
start offsets, with no observed UBSan diagnostic. After the saved-range
assignment, all six passed alongside the 32 existing consumer cases.

## Acceptance boundary

The [multiline token gate](ssa-multiline-token-coordinates.md) remains frozen
historical token-coordinate acceptance. Its AST shape checks do not establish
these root coordinates. This successor has its own independent finite gate.

Segment child coordinates, full parser/OOM acceptance, GC stress, offset-cache
and diagnostic snippet CR handling, Linux GCC/Clang (existing WSL
E_ACCESSDENIED), full MSVC matrix, descriptor/native source execution,
retention and full SSA47 remain OPEN. These six cases do not compile source,
execute the VM, resolve imports, or exercise network, FFI, providers,
capabilities or security. Existing expression-fragment consumers include local
type environments, type inference, semantic facts, AST import normalization
and compiler init/free; they do not establish foreign resolution or execution.
