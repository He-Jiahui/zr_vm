---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_alias.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_alias.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/02-gvn-range.md
doc_type: implementation-note
status: implemented
---

# ExecIR alias facts

Alias classification is parser-owned and uses stable base/projection IDs; it
never stores runtime pointers.  The lattice is `must-alias`, `disjoint`,
`may-alias`, and `unknown`.

Identical stable base and projection identities produce `must-alias`. Distinct
allocation or stack bases are `disjoint` only while both are stable and
unescaped. Distinct parameter or external-handle bases remain `unknown`
because callers and FFI may alias them. Distinct projections are disjoint only
when lowering supplies an explicit layout proof (`projectionDisjoint`) for the
same layout; projection IDs alone are not evidence.

Unknown writes, stale generation identities, invalid/unknown bases, or escaped
external locations force `unknown`. Consumers must therefore retain loads and
guards whenever the query is not a positive proof. This first batch defines
only the conservative query; range propagation and GVN are subsequent stages.
