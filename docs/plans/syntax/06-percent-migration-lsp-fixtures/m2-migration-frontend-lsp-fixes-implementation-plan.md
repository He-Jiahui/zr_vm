# 06A-M2 Migration Frontend + LSP Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use `executing-plans` to implement this plan task by
> task in the existing `main` checkout. Steps use checkbox (`- [ ]`) syntax and may be marked complete
> only with direct command evidence.

**Goal:** Build a parser-owned legacy migration adapter that produces deterministic structured edit
plans and serves `zr migrate syntax`, while validating that the existing LSP revision-guarded
code-action infrastructure remains the sole serialization path when formal migration diagnostics arrive
at the 06B cutover.

**Architecture:** `zr_vm_parser` owns a lexical/structural adapter and plan model. It masks comments,
strings, and modulo expressions; records exact ranges, bindings, target promotion, applicability,
reason, related ranges, source hash, and non-overlapping edits; and delegates paired property accessors
to the existing structured property-migration producer. The CLI renders/applies only
`machineApplicable` plans. M2 deliberately does not inject planner findings into normal current-document
LSP diagnostics: the formal parser migration diagnostic is a 06B/M4 cutover responsibility. Existing
generic code-action and workspace-edit snapshot tests remain the handoff proof and will serialize the
parser-owned structured fix when that formal diagnostic exists.

**Tech Stack:** C17 parser/core arrays and structured diagnostics, existing CLI command/app layers,
existing LSP semantic analyzer and code-action snapshots, Unity C tests, Node stdio smoke, Python M1
fixture protocol, CMake, WSL GCC/Clang, and Windows MSVC.

---

## Scope and Non-Negotiable Boundary

This is M2 of 06A. It implements the design document sections 3, 4, 6.4, 6.6 and the M2 promotion
gate; it does **not** perform the 06B cutover described in section 5.

- `machineApplicable` means the adapter has both a token/structural proof and a current parser/compiler
  witness. It is the only classification that `--write` can publish. In this M2 slice that witness
  exists for `%owned -> resource`; other legacy families remain reportable but non-writable until their
  target grammar and semantic binding proof are available.
- `maybeIncorrect`, `requiresReview`, `blocked`, and `targetNotPromoted` serialize their complete
  report facts but never add a machine edit. Known targets owned by 08, 10, 11, 12, 13, and 14 remain
  `targetNotPromoted` even when replacement spelling is known.
- The adapter must report `%module`, `%import`, `%func`, legacy `func`/keywordless declarations,
  definition/type arrows, `%owned`, `%release`, `%upgrade`, `%weak`, `%shared`, `%detach`, `%unique`,
  `%in/%ref/%out`, `%borrow/%loan/%borrowed/%loaned`, `%type`, `%using`, `$` constructor forms,
  bare type calls, `new Struct`, native prototype factories, and legacy property accessors.
- It must classify every family through all relevant `pass`, `ambiguous`, `blocked`, and
  `targetNotPromoted` fixture cases. Comments, strings, backticks, modulo, and arbitrary `$` remain
  non-items. Unknown `%identifier` is `blocked`, never an implicit rewrite.
- The existing property producer remains authoritative for paired getter/setter edits. The generic
  adapter imports its structured result into CLI reports and skips a second property candidate. M2
  keeps LSP consumption unchanged; the formal cutover reuses its single parser diagnostic.
- `--write` verifies document hash immediately before application, applies edits from largest start
  offset to smallest, rejects overlap, reparses/typechecks each written source, and leaves non-machine
  items untouched. `--check` and reports do not write. Generated, binary, golden, and excluded M1 roots
  do not write unless `--include-generated` is explicitly implemented and the item is still safe.
- M2 adds no formal parser compatibility branch, no compiler/repl/import fallback, and no source-wide
  `strstr`/regular-expression replacement path. The formal syntax cutover remains 06B.

## Exact Planned Write Set

- Create: `zr_vm_parser/include/zr_vm_parser/legacy_migration.h`
- Create: `zr_vm_parser/src/zr_vm_parser/migration/legacy_migration.c`
- Create: `tests/parser/test_legacy_migration.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h`
  (Windows DLL export declarations for existing document-aware conversion helpers only)
