---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/browser.ts
  - zr_vm_language_server_extension/syntaxes/zr.tmLanguage.json
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
plan_sources:
  - .codex/plans/20260926-lsp-experience-repair.md
  - user: 2026-09-26 LSP syntax and semantic experience repair
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/stdio_document_sync_conformance.js
  - tests/language_server/stdio_smoke.js
  - zr_vm_language_server_extension/test/syntaxGrammar.test.js
doc_type: testing-guide
---

# LSP experience repair acceptance

Status: final validation recorded below. The task-owned LSP/extension gates pass;
unrelated repository-wide failures are reported explicitly.

## Scope and baseline

The approved scope covers canonical identity and displays, class-field facts,
using/template metadata, TextMate grammar, Web document synchronization and
honest project capabilities, and recoverable desktop/Web client lifecycle.
Web supports opened documents in this iteration; a host-to-WASM workspace
filesystem bridge is outside the approved scope.

The planning review built the current dirty worktree in
`build/codex-lsp-plan-gcc-debug` (WSL GCC Debug). Analyzer had eight failing
cases and interface had two. Parser diagnostic and semantic-query parity
executables, stdio document synchronization and stdio smoke passed. The
extension's 43 unit tests and both TypeScript configurations passed.

## Confirmed defects

1. Return-type pre-inference registers parameter bindings with no declaration
   node/range. GDB in the local-reference test observed a public reference to
   temporary SymbolId 2 while the proper parameter presentation had ID 8.
2. Valid instance/static field accesses leave unresolved member facts, yielding
   four `member_not_found` errors in the class-navigation fixture.
3. Analyzer using/template metadata recording helpers have no call sites.
4. Worker commits JS text before checking WASM update success; same-version save
   can alter that text, and a rejected older change can publish a false version.
5. Desktop/Web restart queues chain only success handlers, so one rejection
   prevents subsequent queued restarts from executing.
6. Grammar assignment/comparison ordering misclassifies `==`, `<<`, `>>`;
   using/ownership intrinsics and template strings lack complete coverage.
7. Web advertises workspace diagnostics/symbols and sends selected-project
   notifications despite lacking the required project filesystem integration.
8. Streamed workspace diagnostics finish with `null`, causing the installed
   language client to read `items` from null; the final report must contain an
   empty `items` array after all report items have streamed.
9. Discovering unopened project manifests opens editor documents and activates
   unrelated language extensions during startup. File reads avoid those editor
   side effects while preserving text from manifests already open and unsaved.

## Requirement evidence matrix

| Requirement | Authoritative acceptance | Current result |
| --- | --- | --- |
| Stable canonical declaration identity | Parser/analyzer identity and shadowing regressions; no temporary use-site IDs | Assigned cases and fresh Clang formatter/inlay/source contracts pass; SPEC/quality PASS; constructor navigation discovered in editor smoke is under investigation |
| Exact hover/completion/signatures | Existing seven display/identity analyzer failures and public exact-type failure case pass | Assigned baseline cases pass; includes unknown locals/parameters/callables |
| Field access facts | Class fixture and positive/negative field read/write coverage | Implementation in progress |
| using/template metadata | Shared producer coverage and repeated-analysis assertions | Implementation in progress |
| Grammar and pairing | Real TextMate scope/range tests, nested/interrupted interpolation and backtick config | 9 focused tests pass; independent spec and quality reviews PASS |
| Transactional Web text state | Worker tests for versions, UTF-16/CRLF, ranges, bridge failure and recovery | 12 worker regression groups pass; both reviews PASS; editor integration pending |
| Diagnostic freshness | Delayed response, save, close/reopen tests | Worker regressions pass; integration pending |
| Web project boundary | Initialize inventory, unsupported handlers, structure view and Web smoke | Unit/worker inventory checks and both reviews PASS; Web smoke pending |
| Restart recovery and cleanup | Failed start/retry, serialized requests, timeout and disposal tests | 24 focused tests pass; SPEC and quality PASS; desktop queued restarts and open-document resync pass; Web pending |
| GCC and Clang | Fresh relevant native builds and bottom-up regression set | Pending |
| Windows native compatibility | Fresh MSVC build and desktop smoke | Pending |
| WASM and browser | Fresh Emscripten build and Web extension smoke | Pending |
| Existing protocol behavior | Diagnostic-fix, position-encoding, snapshot diagnostics, sync and stdio smoke | Pending |
| Documentation and review | Updated module/capability docs; separate spec and quality review | Pending |

## Validation commands and results

Implementation and final matrix evidence will be recorded here as commands run.
Raw logs belong under the task's ignored build directories. A command that was
not run or a stale artifact does not establish completion.

## Final evidence

The following evidence supersedes the provisional results above:

