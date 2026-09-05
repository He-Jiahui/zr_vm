# LSP Save Capability

## Scope

Plan 00 Task 4 Sub05 removes unhandled willSave notification publication while
preserving willSaveWaitUntil formatting and didSave document refresh.

## Baseline

The initial GCC fixture failed both capability publication cases, exit 1 with
2/4 failures. The actual formatting requests already passed.

## Test Inventory

Empty and save-aware client profiles check the complete sync declaration,
exact formatting edit/range, versioned diagnostics and an empty repeat edit.
Disk cases replace a cached class, wait for didSave-triggered diagnostics at
the next generation and assert the new exact definition range.

## Tooling Evidence

GCC 11.4, Clang 14 and MSVC 19.44 focused builds and suites pass 13/13 each.
After the disk cases were added, the final save fixture passes 6/6 on all
three binaries. A client-only mutation changes each second didSave to an
unknown notification: the two disk cases fail, while the other four pass.
All 29 frozen overlay paths match the workspace on Windows and WSL.

## Results

The declaration now reflects the real notification handler set. Review found
and then confirmed closure of the initial didSave observation gap. Exact
commands and source composition are in
[the milestone record](../../docs/plans/lsp/optimize/2026-09-05-plan00-task04-sub05-save-notification.md).

## Acceptance Decision

This capability subitem is accepted. Compiled inventory, integrated semantic
failures and the parent Plan 00 gate remain open. No later phase is promoted.