- Modify: `zr_vm_cli/src/zr_vm_cli/command/command.h`
- Modify: `zr_vm_cli/src/zr_vm_cli/command/command.c`
- Modify: `zr_vm_cli/src/zr_vm_cli/app/app.c`
- Create: `zr_vm_cli/src/zr_vm_cli/migration/migration.h`
- Create: `zr_vm_cli/src/zr_vm_cli/migration/migration.c`
- Modify: `tests/cli/test_cli_args.c`
- Create: `tests/cli/test_cli_syntax_migration.c`
- Create: `tests/cli/syntax_migration_smoke.js`
- Create: `tests/fixtures/syntax_migration_frontend/input/machine_forms.zr`
- Create: `tests/fixtures/syntax_migration_frontend/input/review_and_blocked_forms.zr`
- Create: `tests/fixtures/syntax_migration_frontend/expected/machine_forms.json`
- Create: `docs/parser-and-semantics/legacy-syntax-migration-frontend.md`
- Modify: `docs/parser-and-semantics/index.md`
- Create: `docs/cli-and-tooling/syntax-migration-command.md`
- Modify: `docs/cli-and-tooling/index.md`
- Create: `tests/acceptance/2026-07-24-syntax-06a-m2-migration-frontend-lsp-fixes.md`
- Modify: `docs/plans/syntax/06-percent-migration-lsp-fixtures/m2-migration-frontend-lsp-fixes.md`
- Modify: `docs/plans/syntax/06-percent-migration-lsp-fixtures/m2-migration-frontend-lsp-fixes-implementation-plan.md`

No current source, fixture, golden, artifact, foreign Syntax-plan draft, `.codex`, or generated `bin/`
path is a M2 write target. If an existing CMake source glob does not discover a new source, add only the
nearest required target registration and test it before continuing.

## Stable Parser API

Task 1 defines these exact API shapes. Implementations may use private helpers, but consumers must not
inspect source text after receiving a plan.

```c
typedef enum EZrLegacyMigrationApplicability {
    ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE = 0,
    ZR_LEGACY_MIGRATION_MAYBE_INCORRECT,
    ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
    ZR_LEGACY_MIGRATION_BLOCKED,
    ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED
} EZrLegacyMigrationApplicability;

typedef struct SZrLegacyMigrationItem {
    SZrString *diagnosticCode;
    SZrString *oldConstructKind;
    SZrString *targetConstructKind;
    SZrString *oldTargetBindingKind;
    SZrString *targetPlanId;
    SZrString *reason;
    SZrFileRange range;
    SZrFileRange relatedRange;
    EZrLegacyMigrationApplicability applicability;
    TZrTypeId resolvedTargetTypeId;
    TZrBool hasResolvedTargetTypeId;
    SZrStructuredDiagnosticFix fix;
    TZrBool hasFix;
} SZrLegacyMigrationItem;

typedef struct SZrLegacyMigrationPlan {
    SZrArray items; /* SZrLegacyMigrationItem */
    TZrUInt64 sourceHash;
    TZrBool hasOverlap;
} SZrLegacyMigrationPlan;

ZR_PARSER_API TZrBool ZrParser_LegacyMigration_PlanSource(
        SZrState *state, const TZrChar *source, TZrSize sourceLength,
        SZrString *sourceName, SZrLegacyMigrationPlan *outPlan);
ZR_PARSER_API void ZrParser_LegacyMigration_PlanFree(
        SZrState *state, SZrLegacyMigrationPlan *plan);
ZR_PARSER_API TZrBool ZrParser_LegacyMigration_ApplyMachineEdits(
        SZrState *state, const SZrLegacyMigrationPlan *plan,
        const TZrChar *source, TZrSize sourceLength,
        TZrChar **outText, TZrSize *outLength);
```

`fix` is initialized and freed with the existing structured diagnostic routines. A non-machine item has
`hasFix == ZR_FALSE`; no empty or placeholder fix is used as a loophole. `targetNotPromoted` has its
exact plan identifier and is never converted to `ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE`.

### Task 1: Freeze the parser-owned plan contract and token boundary RED cases