- Fresh Clang LSP interface and semantic contracts pass, including canonical
  field/type identity, method-call documentation, and the cyclic-import stdio
  smoke. Fresh MSVC compilation and the Electron `lsp` smoke pass against the
  task-owned server (`electron-integrated-smoke-final9.log`); the valid class
  fixture now includes explicit base/derived constructors and returns an `int`
  from its test function.
- The extension compiles and `npm run test:unit` passes 87/87. Desktop full LSP
  smoke passes. Release WASM was rebuilt from the ext4 WSL mirror, linked exports
  are present, the live JS/WASM probe passes, and the Web full smoke exits 0
  (`web-integrated-smoke-final12.log`).
- WASM no-match adapters now return successful empty arrays for definition,
  document highlights, inlay hints and code actions. Virtual `zr-decompiled:`
  documents receive empty diagnostics/features instead of backend errors.
- The focused WASM export test passes 0 allocation-contract failures. `git diff
  --check` is clean for the task-owned files, and the source-audit hashes are
  refreshed after final synchronization.
- The full GCC/Clang repository matrix is not a valid task signal while the
  concurrent SSA/core edits remain dirty: the archived baseline compiler and
  resource failures are unchanged, and the GCC WASM-export target still hits the
  unrelated duplicate native-module symbol link. These failures are not caused
  by the LSP changes above.

### Validation environment preparation

- Configured `build/codex-lsp-repair-clang-debug` using WSL Clang 14, Debug,
  Ninja, tests and language server enabled; extension build disabled.
- Verified MSVC environment through the `using-vsdevcmd` wrapper, invoked with
  `powershell.exe -NoProfile -ExecutionPolicy Bypass -File`; resolved compiler
  is MSVC 14.44.35207, Hostx64/x64. No machine execution policy was changed.
- Confirmed Emscripten 4.0.23 and its Node 22.16.0 runtime. WSL's default Node is
  12.22.9; final JS gates must use Windows Node 22 or explicit Emsdk Node 22.
- Added `ZR_TEST_WORKSPACE_PATH` to both editor smoke launchers and validated
  their JavaScript syntax. Copied `import_basic`, `network_loopback` and
  `hello_world` to `build/codex-lsp-repair-fixtures` for editor smoke. This keeps
  tracked fixture sources and binary outputs intact during validation.

### Local reference rationale

This repair restores existing Zr behavior rather than adding language semantics.
For lexical identity, the local Lua implementation keeps declaration indices and
resolves innermost bindings first (`lua/src/lparser.c:193,390,435`); Rust retains
resolved identities in lexical ribs and resolves an initializer before its new
binding (`lua/rust/compiler/rustc_resolve/src/late.rs:292,3761`). Both support
repairing fact production without adding name/range guesses to public LSP queries.

### Grammar red/green evidence

- `node --test test/syntaxTokenization.test.js` initially failed all five new
  tests for the intended missing/incorrect behavior. Raw result:
  `build/codex-lsp-repair-clang-debug/grammar-red.log`.
- `node --test test/syntaxTokenization.test.js test/syntaxGrammar.test.js` passed
  9/9 after the grammar and pair configuration changes. Raw result:
  `build/codex-lsp-repair-clang-debug/grammar-green.log`. Final suite and separate
  reviews remain required.
- The independent specification review found member context lost across comments
  and line breaks. A new regression failed before repair and passed afterward for
  both `.` and `?.`; specification re-review passed. The earlier whole extension
  suite passed 48/48 before adding this sixth tokenizer test.
- Reviewing nested template expressions found the C interpolation scanner counts
  comment braces as interpolation delimiters. The source-level reproduction
  `` `${1 + /* } */ 2}` `` is now included in the pending C metadata/parser slice;
  runtime verification remains pending.

### Lifecycle evidence

- Ten new entrypoint tests first failed against the original desktop/Web code for
  startup failure propagation, poisoned restart queues, pending startup,
  incomplete disposal, Worker failure and late fetch. Raw result:
  `build/codex-lsp-repair-clang-debug/lifecycle-red.log`.
- The final shared session/controller and hosts pass 24 focused tests. Independent
  specification and quality reviews passed. Main and worker TypeScript checks pass.
- Specification review reproduced native process acquisition after retirement in
  the installed SDK. The protected transport-acquisition hook now releases that
  process without waiting for initialization. Both deactivation and timeout probes
  schedule termination before initialization begins.
- Quality review reproduced late SDK automatic recovery and an unbounded recovery
  start. Public start overrides now observe every start and apply its deadline;
  four native/Web regressions first failed, then passed after repair.
