# LSP Optimize Baseline

## Snapshot

- Captured commit: `130907d19d480c59cee159a315c8eef926d301c3`.
- `ceadabbfa1436fcd0f2cc6ffd788b45120bb2acc` is an ancestor, so the L8
  external callable-value fact contract is committed evidence, not an
  uncommitted overlay.
- The primary worktree contained concurrent dirty parser, LSP, ownership, GC,
  Rust, and documentation paths. They were excluded from this baseline's
  source snapshot and exact staging set.
- The shared branch advanced to `f845719` after the `130907d` snapshot had
  been created. That commit changes only shared-library export declarations;
  it is deliberately not mixed into this frozen behavior record.
- The first `/mnt/e/.../.codex/build-lsp-opt-gcc-20260822` attempt timed out
  while compiling dependencies and produced no executable. It is not test
  evidence. The accepted baseline uses a full `git archive` snapshot plus all
  checked-out submodules under `/home/hejiahui/.codex-snapshots/` and an
  independent `/home/hejiahui/.codex-builds/` directory.

## Environment

| Item | Value |
| --- | --- |
| Compiler | WSL GCC 11.4.0 |
| Generator | Ninja through CMake 3.22.1 |
| Build mode | Debug, shared libraries on, static libraries off |
| CTest Node/npm | WSL v12.22.9 / 8.5.1 |
| Desktop Node/npm | v22.13.1 / 11.1.0 |
| Emscripten | unavailable in the PowerShell environment (`emcc` not found) |

## Existing Test Baseline

| Scope | Result | Evidence | Owner for gaps |
| --- | --- | --- | --- |
| LSP interface binary | 112 pass, 0 `Fail -` | direct Unity runner | none |
| LSP project features binary | 60 pass, 0 `Fail -` | direct Unity runner | none |
| LSP advanced editor binary | 73 pass, 0 `Fail -` | direct Unity runner | none |
| stdio smoke | pass | CTest 17 | none |
| stdio inline-value semantic smoke | fail | CTest 18 | `optimize/04-editor-feature-correctness` |
| stdio diagnostic-fix smoke | pass | CTest 19 | none |
| stdio position-encoding smoke | pass | CTest 20 | none |
| stdio type-hierarchy smoke | pass | CTest 21 | none |
| stdio protocol inventory | pass | CTest 22 | none |
| aggregate `language_server` | infrastructure blocked | CTest 16 stops at missing `zr_vm_language_server_symbol_table_test` because Task 1 intentionally built only the four plan targets | Task 6 full acceptance matrix |
| extension unit tests | 30 pass, 0 fail | `npm --prefix zr_vm_language_server_extension run test:unit` | none |
| extension TypeScript | pass | `npx tsc -p . --noEmit` from extension directory | none |

## Worktree State At Capture

`git status --short` was nonempty. The pre-existing changes were grouped as
parser/core/reference documentation, LSP interface and semantic files,
language-server tests, Rust binding files, test fixtures, and three unrelated
untracked acceptance or generated-support paths. The Task 1 write set was
created after this observation and is limited to this acceptance record, the
milestone record, `tests/CMakeLists.txt`, and
`tests/language_server/stdio_protocol_inventory.js`. No pre-existing dirty
path is staged or committed by this task.

### Recorded Failure: Inline Value

- Test: `language_server_stdio_inline_value_semantic_smoke`.
- Input: computed-member expression statement in the existing stdio fixture.
- Expected: `textDocument/inlineValue` exposes computed-member payload and
  reference facts.
- Actual: only `[ { "text": "reference read" } ]` is returned.
- Owner: `docs/plans/lsp/optimize/04-editor-feature-correctness.md`; no
  capability declaration or response fallback is changed by this baseline.

### Recorded Infrastructure Gap: Aggregate Suite

- Test: `language_server`.
- Expected: full suite runner executes every registered language-server binary.
- Actual: it stops at `zr_vm_language_server_symbol_table_test` because the
  Task 1 build intentionally contains only stdio, interface, project-feature,
  and advanced-editor targets.
- Owner: `docs/plans/lsp/optimize/06-modularization-performance-and-acceptance.md`.
- This is not classified as a semantic regression and is not waived; Task 6
  must build the full suite before it can claim aggregate green.

## Capability Crosswalk

| Existing plan | Completed evidence retained | Pending scope carried into optimize |
| --- | --- | --- |
| `00-current-state.md` | current source and protocol tests remain authoritative | honest capability declaration and reproducible aggregate evidence |
| `01-semantic-inference-core.md` | L8 external callable facts are committed | remaining query/provider coverage and fallback removal |
| `02-diagnostics-and-code-actions.md` | structured diagnostic and safe-fix leaves remain linked | full registry, unresolved fix families, workspace-edit validation |
| `03-robustness-and-position.md` | historical L6 snapshot/cancellation/performance leaves remain linked | fresh aggregate matrix, semantic payload gaps, parity evidence |
| `04-debug-and-repl.md` | formal debug evaluation leaves remain independent | protocol lifecycle and capability compatibility must not weaken debug gates |
| `05-implementation-blueprint.md` | L1/L2/L3/L6 partial records remain linked | L4-L8 global convergence remains incomplete |

Historical leaf records are retained by their existing owner directories:
`01-semantic-core` (22 records), `02-diagnostics` (22), `03-robustness`
(32), and `04-debug-and-repl` (22). This baseline neither supersedes nor
relabels those records; the optimize plan consumes their committed evidence and
assigns any remaining work only through the named optimize task owners above.

The `stdio_protocol_inventory.js` CTest is the Task 1 executable inventory. It
requires every runtime-declared capability to have an explicit native marker,
WASM worker marker state, lifecycle classification, and owning optimize plan.
It reports, but does not waive, known identity-resolve and workspace-notification
overclaim candidates. Task 2 replaces this temporary static table with the
canonical registry; Task 4 withdraws unsupported declarations.

### Inventory Result

- Native `initialize` declares 33 top-level capabilities. All 33 map to an
  explicit native initialization or dispatch marker; there are no unclassified
  native declarations.
- Twelve declarations currently lack their exact worker request marker:
  call hierarchy, color, declaration, on-type formatting, implementation,
  inline completion, inline value, linked editing, moniker, signature help,
  type definition, and type hierarchy. They remain parity work for
  `05-native-web-capability-parity.md`; absence is recorded, not inferred away.
- Five advertised contracts are overclaim candidates: workspace symbol, inlay
  hint, document link, and code lens `resolveProvider`, plus workspace-folder
  change notifications. Task 4 owns their withdrawal or material
  implementation.