**Files:**
- Create: `zr_vm_parser/include/zr_vm_parser/legacy_migration.h`
- Create: `tests/parser/test_legacy_migration.c`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: Write focused failing parser tests for plan facts and lexical boundaries.**

  Add Unity cases that call the proposed API against this exact input:

  ```zr
  %module app.tools
  %owned class FileHandle {}
  let upgraded = %upgrade(weakHandle);
  let remainder = 7 % 2;
  // %module ignored.comment
  let text = "%release(value)";
  %async fn delayed(): int {}
  %unknown thing;
  ```

  Assert deterministic item order, exact offset ranges, `percentModule -> targetNotPromoted/06B`,
  `owned -> resource`, `upgrade -> requiresReview`, no items for modulo/comment/string,
  `percentAsync -> targetNotPromoted/12`, and `unknown -> blocked/06A` with no fix. Test all five
  applicability enum values, initialized `sourceHash`, empty overlap flag, and a repeated plan that is
  byte-equivalent after rendering.

- [x] **Step 2: Register and run the parser test to prove RED.**

  Register `zr_vm_legacy_migration_test` with the parser/core link helper. Run:

  ```text
  cmake --build .codex/build-syntax06a-m2-gcc --target zr_vm_legacy_migration_test -j 8
  ./.codex/build-syntax06a-m2-gcc/bin/zr_vm_legacy_migration_test
  ```

  Expected: compilation fails because `legacy_migration.h` and its API do not exist.

- [x] **Step 3: Declare only the public plan model.**

  Create the header exactly as defined in **Stable Parser API**. Do not add scanner logic to the header,
  do not expose lexer internals, and do not modify formal parser acceptance.

- [x] **Step 4: Rebuild and confirm the same test remains RED at link time.**

  Run the command from Step 2. Expected: unresolved `ZrParser_LegacyMigration_*` symbols, proving the
  test reaches the intended missing production implementation.

### Task 2: Implement token-aware classification, edits, overlap defense, and idempotence

**Files:**
- Create: `zr_vm_parser/src/zr_vm_parser/migration/legacy_migration.c`
- Modify: `tests/parser/test_legacy_migration.c`

- [x] **Step 1: Extend parser RED coverage to every migration family.**

  Add table-driven test rows with `source`, `oldConstructKind`, `applicability`, `targetPlanId`,
  `expectedEdit` or no edit, covering:

  ```text
  machine: %owned resource declaration shell with a current parser/compiler witness
  review/maybe: %import local/dynamic/conflicting alias, %func, func definition, keywordless
                definition, definition/type arrows, %release/%upgrade/%weak/%shared,
                %in/%ref/%out, %detach, %unique, %borrow/%loan, $StaticType(...),
                $(prototype)(...), bare Type(...), new Struct(...), %using role ambiguity
  targetNotPromoted: %module, %type, %extern, %compileTime, %async, %await, %test,
                      native prototype factory
  blocked: unknown directive, overlap, malformed `$`, invalid standalone import, no safe target
  ```

  For machine rows, call `ZrParser_LegacyMigration_ApplyMachineEdits`, assert descending-range
  application has no range corruption, parse/typecheck the result with the current parser/compiler test
  harness, then plan the output again and assert zero machine edits. For every non-machine row assert
  `hasFix == ZR_FALSE` and identical source after application.

- [x] **Step 2: Run the expanded test and prove the classifier is RED.**

  Run the command from Task 1 Step 2. Expected: plan is empty or classifications/fixes are absent.

- [x] **Step 3: Implement one lexer adapter and declarative rule table.**

  In `legacy_migration.c`, implement a single character-state lexer with `CODE`, `LINE_COMMENT`,
  `BLOCK_COMMENT`, `QUOTED_STRING`, `BACKTICK_STRING`, and escaped-character transitions. It emits
  recognized token spans to a rule table carrying diagnostic code, old/target kind, target plan,
  reason, applicability, and an edit constructor. Implement these structural checks before adding a
  machine edit:

  ```text
  %module: retain a complete declaration fact as `targetNotPromoted/06B` until the current parser
           accepts the target declaration grammar.
  %import: only a module-scope literal import with a stable, unassigned alias can become
           let alias = import("path");; local/conditional/dynamic/conflicting cases are review.
  function: distinguish definition return delimiter, callable type delimiter, and anonymous body.
  ownership: only the `%owned` declaration shell has an M2 current-parser/compiler witness;
             builtin conversions and `%detach/%unique` stay review until canonical source proof is
             represented by the adapter.
  $construct: static, dynamic, and unresolved targets remain review/blocked until a canonical TypeRef
             binding is available.
  ```

  Sort items by source start/end/kind, reject intersecting machine edit ranges with a `blocked`
  `migration_overlapping_edits` item, construct the output once by copying source segments from right to
  left, and hash the original bytes before any mutation. Do not scan a line for a replacement string,
  and do not use parser errors as a classification fallback.

