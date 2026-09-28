---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - tests/language_server/stdio_cyclic_import_smoke.js
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
plan_sources:
  - .codex/plans/20260926-lsp-experience-repair.md
tests:
  - tests/language_server/stdio_cyclic_import_smoke.js
doc_type: module-detail
---

# LSP project cycle loading

Project indexing can encounter a cycle while an imported module is being loaded.
The loader now records canonical module names that are active in the current
request. A re-entrant request for an active module returns the existing load
boundary instead of recursively reanalyzing the same source. Each load removes
its guard on success and on every failure path.

This keeps diagnostics, document symbols and semantic tokens responsive for
cyclic imports and prevents native stack overflow. The stdio smoke test opens a
three-document import cycle and requests document diagnostics, document symbols,
and full semantic tokens for the open document.
