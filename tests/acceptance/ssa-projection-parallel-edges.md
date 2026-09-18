# SSA 01.05: projection of parallel-edge phi copies

## Scope and RED evidence

The real two-block ExecIR fixture has two successor slots from block 1 to
block 2 and a phi with two distinct incoming values aligned to its repeated
predecessor slots. Before the projection repair, the MSVC standalone target
failed with `FAIL: ExecBC projection rejected a valid parallel-edge phi`:
the shared projection validator required exactly one matching predecessor
occurrence and independently rejected duplicate phi predecessor IDs.

## Expected behavior and negative boundary

- ExecBC and AOTIR each preserve both original outgoing slots, insert distinct
  synthetic critical-edge blocks 3 and 4, and point the target predecessor
  row and both phi incoming records at [3, 4].
- Copies [1 -> 4, 2 -> 4] have different projected edge tags [3, 4]. The
  source ExecIR adjacency remains [1, 1]. AOTIR remains `runnable=false`.
- Removing one of the duplicate edges from either adjacency row reports
  `INVALID_BLOCK`. Both lowerers leave their previously published output
  untouched when preflight fails.

## Observed validation

- MSVC (VSDevCmd, D:-backed build): focused CTest
  `^ssa_oracle_(parallel_edges|projections)$` passed 2/2; the expanded
  core/effects/dominator/builder/value/oracle/pass-manager selection passed
  8/8.
- WSL GCC 11.4 (D:-backed Debug build): the same focused CTest passed 2/2.
- WSL GCC 11.4 also compiled the standalone test with
  `-fsanitize=address,undefined -fno-omit-frame-pointer`; the D:-backed
  binary printed `ssa oracle parallel edges PASS` (exit 0), with no
  sanitizer report.
- WSL Clang 14 directly compiled the test against the real core/parser ExecIR
  sources and the resulting D:-backed binary printed
  `ssa oracle parallel edges PASS` (exit 0). The separate Clang CMake build
  stalled while regenerating `build.ninja` on the WSL `E:` mount and was
  stopped; no Clang CTest or full-backend result is claimed.

## Acceptance boundary

This verifies the metadata and failure-atomicity seam for parallel-edge phi
projection. The earlier oracle execution regression is recorded in
`ssa-oracle-parallel-edges.md`. Executable ExecBC emission, C/LLVM parity,
source SemIR lowering, pruned phi construction, and exception edges remain
separate milestone gates; this fixture does not claim them.
