---
plan_id: debug-04-zr-debug
record_id: 2026-06-21-script-debug-library-baseline
status: completed
completed_at: 2026-06-21 23:35 +08:00
source_plans:
  - docs/plans/debug/04-script-debug-library.md
evidence_scope: historical-baseline
---

# Script Debug Library Baseline

## 可复用结论

- script-visible traceback/getinfo/local/upvalue/hook functions and trusted/sandboxed descriptors exist.
- current descriptor metadata can seed the target `zr.debug` native library contract.

## 证据入口

- `zr_vm_lib_debug/src/zr_vm_lib_debug/module.c`
- `tests/debug`

## Migration note

The current module name is bare `debug`; target imports use `zr.debug`. Existing behavior must be preserved through a module migration diagnostic/alias period, not compiler string special cases.

