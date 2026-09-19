# SSA exception payload acceptance

## Scope

This phase adds the low-level value contract required before source
`try`/`catch` lowering can bind the active exception. SemanticIR and ExecIR now
have an `EXCEPTION_PAYLOAD` operation with one result and no operands. It is a
nonterminating, zero-effect definition local to an exception handler block.

Core structural verification accepts the operation only when its block is not
the function entry, is marked exceptional, and every predecessor enters through
a may-throw terminator's direct exceptional successor. A handler block may
define at most one payload. The parser builder runs structural and SSA
verification before publishing, preserving the prior output on failure. The
reference oracle and both ExecBC/AOT projections reject the operation with
`UNSUPPORTED` until those execution boundaries carry an active exception
payload.

This checkpoint deliberately does not publish source `try`/`catch`/`finally`
CFG. Existing propagation-only exception sinks remain instruction-free; the
next source phase can consume this operation without inventing a normal-edge
value or a fabricated `THROW` operand.

## Focused coverage

`tests/parser/test_ssa_builder_control_edges.c` constructs a typed call with
ordered normal and exception successors. Its handler defines and returns the
payload, then verifies:

- SemanticIR-to-ExecIR mapping and the one-result/zero-operand schema;
- the exception block marker and direct handler predecessor;
- combined structural and SSA verification;
- rejection of the same payload definition when moved into the normal block
  and rejection of two payload definitions in one handler;
- rejection of a payload handler that is also the function entry block;
- rejection of a payload block that also has a normal predecessor;
- source-identified `EXCEPTION_EDGE` diagnostics and transactional output
  preservation.

`tests/parser/test_ssa_oracle_projections.c` verifies that the oracle, ExecBC
projection, and AOT projection all fail closed with the exact opcode while
retaining previously published projections. The SemanticIR formatting golden
also fixes the appended opcode name and numeric position without renumbering
the existing opcode set.

## Validation

- Windows MSVC Debug: `zr_vm_pre_semantic_ir_test` passed 77/77 and the eleven
  focused SSA adjacency/projection executables passed 11/11 through `ctest`.
- WSL GCC 11.4 Debug: the producer passed 77/77 and the same focused SSA set
  passed 11/11.
- WSL Clang 14 Debug: the producer passed 77/77 and the same focused SSA set
  passed 11/11.
- WSL Clang 14 ASan+UBSan Debug, linked non-PIE to avoid the host's intermittent
  PIE/ASan startup-address collision: the producer passed 77/77, and
  `ssa_builder_control_edges` plus `ssa_oracle_projections` each passed twice
  with leak detection enabled and no sanitizer diagnostic.
- `python scripts/validate_wiki.py --root .` passed for 116 Markdown files,
  115 manifest pages, and 644 local links.
- `python -m unittest tests.scripts.test_validate_wiki -v` passed 5/5.
