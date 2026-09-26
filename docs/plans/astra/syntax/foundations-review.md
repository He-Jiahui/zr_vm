---
related_code:
  - zr_vm_parser/src/zr_vm_parser/place.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_conflicts.c
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
  - zr_vm_core/src/zr_vm_core/property_reference.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/place.c
plan_sources:
  - docs/plans/astra/index.md
  - docs/plans/syntax/README.md
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
  - docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
  - docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md
  - docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md
  - docs/plans/syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md
  - docs/plans/syntax/2026-07-19-07-comprehensive-syntax-reference-fixture-design.md
tests:
  - tests/parser/test_place_cfg_graph.c
  - tests/parser/test_reference_loan_nll.c
doc_type: milestone-detail
---

# Syntax Foundations Review

Review date: 2026-09-05. Initial review baseline: `c95e5387` plus the existing
working-tree changes listed in the parent plan. Review is in progress. The July
and August milestone results are historical evidence, not a claim that the
September working tree passes those gates.

## Confirmed Source Findings

### F01: Dereference after divergent projections loses alias uncertainty

Priority: P1. Status: regression planned; production ownership assigned by the
coordinating agent.

`ZrParser_PlaceGraph_Overlap` in `zr_vm_parser/src/zr_vm_parser/place.c` returns
`disjoint` immediately when two paths on the same base diverge on a field,
constant index, or tuple element. It does not examine later dereferences. For
example, `base.fieldA.deref` and `base.fieldB.deref` have distinct pointer slots,
but the referenced storage can alias. The current result is `disjoint`; the
plan's default dereference contract requires `unknown` without an alias proof.

This result is safety-relevant: `semantic_ir_loan_conflicts.c` skips conflict
diagnostics exactly when overlap is `ZR_PARSER_PLACE_DISJOINT`. The same query
also participates in reborrow source narrowing. The defect is in the shared
Place query, not a particular language spelling or built-in type.

The owning contract is Syntax 01 section 5.2: distinct fields are normally
disjoint, while dereference and native aliases default to conservative
overlap/unknown. Syntax 02 section 7 treats unknown overlap as conflict unless
range/alias facts prove separation. The graph currently has no referent alias
identity that would justify retaining the field-slot disjointness after a
dereference.

Repair plan:

1. Add a failing leaf regression to `tests/parser/test_place_cfg_graph.c` for
   divergent field, constant-index, and tuple paths with a later dereference on
   either path. Assert query symmetry and preserve non-dereferenced disjointness.
2. Preserve disjointness when the shared dereference is before the divergence,
   such as `base.deref.fieldA` versus `base.deref.fieldB`, and no later
   dereference discards that storage proof.
3. In `place.c`, inspect only the suffix after a disjoint projection result and
   return unknown when either suffix dereferences. Reuse the existing
   dereference scan for unrelated bases. Do not add runtime borrow checks or
   new public metadata.
4. Update `docs/parser-and-semantics/place-cfg-graph.md` and write the unique
   acceptance record after actual red/green commands are available.
5. Validate `zr_vm_place_cfg_graph_test`, `zr_vm_reference_loan_nll_test`,
   `zr_vm_pre_semantic_ir_test`, and `zr_vm_reference_receiver_call_boundary_test`
   with the coordinating agent's fresh WSL GCC/Clang builds. Run the required
   Windows compatibility smoke through integration ownership.

Reference review includes Rust's `rustc_borrowck/src/places_conflict.rs` and
C#/.NET ref-field support. Rust's disjoint-place shortcut depends on its own
borrow model; ZR explicitly permits ordinary GC aliases and its Place graph
already returns unknown for dereferences from different bases. The repair
keeps ZR's established conservative contract rather than copying Rust's
stronger alias assumptions.

## Plan Coverage Ledger

The complete source/test mapping and remaining contract findings will be
recorded here as the bounded review proceeds. Syntax 01-07 ownership remains
open until the current build evidence is available.