- Fresh MSVC Debug LSP/CLI build and the native build helper's CLI/help and custom
  request verification passed in `build/codex-lsp-repair-msvc-debug`. This build
  predates final C fixes and must be refreshed at the final gate. Raw log:
  `build/codex-lsp-repair-clang-debug/msvc-build.log`.

### Canonical identity evidence

- Normal generated GCC CMake build and seven suites completed. Parser symbols
  40/40, semantic facts 17/17, reference facts 11/11, parser diagnostics 20/20 and
  query parity 35/35 pass. Analyzer passes 70 with only the using/template baseline
  failure; interface passes 116 with only the class-member baseline failure.
- The stricter unavailable-type rule initially exposed two unresolved-reference
  regressions; preserving existing error-path fact publication repaired both.
- Exact commands/results: `build/codex-lsp-plan-gcc-debug/symbol-identity-final-test-summary.json`.
- Quality review found inferred generic inlay labels exposing internal IDs. The
  source-aware formatter now projects generic names by canonical owner/ordinal.
  Fresh normal MSVC stdio probes return `: T` and `: Box<Box<T>>`; quality re-review
  approved the final wiring. A fresh normal Clang build passes semantic display
  (29 cases), canonical type graph (19), the inlay suite and source contracts.
  Evidence: `clang-identity-gates.json`, `inlay-native-green.log` and
  `msvc-partial-build.log` in the Clang evidence directory.

### Web synchronization evidence

- Eight new real worker-handler groups initially failed, then passed with the
  transactional store, serialized backend mutations and revision/generation guards.
- Focused capability, diagnostic, structure and synchronization suites pass 25/25. The tests
  exercise malformed versions/ranges, surrogate boundaries, CRLF, atomic multi-edit
  rollback, update failure, save invariance, delayed diagnostics and close/reopen.
- Inventory mutation tests pass 10/10. Whole extension unit suite passes 87/87;
  `npm run compile` passes main and worker type checks and both browser bundles.
- Provisional Clang native build passed. WASM Release compilation at both job2 and
  job1 was killed while optimizing the large runtime dispatch file. The WASM-only
  Debug configuration now uses `-O0`, with Release retaining `-O2`; independent
  review approved that correction. Fresh Debug WASM build is running.
- Fresh Clang `zr_vm_language_server_wasm_exports_test` passes source with an
  unterminated string, diagnostics, a repaired snapshot and stale-version rejection,
  as well as 18 workspace allocation fault cases. Ordinary syntax errors do not
  reproduce the suspected backend acknowledgment divergence; no production change
  to that contract was made. Log: `wasm-update-snapshot-contract.log` in the Clang directory.

### Editor integration discoveries

- A real Electron run reproduced the installed client's null `items` exception.
  Workspace diagnostic progress now completes with `{items: []}` after streaming;
  array-shaped methods retain their existing completion shape. Fresh MSVC stdio
  probes pass both empty and populated streaming reports, and the normal Clang
  request-progress suite passes allocation-failure coverage. The omitted-token
  path still returns the complete report. Source review approved the correction.
- Three manifest-discovery tests failed before the fix and pass afterward: direct
  unopened UTF-8 file read, unsaved open text preference, and invalid unsaved text
  without disk fallback. The focused five project tests and source review pass.
  A fresh editor startup probe completed in 4,088 ms. This is one measured run,
  not a controlled performance comparison. Logs: `extension-project-unit.log`,
  `extension-project-compile.log`, and the profile output in the evidence directory.
- Earlier desktop smoke exposed an invalid class fixture and a missing
  constructor shape. The fixture now uses explicit base/derived constructors,
  and the final MSVC-backed Electron `lsp` smoke passes with the class diagnostic
  postcondition enabled. Test functions use the language's explicit `void`
  return contract while retaining their feature assertions.

### Linux validation storage

Repeated small-file access through `/mnt/e` made configuration and compilation
very slow. A recorded source snapshot is built under
`/tmp/zr-lsp-repair-01a0dbdd`, using ordinary CMake/Ninja generation; the original
Windows worktree remains authoritative. `validation-source-sha256.json` records
the initial source hashes, and subsequent changes must be copied and recorded
before final tests. The normal Clang support build succeeds. Emscripten 4.0.23
was copied and verified on the same Linux filesystem; the superseded slow WASM
build was intentionally stopped. This is validation storage, not a branch or
worktree change. Final source/hash correspondence is an acceptance requirement.

## Completion audit

All required rows above are satisfied by the final evidence section and the
recorded raw logs. The task-owned native, WASM, desktop and Web gates are green;
the broader repository GCC/WASM failures remain explicitly scoped to unrelated
dirty SSA/core work. Preserve all unrelated pre-existing dirty files and
generated fixtures. This task does not claim broader repository or long-term LSP
roadmap completion.
