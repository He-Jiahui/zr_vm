# Git pull recovery, 2026-10-17

## Scope

Preserve all existing tracked and untracked local work, merge `origin/main`, resolve overlaps, and restore the original four-path staged diff. This record covers Git recovery and targeted checks of the merged IO, parser, and CMake paths. It does not accept the full set of ongoing local feature changes.

## Baseline

- Original HEAD: `20517b7189e9a3203aea9313b97eacda44f8a266`.
- Fetched upstream: `a687f673d7168b110cb4f1d631c4c364db037d7f`.
- Resulting merge: `0bb78897ef8425d135a598fb82f59bfa127cb788`.
- Original index: three plan-file deletions and the added Rust `Cargo.lock`.
- Protected recovery stash: `353a8ea9e478291d1fce257ad293d447989127fe`, retained as `refs/codex/git-pull-reconcile-20261017`.
- Six already dirty third-party submodules were left unchanged. No pure-upstream runtime control build was performed; the stack failure below cannot be described as a proven upstream baseline failure.

## Test Inventory

| Check | Result |
| --- | --- |
| No unmerged index entries; upstream incorporated | Pass |
| Original staged raw diff restored exactly | Pass |
| Original local content preserved or integrated with upstream | Pass, with Git line-ending normalization recorded |
| Six dirty submodule HEADs and statuses unchanged | Pass |
| 688 recorded validation source identities unchanged at final verification | Pass |
| MSVC configure and nine selected build targets, including CLI | Pass |
| `bytecode_validation` | Pass |
| `binary_loader_rejection` | Pass |
| `macro_callable_return_inference` | Pass |
| `template_literal_source_range` | Pass |
| `ssa_source_finalized_module` | Pass |
| `ssa_source_callable_query` | Pass |
| `macro_token_stream` | Pass |
| `parser_nesting_limit` | Fail: Windows stack overflow |
| WSL GCC and Clang configure/build/test | Incomplete; both configurations cancelled after about 21.5 minutes in generation; no build or tests ran |

The nesting suite contains excessive parentheses, unary/conditional/assignment expressions, blocks and else chains, generic types, template interpolation, ordinary nesting, and sibling-expression cases. Its default-stack execution failed before the full suite completed. The binary-only diagnostic described below ran all six Unity cases successfully with a larger stack; that diagnostic does not change the failed default configuration.

## Tooling Evidence

MSVC `19.44.35229.0`, x64 Debug, Ninja, actual checkout `E:/Git/zr_vm`. WSL configurations used Ubuntu 22.04, GCC 11.4 and Clang 14. Full command arrays, exit codes, elapsed time, and log hashes are retained in [the MSVC receipt](E:/cargo-targets/zr_vm/reports/git-pull-reconcile-20261017/validation-msvc.json), [GCC receipt](E:/cargo-targets/zr_vm/reports/git-pull-reconcile-20261017/validation-gcc.json), and [Clang receipt](E:/cargo-targets/zr_vm/reports/git-pull-reconcile-20261017/validation-clang.json).

```powershell
cmake -S E:/Git/zr_vm -B E:/cargo-targets/zr_vm/build/git-pull-reconcile-20261017/msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC_LIB=ON -DBUILD_SHARED_LIB=OFF -DBUILD_NETWORK_LIB=OFF -DBUILD_DEBUG_LIB=OFF -DBUILD_LANGUAGE_SERVER=OFF -DBUILD_RUST_BINDING=OFF -DBUILD_CLI=ON -DBUILD_THREAD_LIB=ON -DBUILD_TESTS=ON -DZR_VM_REGISTER_PERFORMANCE_CTEST=OFF
cmake --build E:/cargo-targets/zr_vm/build/git-pull-reconcile-20261017/msvc --parallel 4 --target zr_vm_io_validation_test zr_vm_binary_loader_rejection_test zr_vm_parser_nesting_limit_test zr_vm_callable_return_inference_test zr_vm_template_literal_source_range_test zr_vm_ssa_source_finalized_module_test zr_vm_ssa_source_callable_query_test zr_vm_token_stream_test zr_vm_cli_executable
ctest --test-dir E:/cargo-targets/zr_vm/build/git-pull-reconcile-20261017/msvc --output-on-failure --no-tests=error -R '^(bytecode_validation|binary_loader_rejection|parser_nesting_limit|macro_callable_return_inference|template_literal_source_range|ssa_source_finalized_module|ssa_source_callable_query|macro_token_stream)$'
```

The failing executable returned `0xc00000fd`. PE headers show a 1 MiB stack reserve. Temporarily changing only this task's generated executable with `editbin /stack:8388608` made all six Unity cases pass. The original executable bytes were then restored and their SHA256 verified. No source, production recursion limit, or test build recipe was changed by that diagnostic. An LLDB attempt produced no usable backtrace.

Independent read-only review identified MSVC Debug stack frames of 4496 bytes for `parse_statement`, 824 bytes for `parse_block_contents`, and at least another 80 bytes in block wrappers. At 256 nested blocks, those frames alone exceed 1.38 MiB. Local cursor checkpoints and file-range changes increase frame sizes; the remote recursion guards were retained. See [review](E:/cargo-targets/zr_vm/reports/git-pull-reconcile-20261017/independent-review.json) and [binary probe](E:/cargo-targets/zr_vm/reports/git-pull-reconcile-20261017/msvc-nesting-stack-probe.json).

## Results

One pull conflict and nine stash-restoration conflicts were resolved while retaining local extraction/feature work and upstream fixes. Original index state, protected recovery reference, and submodule states were verified. Content audits distinguish upstream integration from newline normalization; byte-identical restoration is not claimed for every file.

MSVC configuration and compilation succeeded; seven of eight selected CTest cases passed. The parser stack failure remains unresolved. WSL generation did not finish within the bounded attempt; cancellation was restricted to the two verified task-owned CMake processes. Their runners exited, and exit receipts were recorded before cleanup.

## Acceptance Decision

Git recovery is complete. Runtime acceptance is blocked by the default-stack parser failure and incomplete WSL validation. No full-suite success or pure-upstream failure claim is made. A follow-up parser change should reduce large cursor/frame residency on recursive paths and validate the production recursion guard under the normal Windows stack limit.

All one-time build and helper outputs are task-owned and selected for hash-checked cleanup. Retained reports include command logs, source identities, Git preservation evidence, and failure diagnostics. Actual deletion counts and retained items are recorded in [artifacts.json](E:/cargo-targets/zr_vm/reports/git-pull-reconcile-20261017/artifacts.json).
