# AOT / Syntax Traceability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `docs/plans/aot` trace every current Syntax 01-14 contract and milestone while preserving independent AOT status and all historical evidence.

**Architecture:** Keep the AOT subsystem-oriented plan layout and add a canonical milestone crosswalk. `docs/plans/syntax/README.md` remains the semantic authority; AOT documents link to it, describe backend outputs, and never infer AOT completion from Syntax completion.

**Tech Stack:** Markdown, PowerShell coverage/link audits, Git exact-path commits.

---

### Task 1: Publish the canonical crosswalk and repair the AOT index

**Files:**
- Create: `docs/plans/aot/syntax-contract-traceability.md`
- Modify: `docs/plans/aot/index.md`

- [ ] **Step 1: Freeze the expected Syntax node set**

Use these exact 69 node identifiers in the detailed matrix:

```text
01/M1..M5
02/M1..M6
03/M1..M5
04/M1..M7
05/M1..M5
06A/M1..M3
06B/M4..M5
07A, 07B
08/M1..M5
09/M1..M5
10R/M1..M2
10F/M3
10C/M4..M5
11/M1..M5
12/M1..M6
13/M1..M4
14/M1..M4
```

- [ ] **Step 2: Write the detailed traceability table**

Every row must use this schema and one of the approved AOT states:

```markdown
| Syntax node | Canonical Syntax output / gate | AOT responsibility | AOT plan | Required AOT output / evidence | AOT status |
|---|---|---|---|---|---|
| `01/M1` | Canonical Type graph; link the exact design/record | Consume TypeId/TypeUse without spelling fallback | AOT 02, 08, 11 | Type table roundtrip, invalid/open type rejection, four-path parity | `partially_verified` |
```

Allowed states are `blocked_by_syntax`, `ready`, `in_progress`, `partially_verified`, `completed`, and `not_applicable`. Do not use `completed` without a linked record proving the entire row.

- [ ] **Step 3: Rewrite the index projection**

The index must contain exactly fourteen theme rows and explicitly name `06A/06B`, `07A/07B`, and `10R/10F/10C`. Link the detailed matrix as the authoritative AOT crosswalk and state that an unmapped future Syntax node blocks AOT plan promotion.

- [ ] **Step 4: Run the first coverage check**

Run:

```powershell
$text = Get-Content -Raw docs/plans/aot/syntax-contract-traceability.md
1..14 | ForEach-Object { if ($text -notmatch ('`{0:d2}/|`{0:d2}[A-Z]?' -f $_)) { throw "missing theme $_" } }
$rows = Select-String -Path docs/plans/aot/syntax-contract-traceability.md -Pattern '^\| `(?:0[1-9]|1[0-4])'
if ($rows.Count -ne 69) { throw "expected 69 milestone rows, found $($rows.Count)" }
```

Expected: exit 0 and no output.

- [ ] **Step 5: Commit the matrix and index**

```powershell
git add -- docs/plans/aot/index.md docs/plans/aot/syntax-contract-traceability.md
git diff --cached --name-only
git commit -m "docs(aot): map all syntax contracts"
```

Expected staged paths: exactly the two listed files.

### Task 2: Align the shared AOT plan rules

**Files:**
- Modify: `docs/plans/aot/00-current-state.md`
- Modify: `docs/plans/aot/01-design-principles.md`
- Modify: `docs/plans/aot/06-implementation-blueprint.md`

- [ ] **Step 1: Update the baseline and gap model**

Add the current Syntax 01-14 authority, distinguish verified AOT baselines from target closure, and list missing 11-14 coverage plus the split phase nodes as the reason for this revision.

- [ ] **Step 2: Add traceability invariants**

Add rules stating that every AOT work package declares Syntax inputs, every Syntax node has one crosswalk row, status transitions are independent, and upstream changes reopen affected AOT rows.

- [ ] **Step 3: Replace the isolated M1-M7 ledger with two-axis gates**

Keep the backend order Foundation -> Typed execution -> Cleanup -> ABI/backend -> Runtime bridges -> Packaging/stripping -> Convergence. For every backend gate, list the required Syntax readiness nodes and link the detailed matrix. State that 06B and 07B require all relevant 08-14 backend rows, while 06A, 07A, and 10R can advance independently under their Syntax rules.

- [ ] **Step 4: Audit terminology**

Run:

```powershell
Select-String -Path docs/plans/aot/00-current-state.md,docs/plans/aot/01-design-principles.md,docs/plans/aot/06-implementation-blueprint.md -Pattern '01-10|syntax 10$|06[^AB]|07[^AB]|10[^RFC]'
```

Expected: no stale statement treating Syntax 06, 07, or 10 as an indivisible gate; incidental AOT document numbers are reviewed manually.

- [ ] **Step 5: Commit the shared plan rules**

```powershell
git add -- docs/plans/aot/00-current-state.md docs/plans/aot/01-design-principles.md docs/plans/aot/06-implementation-blueprint.md
git diff --cached --name-only
git commit -m "docs(aot): align promotion gates with syntax"
```

Expected staged paths: exactly the three listed files.

### Task 3: Add reverse Syntax links to non-overlapping subsystem plans

**Files:**
- Modify: `docs/plans/aot/02-typed-value-and-layout.md`
- Modify: `docs/plans/aot/03-instruction-set-refactor.md`
- Modify: `docs/plans/aot/04-semir-and-c-backend.md`
- Modify: `docs/plans/aot/05-ownership-gc-and-bridge.md`
- Modify: `docs/plans/aot/08-generic-sharing.md`
- Modify: `docs/plans/aot/09-memory-management.md`
- Modify: `docs/plans/aot/10-reflection.md`
- Modify: `docs/plans/aot/11-metadata.md`

- [ ] **Step 1: Add one `Syntax 上游追踪` table to each file**

Use this exact shape:

```markdown
## Syntax 上游追踪

