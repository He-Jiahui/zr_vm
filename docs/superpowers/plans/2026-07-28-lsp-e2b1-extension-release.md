# LSP E2b1 Extension Release Plan

> **Execution note:** Follow `evidence-driven-wsl-validation` and
> `verification-before-completion` before staging, committing, or packaging.

**Goal:** Stabilize the paused-frame canonical-binding integration, commit only
that independently verified LSP slice, run the VS Code desktop smoke test, and
produce an installable VSIX from the committed source state.

**Scope boundary:** This plan excludes the pre-existing staged LSP scheduler
rewrite, semantic variance rewrite, and unrelated parser/syntax work. Those
paths have staged deletions with separate working-tree recreations and cannot
be included in a release commit without an explicit reconciliation task.

## 1. Establish the E2b1 regression contract

- Inspect the paused-frame test and the canonical-binding injection API added
  by commit `4656de4`.
- Build and run `zr_vm_debug_expression_diagnostics_test` from a dedicated
  cache to establish the current result.
- Correct only E2b1 implementation or test defects exposed by that target.

## 2. Validate the affected language-server surface

- Build the stdio language-server target from a clean release cache.
- Run the focused language-server tests that exercise the current working-tree
  LSP semantic path, without staging their unrelated pre-existing changes.
- Record the exact commands and results in the E2b1 milestone record.

## 3. Commit the isolated E2b1 slice

- Stage only the debug semantic-binding implementation, its focused regression
  test, E2b1 milestone record, status update, and any directly required public
  declarations.
- Verify both the staged diff and the resulting commit before proceeding.

## 4. Smoke and package the extension

- Build the native and WASM language-server artifacts from the committed tree.
- Run `npm run test:e2e:desktop` in `zr_vm_language_server_extension`.
- Package the extension with `npm run package:vsix` and validate installation
  into an isolated VS Code extensions directory.

## Acceptance Criteria

- The E2b1 test proves a paused binding keeps its captured canonical symbol,
  type, and declaration location through expression inference.
- The focused debug and language-server test targets pass from fresh build
  caches.
- The release commit contains no scheduler deletion/recreation split and no
  unrelated syntax changes.
- Desktop smoke exits successfully, the VSIX is generated from the release
  commit, and `code --install-extension` accepts it in an isolated directory.
