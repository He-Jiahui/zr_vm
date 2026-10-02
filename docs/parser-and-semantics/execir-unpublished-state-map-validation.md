---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_ssa_builder_cfg.c
  - tests/parser/ssa_builder_unpublished_state_maps.inc
  - tests/acceptance/2026-10-02-ssa-unpublished-state-map-validation.md
doc_type: module-detail
---

# State-map validation before function publication

The source compiler can supply a SemanticIR function whose `symbolId` is zero.
`ZrParser_ExecIr_Build` constructs an unpublished candidate: its function ID
remains zero and its token remains the semantic symbol ID. `BuildModule` assigns
the actual ID, token, signature hash and contract generation only after the
candidate has passed construction and verification. Reserving a module slot
before that point would weaken the existing rollback guarantee.

State-map validation requires a nonzero function ID and token and a matching
map token, signature hash and generation. During candidate validation, the
builder therefore needs a private nonzero identity for both the function and
its map. Restoring a map token to zero after construction and then temporarily
changing only the function token to one makes the identities disagree, even
when the checkpoint storage itself is correct. This also affects empty maps.

`verify_unpublished_ssa` takes a shallow function copy and, when a map exists,
a shallow map copy. It substitutes the private ID and token in the function
view and synchronizes only the copied map token. The map entries and all other
storage remain shared, so the normal core verifier checks the actual candidate
instructions, SSA, effects, ownership and checkpoint contents. The copies are
stack values; they own no allocation and must never be freed as functions or
maps. The original identity fields and storage remain untouched on both success
and failure.

This operation does not repair signature, generation or checkpoint corruption.
Those fields retain their original values and continue through strict map
validation. Once `BuildModule` obtains the final contract, it publishes that
identity into the candidate and its owned map together, using the existing
transactional module construction path.

The focused builder regression uses an external value followed by `DROP` and
`RETURN`. The cleanup boundary must retain its before-effect, after-effect and
cleanup-complete entries. Raw construction must leave the unpublished function
and map identities at zero; module construction must publish matching final
identities. Changing a published checkpoint's source ID must produce
`STATE_MAP_INVALID` at the cleanup instruction, and restoring it must allow
verification again. This covers real checkpoint storage as well as identity.
