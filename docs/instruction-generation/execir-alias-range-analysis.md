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
guards whenever the query is not a positive proof.

Range and shape facts are generation-scoped. A bounds proof requires known
lower and upper bounds for both index and length, matching generations, no
arithmetic overflow, non-negative bounds, and `index.upper < length.lower`.
Missing lower bounds, mutable lengths, overflow, or stale generations return
false. Shape facts require nonzero type/layout/shape identities and are
discarded on generation invalidation. Range propagation and GVN are subsequent
stages.

## Conservative GVN and guard use

The initial GVN pass only folds syntactically identical pure scalar operations
within one basic block. It rejects memory/effect-token users, `LOAD`, calls,
allocation, barriers, drop, throwing operations, GC, suspension, and all
terminators. A duplicate is rewritten to `COPY` of the prior result while
retaining its original result ID; this avoids assuming that physical instruction
order proves dominance for arbitrary later uses. Invalid storage/ranges and
allocation failure return a structured diagnostic without partial use rewrites.

The bounds-elision API is proof-only: it does not delete an instruction and
returns false for incomplete range facts. Callers retain the original guard on
false and emit their own missed/blocked remark with the relevant source ID.
