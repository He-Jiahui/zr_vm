# Source for short-circuit CFG (2026-09-27)

## Scope and baseline

Advance SSA plan 01.02 for statement-form `for` with a supported top-level
`&&` or `||` condition and existing linear initializer/step preflight. The
logical join branches to body/exit; body and unvalued `continue` reach the
step block, which returns to the original condition entry. Unsupported RHS
expressions retain the analysis-only source graph.

The focused MSVC Debug static regression initially failed 2/3 cases before
`for` condition preflight changed; the unmodeled RHS negative passed. After
enabling the condition shape, source graph/build tests passed, but the Oracle
found a separate zero-iteration result error: `return step` yielded 1 while
the step block executed zero stores.

The minimal no-loop source `var flag: bool = false; var side: bool = true;
var step: int = 0; return step;` isolated the shared root cause. Reusing a
temporary compiler slot left the declaration conversion reading the preceding
boolean's ValueId 3 instead of the current literal ValueId 5. The lower-layer
test failed at `Expected 5 Was 3` before the shared expression-result transfer
created a fresh binding. The `for` Oracle failure and no-loop regression then
passed together, with the previous `while` regression still passing.

## Validation boundary

`tests/parser/test_ssa_source_for_short_circuit.c` checks exact `&&`/`||`
edge order, RHS and step block ownership, the step backedge, strict ExecIR
build, and conservative unsupported fallback. Oracle cases assert returned
integer, conditional RHS store count, and step store count, with a 128-step
limit to catch broken cycles. The no-loop check verifies the conversion's
source ValueId directly. Target: `zr_vm_ssa_source_for_short_circuit_test`;
CTest name: `ssa_source_for_short_circuit`. This stage does not claim nested
logical operands or full SSA plan completion.

The shared transfer also changed an existing straight-line golden: after the
stale temporary is replaced, an explicitly typed same-type string declaration
can pass source preflight and strict ExecIR build. Its updated instruction
sequence includes the new temporary and a real branch/return; nonnumeric
cross-type conversion is still rejected. MSVC 19.44 Debug static validation
passed `pre_semantic_ir` 101/101 and the selected eight SSA CTests 8/8.
No new cancellation API exists at this producer boundary. The fresh
temporary creation uses the existing SemanticIR allocator path; this stage
does not claim injected OOM rollback or GCC/Clang validation.
