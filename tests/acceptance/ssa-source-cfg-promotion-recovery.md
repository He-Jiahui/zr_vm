# Source CFG promotion recovery

## Boundary

Base: `73d58e63`. The source-owned straight-line finalizer previously freed
the analysis CFG before calling `ensure_active` and `finish`. A recoverable
failure after activation left `preSemanticIrCfgActive` set and discarded the
prior graph. A failure during finish could also append a BRANCH and RETURN
without restoring their instruction and source-map lengths.

The new fault hooks call the real helpers before reporting failure. The RED
case observed active CFG state after the finalizer returned false. The fixed
implementation promotes into a temporary graph, restores previous logical
lengths and compiler cursors on failure, and frees the previous graph only
after both helper calls succeed. The regression covers activation and finish
failure with and without an existing analysis graph, then retries validation
and strict ExecIR construction from the unchanged source facts.

This is a recoverable-helper-failure transaction, not a claim that the core
array allocator's throwing/fatal OOM behavior is caught. The independent
preflight scratch-allocation fault enumeration remains in the same suite.
The helper fault hooks report failure after real activation or complete finish;
they do not inject each intermediate branch/edge append failure inside finish.
Those internal paths share the same rollback branch but are not fault-enumerated.

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