- [x] **Step 4: Make the complete parser suite GREEN.**

  Run Task 1's command, then:

  ```text
  ctest --test-dir .codex/build-syntax06a-m2-gcc -R "^(legacy_migration|property_consumer_contracts)$" --output-on-failure
  ```

  Expected: all M2 plan, lexical boundary, classification, overlap, current-parser/typecheck, and
  idempotence assertions pass. Existing property tests remain green without a duplicate plan item.

### Task 3: Add the `zr migrate syntax` command, deterministic JSON/text report, and guarded writes

**Files:**
- Modify: `zr_vm_cli/src/zr_vm_cli/command/command.h`
- Modify: `zr_vm_cli/src/zr_vm_cli/command/command.c`
- Modify: `zr_vm_cli/src/zr_vm_cli/app/app.c`
- Create: `zr_vm_cli/src/zr_vm_cli/migration/migration.h`
- Create: `zr_vm_cli/src/zr_vm_cli/migration/migration.c`
- Modify: `tests/cli/test_cli_args.c`
- Create: `tests/cli/test_cli_syntax_migration.c`
- Create: `tests/cli/syntax_migration_smoke.js`
- Create: `tests/fixtures/syntax_migration_frontend/input/machine_forms.zr`
- Create: `tests/fixtures/syntax_migration_frontend/input/review_and_blocked_forms.zr`
- Create: `tests/fixtures/syntax_migration_frontend/expected/machine_forms.json`
- Modify: `tests/CMakeLists.txt`

- [x] **Step 1: Add CLI argument and report RED tests.**

  Extend `test_cli_args.c` to require exactly this parse contract:

  ```text
  zr_vm_cli migrate syntax <path> --check --format json
  zr_vm_cli migrate syntax <path> --write --format text
  ```

  Reject missing `syntax`, missing path, `--check` plus `--write`, invalid `--format`, unsupported
  language direction, runtime/compile/debug modifiers, and duplicate migration modes. In the new
  migration test, write a temporary source with `%module`, `%owned`, `%async`, and modulo; assert JSON
  schema version, fields `diagnosticCode/file/range/oldConstructKind/targetConstructKind/
  oldTargetBindingKind/resolvedTargetTypeId/applicability/targetPlanId/targetPromotionGate/edits/
  relatedDeclarations/reason`, stable ordering, and no write under `--check`.

  Add a Node smoke that invokes the built executable, reads its JSON, confirms `--write` transforms only
  machine items, and invokes the command a second time to assert no edits and a byte-identical output.

- [x] **Step 2: Run the CLI tests and prove RED.**

  Run:

  ```text
  cmake --build .codex/build-syntax06a-m2-gcc --target zr_vm_cli_args_test zr_vm_cli_syntax_migration_test zr_vm_cli_executable -j 8
  ./.codex/build-syntax06a-m2-gcc/bin/zr_vm_cli_args_test
  ./.codex/build-syntax06a-m2-gcc/bin/zr_vm_cli_syntax_migration_test
  node tests/cli/syntax_migration_smoke.js ./.codex/build-syntax06a-m2-gcc/bin/zr_vm_cli
  ```

  Expected: argument parsing fails because migration mode and runner do not exist.

- [x] **Step 3: Implement a dedicated CLI mode and runner.**

  Add `ZR_CLI_MODE_MIGRATE_SYNTAX` and command fields for path, check/write, JSON/text, generated
  opt-in, and fixed `legacy -> current` direction. `migration.c` recursively reads only eligible text
  sources under the supplied path, consults M1 exclusion categories, invokes
  `ZrParser_LegacyMigration_PlanSource`, and emits normalized forward-slash JSON or a one-line-per-item
  text report. It writes only after source hash revalidation, machine-only application, and current
  parser/typecheck succeeds; failure leaves the original file unchanged and is a report item, not a
  partial overwrite. Dispatch this mode in `app.c`; no compile/repl/project route invokes it.

