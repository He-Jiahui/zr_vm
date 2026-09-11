---
plan_id: optimize
task: plan03-task03-sub36
status: partial-acceptance
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_module_ranges.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
tests:
  - tests/language_server/test_lsp_project_module_summary_range_cases.h
  - tests/language_server/test_lsp_interface.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Task 3.36 Acceptance Evidence

## Scope

This record covers exact source declaration ranges in `projectModules`.
Explicit source module declarations use the parser's module-name token range;
implicit modules and unavailable/stale AST state retain the conservative file
origin fallback. Binary metadata URI/range projection, native virtual
projection, parser source/binary origin producers and multi-definition
relations are outside this submilestone.

## Test Inventory

- `LSP Project Modules Publish Exact Source Declaration Range` opens the
  `lsp_ownership` project, finds the `main` source summary, checks its source
  navigation URI, and asserts the UTF-16 LSP range `(0,7)-(0,11)`.
- The existing project module summary and source-bootstrap cases continue to
  exercise binary/native summaries and opened-source records in the same
  interface runner.

## Tooling Evidence

GCC built and linked the focused interface target after CMake discovered the
new project range source. The runner exits 1 only for the two pre-existing
functional baseline failures listed in the milestone record; the new exact
range case and its neighboring project cases pass. A separate RED run failed
at the new assertion before the implementation. Clang, MSVC, full project
runner, sanitizer-clean accounting and the 16-target gate remain pending for
this submilestone.

## Acceptance Decision

The source-summary range slice is accepted for a scoped commit. It does not
advance the parent Task 3/7/8 status or waive the existing interface baseline
failures and cross-toolchain gates.
