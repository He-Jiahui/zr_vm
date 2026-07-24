# 06A-M1 Migration Inventory Implementation Plan

> **For agentic workers:** execute this plan task-by-task from the existing `main` checkout. Steps use
> checkbox syntax and must be updated only with direct evidence.

**Goal:** Produce one deterministic, repository-wide inventory of legacy ZR syntax and classify every
in-scope occurrence as `machineApplicable`, `maybeIncorrect`, `requiresReview`, `blocked`, or
`targetNotPromoted`, without changing the compiler's accepted syntax.

**Architecture:** A standalone Python audit tool walks explicitly selected language-bearing source,
fixture, and current-document-code inputs. It lexes ZR text rather than applying raw regular-expression
replacement, preserves host and embedded ranges, and emits a stable JSON/text manifest that maps each
legacy form to its target plan and promotion state. M1 is inventory only: it neither writes source files
nor enters the parser, compiler, CLI, or LSP production path.

**Tech Stack:** Python 3 standard library, JSON, deterministic UTF-8 repository traversal, existing
PowerShell/WSL validation, and checked-in fixture manifests.

---

## Scope and Boundary

This is `06A M1` from
`docs/plans/syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md`.

- Include current `.zr` source, project fixtures, benchmark ZR inputs, library script resources,
  embedded ZR test strings, and current `zr` fenced documentation snippets.
- Keep migration fixtures, legacy parser negatives, historical plans, generated output, binary artifacts,
  third-party code, build directories, and `.codex` out of the current-source result. Their exclusions
  are explicit report entries, never silent omissions.
- Treat `%` modulo, comments, arbitrary strings, and an ordinary `$` outside a recognized construct as
  non-findings. Do not report them as migration candidates.
- An unrecognized legacy-looking construct is a `blocked` finding with a stable reason. The tool never
  emits an `unknown` classification.
- Target plans 08, 10, 11, 12, 13, and 14 are not promoted at M1; their known schemas are recorded as
  `targetNotPromoted(planId)`, not offered as safe edits.
- M1 makes no parser/compiler semantic change, no source rewrite, no artifact rewrite, and no LSP code
  action. Those belong to 06A M2/M3 and 06B.

## Exact Initial Write Set

- Create: `scripts/syntax_migration_inventory.py`
- Create: `tests/scripts/test_syntax_migration_inventory.py`
- Create: `tests/fixtures/syntax_migration_inventory/source/current_forms.zr`
- Create: `tests/fixtures/syntax_migration_inventory/source/legacy_forms.zr`
- Create: `tests/fixtures/syntax_migration_inventory/source/ignored_forms.zr`
- Create: `tests/fixtures/syntax_migration_inventory/embedded/embedded_legacy_fixture.c`
- Create: `tests/fixtures/syntax_migration_inventory/docs/current-snippets.md`
- Create: `tests/fixtures/syntax_migration_inventory/expected/inventory.json`
- Create: `docs/parser-and-semantics/syntax-migration-inventory.md`
- Create: `tests/acceptance/2026-07-24-syntax-06a-m1-migration-inventory.md`
- Create: `docs/plans/syntax/06-percent-migration-lsp-fixtures/m1-migration-inventory.md`
- Modify: `docs/parser-and-semantics/index.md`
- Modify: `docs/plans/syntax/06-percent-migration-lsp-fixtures/m1-migration-inventory-implementation-plan.md`

No parser, compiler, runtime, CLI, LSP production, CMake, generated, or foreign Syntax-plan path is in
the initial M1 write set. If repository evidence requires one, stop and coordinate the exact extension
before editing it.

### Task 1: Define the inventory protocol and fixture taxonomy

