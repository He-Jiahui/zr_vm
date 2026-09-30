# Source CFG promotion recovery

## Boundary

Base: `73d58e63`. The source-owned straight-line finalizer previously freed
the analysis CFG before calling `ensure_active` and `finish`. A recoverable
failure after activation left `preSemanticIrCfgActive` set and discarded the
prior graph. A failure during finish could also append a BRANCH and RETURN
without restoring their instruction and source-map lengths.

The earlier promotion transaction protects an inactive analysis graph while
the compiler activates and finishes a temporary source graph. Its helper-level
fault hooks report failure after activation or after a complete finish; they
remain useful for that boundary but do not cover failures inside an active
finalizer.

The active finalizer now deep-stages the CFG block array and every block's
outgoing edges, along with the append-only SemanticIR instructions, source map
and value operands. `compiler_semantic_cfg_finish` runs against that separate
state. A failed snapshot allocation returns before finish can mutate the
source compiler. A finish or staged-storage failure frees the staged data and
leaves the original graph, block ranges, append counts and old bytes, active
flag, cursor and validation flag unchanged. A successful finish publishes the
prepared graph and arrays without further allocation.

The internal fault matrix uses the real source `if (true) { return 9; }` and
injects at all six finalizer sites: EXIT block append, BRANCH instruction
append, BRANCH range bind, normal-edge append, RETURN instruction append and
RETURN range bind. After each failure the test compares the active source CFG
and append-only arrays against their pre-finish snapshot, then retries the same
compiler state through ExecIR build, Core Verify ALL and Oracle execution.
The expected result is a normal return of signed integer 9.

The confirmed pre-fix RED was at the second fault, BRANCH instruction append:
the snapshot expected 4 CFG blocks and observed 5 because the live finish had
already appended its EXIT block. The baseline ExecIR build, Core Verify and
Oracle had passed before this internal fault matrix ran. The evidence is in
`D:/tmp/zr_vm/ssa-control/cfg-provider-ctest.log`.

The transaction checks its copied arrays for allocator failure before it
starts finish. The six-point matrix injects failures inside finish; it does not
currently force an OOM independently at each backup-allocation site. The core
allocator's terminal or throwing OOM behavior remains outside this returned-
failure contract.

## Validation

With existing debug configurations, GCC 11.4, Clang 14, MSVC 19.44 static,
and GCC ASan/UBSan (leak detection enabled) each passed the selected 17 SSA
CTests, including the 4/4 fault suite and 29/29 source CFG suite. The direct
`pre_semantic_ir` executable passed 101/101 in all four. MSVC shared-DLL
passed both the source CFG and fault CTests (2/2). Builds used
`cmake --build build/<configuration> --target
zr_vm_ssa_source_cfg_faults_test zr_vm_ssa_source_straight_line_cfg_test
zr_vm_pre_semantic_ir_test -j 4` (the DLL run omitted the golden target).
Tests used `ctest --test-dir build/<configuration> --output-on-failure -R
<selected SSA names> --no-tests=error`, plus direct golden executables.

The separate dirty `test_place_cfg_graph.c` still has two existing
alias-overlap assertion failures; it was not edited or included in the passing
selection. The scope excludes other dirty runtime, Rust and parser test files
in the shared worktree. A core allocator exception/terminal OOM remains outside
this recoverable-return transaction.

## Current verification status

Post-fix native build completed 18/18 steps with exit code 0. The targeted
CTest run passed both `ssa_source_straight_line_cfg` (10.23 s) and
`ssa_source_cfg_faults` (0.54 s), 2/2 tests in 10.88 s total. The direct
`pre_semantic_ir` executable passed 117 tests with zero failures and zero
ignored tests. Logs are under `D:/tmp/zr_vm/ssa-control/`:
`cfg-transaction-build.log`, `cfg-transaction-ctest.log`, and
`cfg-pre-semantic-direct.log`.

The six internal failure sites and post-failure source retry are therefore
verified by the native target. Independent review found no production blockers;
the snapshots verify logical contents and compiler cursors, rather than array
head or capacity identity. Commit is pending. SSA plan 01.02 remains open; this focused transaction result does
not complete the plan.