- [x] **Step 4: Make CLI tests and smoke GREEN.**

  Run Task 3 Step 2. Expected: exact argument failures, check/no-write behavior, JSON golden, text
  report, machine-only write, target-not-promoted nonpublication, overlap rejection, and second-run
  idempotence all pass.

### Task 4: Preserve the LSP handoff boundary and validate existing guarded code actions

**Files:** Modify `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h`
only to export already-existing document-aware conversion helpers from the Windows DLL. Existing LSP
tests and stdio smoke remain validation consumers; M2 adds no migration diagnostic or LSP text rewrite.

The initial M2 draft attempted to project the adapter directly into ordinary current-document LSP
diagnostics. That would turn accepted repository source into new warnings before the formal parser
cutover, contradicting the design's 06B/M4 ownership of migration diagnostics. Do not add a semantic
analyzer adapter, an LSP-only text rewrite, or migration-specific LSP fixtures in 06A.

- [x] **Step 1: Establish the non-cutover RED boundary.**

  Confirm that speculative projection makes existing current-source LSP contracts fail, then remove the
  speculative adapter. The parser adapter remains a CLI/frontend fact producer and is not a substitute
  for a formal parser diagnostic.

- [x] **Step 2: Re-run the existing LSP interface contract without speculative diagnostics.**

  Run:

  ```text
  cmake --build .codex/build-syntax06a-m2-gcc --target zr_vm_language_server_lsp_interface_test -j 8
  ./.codex/build-syntax06a-m2-gcc/bin/zr_vm_language_server_lsp_interface_test
  ```

  Expected: the pre-existing structured-fix/code-action snapshot contract passes without a new
  migration diagnostic. Direct evidence: the target exited 0 on 2026-07-24.

- [x] **Step 3: Verify the LSP stdio surface in the final toolchain matrix.**

  Run the existing stdio smoke in each final isolated build. It validates revision-guarded code-action
  serialization remains available to the future formal parser diagnostic without changing current
  migration diagnostic visibility.

### Task 5: Document the contract, run evidence matrix, record M2, and commit

**Files:**
- Create: `docs/parser-and-semantics/legacy-syntax-migration-frontend.md`
- Modify: `docs/parser-and-semantics/index.md`
- Create: `docs/cli-and-tooling/syntax-migration-command.md`
- Modify: `docs/cli-and-tooling/index.md`
- Create: `tests/acceptance/2026-07-24-syntax-06a-m2-migration-frontend-lsp-fixes.md`
- Modify: `docs/plans/syntax/06-percent-migration-lsp-fixtures/m2-migration-frontend-lsp-fixes.md`
- Modify: `docs/plans/syntax/06-percent-migration-lsp-fixtures/m2-migration-frontend-lsp-fixes-implementation-plan.md`

- [x] **Step 1: Write module docs with machine-readable headers.**

  Add YAML front matter with `related_code`, `implementation_files`, `plan_sources`, `tests`, and
  `doc_type`. Describe the five applicability levels, all form families and target gates, source-hash
  and non-overlap guarantees, property producer ownership, CLI modes/exit behavior, LSP snapshot
  behavior, and the explicit non-cutover boundary. Add both documents to their functional indexes.