**Files:**
- Create: `scripts/syntax_migration_inventory.py`
- Create: `tests/scripts/test_syntax_migration_inventory.py`
- Create: `tests/fixtures/syntax_migration_inventory/source/current_forms.zr`
- Create: `tests/fixtures/syntax_migration_inventory/source/legacy_forms.zr`
- Create: `tests/fixtures/syntax_migration_inventory/source/ignored_forms.zr`
- Create: `tests/fixtures/syntax_migration_inventory/embedded/embedded_legacy_fixture.c`
- Create: `tests/fixtures/syntax_migration_inventory/docs/current-snippets.md`

- [x] **Step 1: Add failing unit tests for the stable JSON protocol.**

  Define expected fields for every finding:

  ```json
  {
    "schemaVersion": 1,
    "sourceKind": "zrSource|embeddedZrFixture|currentDocumentation",
    "file": "relative/forward/slash/path",
    "range": {"start": {"line": 1, "column": 1}, "end": {"line": 1, "column": 8}},
    "legacyForm": "percentImport",
    "classification": "machineApplicable",
    "targetPlan": "01-05|06A|08|10|11|12|13|14",
    "reason": "stable_machine_readable_reason"
  }
  ```

  Test deterministic ordering by `(file, start line, start column, legacyForm)`, schema version `1`,
  no `unknown` classification, and a report summary containing `findings`, `exclusions`, and
  `classificationCounts`.

- [x] **Step 2: Run the new test and prove the missing command fails.**

  Run:

  ```text
  python tests/scripts/test_syntax_migration_inventory.py
  ```

  Expected: nonzero exit because `scripts/syntax_migration_inventory.py` does not yet exist.