| Syntax 节点 | 本计划消费的稳定输入 | 本计划退出责任 |
|---|---|---|
| [`03/M1`](../syntax/...design.md) | TypeLayout/copy-map contract | Target layout and aggregate ABI parity |
```

Only include relevant nodes. End each section with a link to `syntax-contract-traceability.md`; do not duplicate Syntax semantics.

- [ ] **Step 2: Include upper Syntax designs**

Ensure the combined tables mention Syntax 11 compile-time generated declarations, 12 Task/frame/scheduler, 13 iterator frames, and 14 TestManifest/runner trimming where relevant. Do not stop at Syntax 10.

- [ ] **Step 3: Preserve completion records verbatim**

Do not edit any file under `02-type-layout/`, `07-codegen/`, `08-generics/`, `09-memory/`, `10-reflection/`, `11-metadata/`, or `12-stripping/`. Existing top-level completion links remain intact.

- [ ] **Step 4: Commit the non-overlapping subsystem plans**

```powershell
git add -- docs/plans/aot/02-typed-value-and-layout.md docs/plans/aot/03-instruction-set-refactor.md docs/plans/aot/04-semir-and-c-backend.md docs/plans/aot/05-ownership-gc-and-bridge.md docs/plans/aot/08-generic-sharing.md docs/plans/aot/09-memory-management.md docs/plans/aot/10-reflection.md docs/plans/aot/11-metadata.md
git diff --cached --name-only
git commit -m "docs(aot): trace syntax inputs by subsystem"
```

Expected staged paths: exactly the eight listed files.

### Task 4: Merge traceability into concurrently edited AOT 07 and 12 plans

**Files:**
- Modify after the A7.2D exact commit: `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`
- Modify after the A7.2D exact commit: `docs/plans/aot/12-code-stripping.md`

- [ ] **Step 1: Wait for and verify the concurrent exact commit**

Run:

```powershell
git status --short -- docs/plans/aot/07-codegen-register-model-and-environment-isolation.md docs/plans/aot/12-code-stripping.md
git log -1 --oneline -- docs/plans/aot/07-codegen-register-model-and-environment-isolation.md docs/plans/aot/12-code-stripping.md
```

Expected before editing: both paths clean and the latest commit contains the A7.2D constructor bitmap layout verifier record.

- [ ] **Step 2: Re-read both files and append traceability sections**

Add only `Syntax 上游追踪` tables. Preserve the new 2026-08-01 completion links and all prior A7/S records byte-for-byte outside the appended sections.

- [ ] **Step 3: Verify the concurrent record remains present**

```powershell
Select-String -Path docs/plans/aot/07-codegen-register-model-and-environment-isolation.md,docs/plans/aot/12-code-stripping.md -Pattern '2026-08-01','constructor bitmap','A7.2D'
```

Expected: the concurrent completion record links are present in both files.

- [ ] **Step 4: Commit only the two merged files**

```powershell
git add -- docs/plans/aot/07-codegen-register-model-and-environment-isolation.md docs/plans/aot/12-code-stripping.md
git diff --cached --name-only
git commit -m "docs(aot): trace codegen and stripping syntax inputs"
```

Expected staged paths: exactly the two listed files.

### Task 5: Validate exact coverage and historical preservation

**Files:**
- Verify: `docs/plans/aot/*.md`
- Verify read-only: `docs/plans/syntax/**`
- Verify read-only: `docs/plans/aot/*/*.md`

- [ ] **Step 1: Check topic, phase, and milestone coverage**

Run the 69-row check from Task 1, then verify the phase strings:

```powershell
$text = Get-Content -Raw docs/plans/aot/syntax-contract-traceability.md
'06A','06B','07A','07B','10R','10F','10C' | ForEach-Object { if ($text -notmatch [regex]::Escape($_)) { throw "missing phase $_" } }
```

Expected: exit 0.

- [ ] **Step 2: Check every AOT top-level plan has reverse traceability**

```powershell
$files = Get-ChildItem docs/plans/aot -File -Filter '*.md' | Where-Object { $_.Name -notin @('index.md','syntax-contract-traceability.md') }
$missing = $files | Where-Object { (Get-Content -Raw $_.FullName) -notmatch 'Syntax 上游追踪|Syntax Contract|syntax-contract-traceability' }
if ($missing) { throw "missing reverse traceability: $($missing.Name -join ', ')" }
```

Expected: exit 0.

- [ ] **Step 3: Check relative Markdown links**

Resolve every relative `.md` link in changed AOT files against its containing directory. Expected: zero missing targets.

- [ ] **Step 4: Check task scope and whitespace**

```powershell
git status --short -- docs/plans/aot docs/plans/syntax docs/superpowers
$subjects = @(
  'docs(aot): map all syntax contracts',
  'docs(aot): align promotion gates with syntax',
  'docs(aot): trace syntax inputs by subsystem',
  'docs(aot): trace codegen and stripping syntax inputs'
)
foreach ($subject in $subjects) {
  $commit = git log -1 --format='%H' --fixed-strings --grep=$subject
  if (-not $commit) { throw "missing task commit: $subject" }
  git diff-tree --no-commit-id --name-only -r $commit
  git show --check --format= $commit
}
```

Expected: no whitespace errors; no task commit modifies `docs/plans/syntax` or AOT historical record files; unrelated pre-existing Syntax work remains untouched.

- [ ] **Step 5: Record final validation**

Update the active `.codex/sessions/20260801-0150-aot-syntax-traceability.md` with the exact commit IDs and coverage result, then delete it if no other session requires a handoff. Do not commit the active session note.
