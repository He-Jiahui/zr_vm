# SSA builder iterator invoke contract

## Scope

This slice establishes canonical SemanticIR and ExecIR identities for iterator
protocol dispatch before source `foreach` publishes its graph. Iterator
initialization, advance, and current-value retrieval remain distinct operations
instead of being encoded as generic calls.

Each ExecIR iterator operation has one operand and one result, is a throwing and
allocating terminator, and conservatively reads and writes managed-heap and
native-FFI memory classes. Its successors are ordered normal then exception.
The builder splits consecutive invoke-capable operations so every protocol step
owns one normal continuation and one shared exceptional continuation.

The result exists only on the normal edge. The SSA verifier applies this rule to
all value-producing, may-throw terminators, so consuming `ITER_CURRENT`'s result
from its exceptional path reports `ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE`.

ExecBC and AOT projections reject these opcodes with
`ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED` and retain any earlier published projection.
Backend execution is deliberately outside this contract slice.

## Focused coverage

`tests/parser/test_ssa_builder_iterator_invokes.c` verifies opcode identity,
block splitting, successor order, exception marking, schema effects, reversed
edge rejection, rejection of two-normal and untyped inline edges, core
structural rejection of missing or misplaced exception markers, and
exceptional-result unavailability.
`tests/parser/test_ssa_oracle_projections.c` verifies the fail-closed projection
boundary for all three opcodes. `tests/parser/test_pre_semantic_ir.c` freezes the
three golden SemanticIR names.

## Validation

- Windows MSVC Debug: the 10-test builder/SSA/projection adjacency set passed;
  the SemanticIR producer passed 67/67.
- Fresh WSL GCC 11.4 and Clang 14 Debug builds: the same 10 CTest cases passed
  on each compiler; the SemanticIR producer passed 67/67 on each.
- Fresh WSL GCC ASan+UBSan with leak detection: the iterator builder and
  projection boundary executables passed without sanitizer diagnostics.
- Documentation validation passed with 116 Markdown files, 115 manifest pages,
  and 644 local links.