- [x] **Step 3: Implement only protocol parsing and fixture discovery.**

  Add standard-library-only dataclasses/records for `SourceKind`, `MigrationClassification`,
  `InventoryFinding`, and `InventoryExclusion`. Give the scanner explicit root categories:

  ```text
  zrSource: tests/**/*.zr, fixtures/projects/**/*.zr, benchmarks/**/*.zr, module script resources
  embeddedZrFixture: C/C++ test string literals that contain recognized legacy ZR tokens
  currentDocumentation: fenced ```zr blocks outside historical plans
  exclusions: migration fixtures, legacy negatives, docs/plans history, generated, binary, third_party,
              build, .git, .codex
  ```

  Normalize UTF-8 paths to repository-relative forward slashes. Do not mutate or rewrite any scanned
  file.

- [x] **Step 4: Make the protocol tests pass.**

  Run the test command from Step 2. Expected: exit `0`, with protocol, ordering, source-kind, and
  exclusion assertions passing.

### Task 2: Lex recognized legacy forms without text-replacement false positives

**Files:**
- Modify: `scripts/syntax_migration_inventory.py`
- Modify: `tests/scripts/test_syntax_migration_inventory.py`
- Modify: `tests/fixtures/syntax_migration_inventory/source/legacy_forms.zr`
- Modify: `tests/fixtures/syntax_migration_inventory/source/ignored_forms.zr`
- Modify: `tests/fixtures/syntax_migration_inventory/embedded/embedded_legacy_fixture.c`
- Modify: `tests/fixtures/syntax_migration_inventory/docs/current-snippets.md`

- [x] **Step 1: Add RED coverage for lexical boundaries.**

  Cover every M1 form family in executable fixtures:

  ```text
  %module, %import, %async, %await, %extern, %test, %compileTime, %func,
  %owned, %release, %upgrade, %weak, %shared, %detach, %unique,
  %in, %ref, %out, %borrow, %loan, %type, %using,
  legacy getter/setter, func/keywordless definition, definition arrows,
  $Type(...), $(expr)(...), bare Type(...), new Struct(...), native prototype factory
  ```

  Include `%` modulo, comments, quoted strings, backtick strings, escaped C fixture strings, current
  `zr` documentation fences, migration/legacy fixture exclusions, and a malformed `%unknown` form.
  Assert modulo/comments/strings do not become findings and malformed legacy spelling is `blocked`.

- [x] **Step 2: Run the lexical tests and prove they fail.**

  Run the command from Task 1 Step 2. Expected: assertions fail until the lexer distinguishes code from
  comments/strings and recognizes the complete M1 form inventory.

- [x] **Step 3: Implement context-aware scanners.**

  Implement a compact ZR lexical state machine for code, line comment, block comment, quoted string,
  escaped character, and backtick string. Extract C/C++ string literal payloads only under test/fixture
  roots and retain host-file positions. Parse only current `zr` fenced blocks in documentation roots.
  Recognize `$` only when followed by a TypeRef-like token or `(` expression marker; leave other `$`
  text untouched.

- [x] **Step 4: Make the lexical tests pass.**

  Run the command from Task 1 Step 2. Expected: all listed families appear once with exact ranges; false
  positives remain zero; malformed form is reported as `blocked` rather than `unknown`.

### Task 3: Classify every recognized form by target promotion ownership

**Files:**
- Modify: `scripts/syntax_migration_inventory.py`
- Modify: `tests/scripts/test_syntax_migration_inventory.py`
- Create: `tests/fixtures/syntax_migration_inventory/expected/inventory.json`

- [x] **Step 1: Add RED classification matrix tests.**

  Assert this stable M1 policy:

  | Form family | M1 classification | target owner |
  |---|---|---|
  | `func`, definition-arrow, `%in/%ref/%out`, owner generic spellings, safe paired property | `machineApplicable` candidate | 01-05 / 06A |
  | dynamic call/prototype, ambiguous call marker, unsafe property storage, union/plugin/field `%using` | `requiresReview` or `blocked` | 06A |
  | `%type`, `%extern`, `%compileTime`, `%async/%await`, generator, `%test` | `targetNotPromoted` | 08, 10, 11, 12, 13, 14 |
  | malformed or obsolete form without a semantics-preserving target | `blocked` | 06A |

  Verify a target-not-promoted finding carries its exact plan id and cannot be counted as a machine edit.

- [x] **Step 2: Run the classification tests and prove they fail.**

  Run the command from Task 1 Step 2. Expected: failure until every recognized family receives a stable
  classification, reason, and target plan.

- [x] **Step 3: Implement a declarative rule table.**

  Add rule records keyed by lexical form and local structural context. Rules return a stable reason,
  target plan, and one of the five allowed classifications. Reject any attempt to serialize another
  classification. Emit `targetNotPromoted` for known downstream forms even when their replacement text
  is known; this M1 tool never calls it `machineApplicable`.

- [x] **Step 4: Make the classification matrix pass and freeze a synthetic golden.**

  Run the test command. Expected: exit `0`; compare the synthetic fixture output byte-for-byte with
  `tests/fixtures/syntax_migration_inventory/expected/inventory.json` after normalizing only the
  repository root.

### Task 4: Close the real-repository inventory baseline

**Files:**
- Modify: `scripts/syntax_migration_inventory.py`
- Modify: `tests/scripts/test_syntax_migration_inventory.py`
- Modify: `tests/fixtures/syntax_migration_inventory/expected/inventory.json`
- Create: `docs/parser-and-semantics/syntax-migration-inventory.md`
- Modify: `docs/parser-and-semantics/index.md`

- [x] **Step 1: Add RED tests for repository walk closure.**

  Exercise the real repository in a temporary JSON output location. Assert every selected input is
  either represented in `scannedFiles` or appears exactly once in `exclusions`; assert every finding has
  a valid classification; assert `unknownCount == 0`; assert no generated/binary file is scanned as
  source; and assert repeated runs produce byte-identical output.

- [x] **Step 2: Run the repository closure tests and prove they fail.**

  Run:

  ```text
  python tests/scripts/test_syntax_migration_inventory.py --repository E:\Git\zr_vm
  ```

  Expected: failure until the complete root policy, exclusions, and stable real-repository baseline are
  implemented.

- [x] **Step 3: Implement deterministic repository reporting.**

  Add:

  ```text
  python scripts/syntax_migration_inventory.py --root . --format json --output <file>
  python scripts/syntax_migration_inventory.py --root . --format text
  ```

  The JSON report includes schema version, scanner version, selected roots, all scanned files,
  exclusions with reasons, sorted findings, classification counts, target-plan counts, and
  `unknownCount: 0`. Generate the checked-in baseline only after reviewing every count and all blocked/
  target-not-promoted categories; do not curate the result by hiding findings.

- [x] **Step 4: Document the tool and make real-repository closure pass.**

  Document roots, exclusion rationale, five classifications, target-promotion rules, report schema,
  deterministic invocation, and the non-authority boundary: M1 does not modify input or approve edits.
  Add the document to the parser-and-semantics index. Run Task 4 Step 2 and the normal fixture suite;
  both must exit `0` with a stable baseline.

### Task 5: Freeze M1 evidence, record status, and commit

**Files:**
- Create: `tests/acceptance/2026-07-24-syntax-06a-m1-migration-inventory.md`
- Create: `docs/plans/syntax/06-percent-migration-lsp-fixtures/m1-migration-inventory.md`
- Modify: `docs/plans/syntax/06-percent-migration-lsp-fixtures/m1-migration-inventory-implementation-plan.md`

- [x] **Step 1: Run M1 focused validation on GCC/Clang/MSVC-hosted Python.**

  Run fixture, real-repository, JSON determinism, exclusion, no-unknown, and report-format tests in
  WSL GCC/Clang build environments and Windows PowerShell Python. The inventory script is independent
  of compiler output, but all three host runs must produce the same normalized JSON SHA-256.

- [x] **Step 2: Record only verified evidence.**

  Under `## 状态与产出记录`, record completion time, `completed`, scanned/excluded/finding counts,
  classification and target-plan counts, byte-identical report hashes, commands, and any pre-existing
  unrelated baseline. Do not state that 06A M2/M3 or 06B is complete.

- [x] **Step 3: Audit exact ownership and commit M1.**

  Require `git diff --check`, checked-in JSON determinism, exact staging, no foreign Syntax plan,
  build, generated, or binary path, and empty shared index before/after committing:

  ```text
  feat(syntax): inventory legacy migration forms
  ```

## M1 Promotion Gate

M1 is complete only when every selected current source/fixture/document input is explicitly scanned or
explicitly excluded, every recognized legacy form has a stable classification/target/reason, the report
has no unknown items, synthetic and real-repository reports are deterministic, documentation explains
the contract, and the exact-path commit is verified. Passing M1 does not permit any source rewrite,
legacy parser removal, or formal parser cutover.

## 状态与产出记录

- 完成时间：2026-07-24 12:18 +08:00
- 状态：completed
- 完成项目：
  - Task 4 repository closure：811 scanned、336 explicit exclusions、770 findings、unknown=0、blocked=0；
    checked-in report 的 raw SHA-256 为
    `23397b06e154a009e2db276f7f2ae8917b4c088e75066e17851e8e4a106657e2`。
  - Task 5 host matrix：Windows Python 3.14、WSL GCC 11.4/Python 3.10、WSL Clang 14.0/Python 3.10
    各 5/5 通过，三个 host 的 raw JSON hash 相同；CRLF output mismatch 已由 UTF-8/LF CLI regression
    修复并覆盖。
  - 已完成 14-path exact ownership audit，`git diff --cached --check` 与 working-tree `git diff --check`
    均通过；三份 foreign Syntax design drafts、generated fixture binary 与 `.codex` 均未暂存。
  - Task 3 classification matrix：`python tests/scripts/test_syntax_migration_inventory.py` 已于
    2026-07-24 通过 5/5；fixture golden 固定所有已识别形式到五种分类、目标计划和稳定原因。
  - 已从 Syntax 06 设计的 06A M1 建立独立执行计划；明确 M1 是只读 migration inventory，
    不改变正式 parser/compiler/runtime/LSP 语义。
  - 已冻结初始 write set、全仓语言输入类别、显式排除项、五级 classification 和 08/10-14
    targetNotPromoted 边界；待 TDD RED 证据后进入实现。