- [x] **Step 2: Run focused validation in three toolchains.**

  Configure isolated M2 directories, then run parser, CLI, and LSP tests/smokes:

  ```text
  wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B .codex/build-syntax06a-m2-gcc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build .codex/build-syntax06a-m2-gcc --target zr_vm_legacy_migration_test zr_vm_property_consumer_contracts_test zr_vm_cli_args_test zr_vm_cli_syntax_migration_test zr_vm_cli_executable zr_vm_language_server_lsp_interface_test zr_vm_language_server_stdio -j 8 && ctest --test-dir .codex/build-syntax06a-m2-gcc -R "^(legacy_migration|property_consumer_contracts|cli_args|cli_syntax_migration)$" --output-on-failure && ./.codex/build-syntax06a-m2-gcc/bin/zr_vm_language_server_lsp_interface_test && node tests/cli/syntax_migration_smoke.js ./.codex/build-syntax06a-m2-gcc/bin/zr_vm_cli && node tests/language_server/stdio_smoke.js ./.codex/build-syntax06a-m2-gcc/bin/zr_vm_language_server_stdio ./.codex/build-syntax06a-m2-gcc/bin/zr_vm_cli'
  wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B .codex/build-syntax06a-m2-clang -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build .codex/build-syntax06a-m2-clang --target zr_vm_legacy_migration_test zr_vm_property_consumer_contracts_test zr_vm_cli_args_test zr_vm_cli_syntax_migration_test zr_vm_cli_executable zr_vm_language_server_lsp_interface_test zr_vm_language_server_stdio -j 8 && ctest --test-dir .codex/build-syntax06a-m2-clang -R "^(legacy_migration|property_consumer_contracts|cli_args|cli_syntax_migration)$" --output-on-failure && ./.codex/build-syntax06a-m2-clang/bin/zr_vm_language_server_lsp_interface_test && node tests/cli/syntax_migration_smoke.js ./.codex/build-syntax06a-m2-clang/bin/zr_vm_cli && node tests/language_server/stdio_smoke.js ./.codex/build-syntax06a-m2-clang/bin/zr_vm_language_server_stdio ./.codex/build-syntax06a-m2-clang/bin/zr_vm_cli'
  powershell -NoProfile -Command '. .\\.codex\\skills\\using-vsdevcmd\\scripts\\Import-VsDevCmdEnvironment.ps1; cmake -S . -B .codex\\build-syntax06a-m2-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl; cmake --build .codex\\build-syntax06a-m2-msvc --target zr_vm_legacy_migration_test zr_vm_property_consumer_contracts_test zr_vm_cli_args_test zr_vm_cli_syntax_migration_test zr_vm_cli_executable zr_vm_language_server_lsp_interface_test zr_vm_language_server_stdio --parallel 8; ctest --test-dir .codex\\build-syntax06a-m2-msvc -R "^(legacy_migration|property_consumer_contracts|cli_args|cli_syntax_migration)$" --output-on-failure; .\\.codex\\build-syntax06a-m2-msvc\\bin\\zr_vm_language_server_lsp_interface_test.exe; node tests\\cli\\syntax_migration_smoke.js .\\.codex\\build-syntax06a-m2-msvc\\bin\\zr_vm_cli.exe; node tests\\language_server\\stdio_smoke.js .\\.codex\\build-syntax06a-m2-msvc\\bin\\zr_vm_language_server_stdio.exe .\\.codex\\build-syntax06a-m2-msvc\\bin\\zr_vm_cli.exe'
  ```

  Result: GCC 11.4, Clang 14.0.0, and MSVC 19.44 all exited zero for the focused CTest group, the direct
  LSP interface executable, CLI migration smoke, and LSP stdio smoke. The MSVC run first exposed missing
  DLL exports for the already-used document conversion helpers and Windows separator handling for
  generated-directory exclusion; both were fixed with the existing tests as reproductions. See the
  acceptance record for exact commands and output evidence.

- [x] **Step 3: Record acceptance evidence and M2 completion.**

  In the acceptance document include scope, pre-change RED, all pass/ambiguous/blocked/target-not-
  promoted cases, idempotence/overlap/hash/stale boundaries, tool versions, exact commands, stdout/exit
  evidence, and acceptance decision. In the milestone record write the actual completion time, status
  `completed`, items delivered, precise test results, and any residual baseline; do not claim M3/06B
  completion.

- [x] **Step 4: Audit exact paths and make the milestone commit.**

  Require a clean index before staging, stage only M2 files, run `git diff --cached --check`, prove the
  three user-owned Syntax draft files and fixture `bin/` are absent, commit, then verify a clean index:

  ```text
  feat(syntax): add migration frontend and lsp fixes
  ```

## M2 Promotion Gate

M2 is complete only when every inventory form has deterministic parser-owned plan output and executable
pass/ambiguous/blocked/target-not-promoted coverage; machine output parses/typechecks after application;
all non-machine output is nonpublishable; reports are deterministic and idempotent; CLI writes are hash-
guarded and machine-only; existing LSP structured-fix and stale-snapshot contracts pass unchanged; the
future formal parser diagnostic has no second LSP text-rewrite implementation; module docs and acceptance
evidence exist; and the exact M2 milestone commit is verified.
