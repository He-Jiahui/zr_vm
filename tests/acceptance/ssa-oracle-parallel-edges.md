# SSA 01.02/01.05: oracle parallel-edge phi identity

## Scope and RED evidence

Two explicit CFG edges from the same source block reach one target block,
whose phi has two different incoming values ordered by predecessor edge slot.
The focused real-core test first failed in MSVC CTest with
`FAIL: oracle rejected parallel CFG edges with ordered phi incoming slots`;
the oracle diagnostic was `PHI_PREDECESSOR_MISMATCH` (code 15), block 1,
before interpretation. After removing only the duplicate-block rejection, the
test failed with `FAIL: oracle merged the two parallel edges into one phi
input`: interpretation still selected the first incoming by block ID.

The test fixture initially used function ID zero; core verification reported
`INVALID_ARGUMENT`. The fixture now has a valid ID and is verified at
STRUCTURE|SSA level before entering the oracle. That early failure was a test
setup error, not evidence of a production verifier defect.

## Fixture and boundary cases

- A two-block real ExecIR function defines values 11 and 22 in the source;
  both outgoing slots target the same successor with predecessor range [1,1].
- A two-slot phi has distinct incoming value IDs at each position. For
  `CONDITIONAL_BRANCH`, true selects 11 and false selects 22.
- The same edges under `SWITCH` with selector 0 select 11, selector 1 selects
  22. Four oracle executions use the real verifier and core interpreter.
- An instruction selecting its second successor while the source block
  advertises only its first adjacency occurrence reports
  `PHI_PREDECESSOR_MISMATCH` at destination block 2 and terminator instruction
  4, without publishing any partial oracle output. Other malformed adjacency
  cases remain part of the wider CFG/verifier acceptance matrix.

## Observed validation

- MSVC via VSDevCmd built in `D:/zr-ssa-verify-871bc234`; `ctest --test-dir
  D:/zr-ssa-verify-871bc234 -R
  '^ssa_(oracle_parallel_edges|oracle_projections|builder_cfg|dominator_cfg|core_model|effects_verifier|value_validation|pass_manager_scalar)$'
  --output-on-failure --no-tests=error` reported **8/8 passed** after the
  switch variants were added. Compiler warning D9025 reports existing `/W3`
  overridden by `/W4`; compilation and linking succeeded.
- WSL GCC 11.4 compiled the standalone test plus real core ExecIR sources
  with `-std=c11 -g -O0 -fsanitize=address,undefined
  -fno-omit-frame-pointer`, writing the binary to
  `/mnt/d/zr-ssa-verify-871bc234/ssa_oracle_parallel_gcc_asan`.
  Running the binary printed `ssa oracle parallel edges PASS` (exit 0),
  without a sanitizer report, both before and after extracting the phi entry
  module. A separate D:-backed GCC Debug CMake build successfully built
  `zr_vm_ssa_oracle_parallel_edges_test`, and `ctest --test-dir
  /mnt/d/zr-ssa-verify-871bc234/wsl-gcc -R '^ssa_oracle_parallel_edges$'
  --output-on-failure --no-tests=error` reported 1/1 passed after the split.
- WSL Clang compiled the same sources to
  `/mnt/d/zr-ssa-verify-871bc234/ssa_oracle_parallel_clang`; running it
  printed `ssa oracle parallel edges PASS` (exit 0). After the phi-entry
  module was extracted, the D:-backed Clang 14 Debug target built and
  `ctest --test-dir /mnt/d/zr-ssa-verify-871bc234/wsl-clang -R
  '^ssa_oracle_parallel_edges$' --output-on-failure --no-tests=error`
  reported 1/1 passed. These focused CTests are not a claim that the full
  GCC/Clang test matrix or cross-backend oracle parity passed.

## Acceptance boundary

Accepted for the oracle's branch/switch phi selection by edge occurrence.
The separate ExecBC/AOT metadata projection regression is recorded in
`ssa-projection-parallel-edges.md`; neither test establishes executable
backend parity. Complete 01.02/01.05 and M1 acceptance also need source
SemIR lowering, true pruned phi construction, exception edges and
four-backend differential behavior.
