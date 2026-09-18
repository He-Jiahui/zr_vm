# SSA 01.02: canonical value and instruction IDs

## RED and boundary fixtures

The independent `ssa_builder_fact_identity` fixture gives the first semantic
value ID 2 while an instruction expects value 1. The old builder published
that value as ID 1 regardless; the MSVC red run printed `builder silently
renumbered a mismatched canonical value ID`. A second fixture gives the
first semantic instruction ID 7 and would previously publish it in the
source map as though it were valid. Both now fail before candidate
construction, without modifying caller-owned output:

- Value mismatch: `INVALID_VALUE`, function token 42, expected/actual ID
  [1,2].
- Instruction mismatch: `INVALID_RANGE`, function token 42, instruction and
  source ID 7, expected/actual ID [1,7].

A valid one-block constant/return fixture retains one-based value ID 1 and
source IDs [1,2]. This checks the contract enforced by the upstream
`ZrParser_SemanticIr_Emit` producer without silently inventing IDs.

## Observed validation

- MSVC (VSDevCmd, D:-backed build): focused adjacent SSA CTest selection
  passed 11/11, including CFG, dominance, control-edge and identity targets.
- WSL GCC 11.4 directly compiled the real builder/core sources with
  `-fsanitize=address,undefined`; the D:-backed identity fixture printed
  `ssa builder fact identity PASS` (exit 0), without a sanitizer report.
- WSL Clang 14 compiled the same sources without sanitizers; its D:-backed
  fixture printed `ssa builder fact identity PASS` (exit 0). Full GCC/Clang
  CTest and Clang sanitizer checks were not run for this slice.

## Remaining gates

IDs alone do not prove definition liveness, place alias safety, pruned phi
placement, exception-edge availability or full 01.02 source parity. The
unrelated dirty `test_ssa_construction.c` was not changed.
