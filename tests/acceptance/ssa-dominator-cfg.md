# SSA 01.02: validated CFG dominator analysis

## Scope

- Parser ExecIR CFG analysis in
  `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c`.
- An independent focused `ssa_dominator_cfg` CTest target avoids staging the
  pre-existing edits in `tests/parser/test_ssa_construction.c`.
- This slice covers edge prevalidation, reverse-postorder DFS and idom
  convergence. Full SemIR lowering, phi insertion and four-backend execution
  are still outside this acceptance decision.

## Baseline and RED

On main after `af03df20`, the new standalone GCC fixture linked against the
existing parser CFG implementation and exited 1 with
`FAIL: out-of-range successor was silently skipped`. The old traversal
ignored invalid successors. After prevalidation was added, the same fixture
reached its valid diamond case, where the original preorder/intersection
implementation did not terminate; the running fixture was interrupted.
An iterative reverse-postorder implementation replaced those two faulty
paths before the focused GREEN run.

## Test inventory

- Invalid successor outside the one-block graph: reject before traversal,
  preserve cached idom, and report `INVALID_BLOCK` with block 1, expected
  maximum 1 and actual ID 2.
- Diamond: direct child idoms and the merge idom all equal the entry; running
  the analysis twice gives the same results.
- Entry/header/body loop: header idom is entry and body idom is header; a
  disconnected fourth block remains undefined.
- The standalone target links the real core model, verifiers, materializer,
  execution contract and parser CFG analyzer, with no mocked graph algorithm.
- Malformed edge ranges and invalid predecessors are validated by the new
  preflight but do not yet have individual regression fixtures; duplicate
  edge identity, exceptional edges, phi insertion and builder-produced graphs
  keep the broader 01.02 milestone open.

## Tooling evidence (2026-09-17)

- WSL GCC 11.4 built `zr_vm_ssa_dominator_cfg_test` in
  `build/ssa-gcc-debug`; `ctest --test-dir build/ssa-gcc-debug -R
  '^ssa_dominator_cfg$' --output-on-failure --no-tests=error` reported 1/1
  passed on the initial GREEN implementation.
- WSL GCC rebuilt the focused sources with `-std=c11 -g -O0
  -fsanitize=address,undefined -fno-omit-frame-pointer`; the resulting
  `/mnt/d/zr-ssa-verify-871bc234/ssa_dominator_gcc_asan` printed
  `ssa dominator CFG PASS` with no sanitizer report (exit 0) after the final
  32-bit-only overflow-guard cleanup.
- WSL Clang 14 built the standalone focused executable from the same source
  set and `/mnt/d/zr-ssa-verify-871bc234/ssa_dominator_clang` printed
  `ssa dominator CFG PASS` (exit 0) after that cleanup. This is **not** a
  Clang CTest matrix claim.
- MSVC 19.44 through VSDevCmd configured and built in
  `D:/zr-ssa-verify-871bc234`. After rebuilding the parser CFG source for
  `ssa_pass_manager_scalar`, `ctest --test-dir D:/zr-ssa-verify-871bc234
  -R 'ssa_(dominator_cfg|core_model|effects_verifier|pass_manager_scalar)$'
  --output-on-failure --no-tests=error` reported 4/4 passed. Compiler D9025
  warning reflects the existing `/W3`→`/W4` command-line override.

The E: volume was already full during the prior CFG-verifier slice; no
existing build outputs were deleted. A later attempt to configure the Clang
CTest target was stopped by WSL host resource error `HCS 0x800705aa`.
`wsl --exec uname -r` reproduced the same host error, so Clang CTest and a
fresh final GCC CTest could not be rerun after that point. The GCC sanitizer,
Clang executable and MSVC CTest evidence above are distinct observed checks.

The pre-existing dirty `ssa_construction` test failed independently in a
fresh GCC build: its diamond fixture replaces a block's side-pool range on
the second append instead of keeping both edges, and its invalid-successor
fixture exercised this parser defect before the fix. We neither changed nor
staged that test file; until its fixture is corrected and rerun, it cannot
close the 01.02 integration gate.

## Acceptance decision

Accepted only as the independent dominator-analysis correctness slice. Full
01.02 and M1 acceptance remain open because the production builder,
pre-existing dirty test fixture, full GCC/Clang CTest matrix and downstream
ExecBC/AOT parity have not been verified at this revision.
