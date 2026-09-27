---
related_code:
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir.h
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir_scalar.h
  - zr_vm_core/include/zr_vm_core/module.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/01-schema-relocation.md
  - user: 2026-09-27 first persistent canonical ExecIR slice
tests:
  - tests/library/test_ssa_exec_ir_artifact_v6.c
  - tests/library/test_ssa_schema_relocation.c
  - tests/acceptance/ssa-artifact-v6-canonical-exec-ir.md
  - tests/acceptance/ssa-artifact-v6-eri1-relocation-boundary.md
doc_type: module-detail
status: partial
---

# ExecIR bundle and relocation boundary

`artifact_exec_ir.c` implements the ERI1 nested document header and section
directory. It bounds the document to 64 MiB and 32 sections, rejects
duplicate or overlapping sections, and checks the stored ExecIR/ExecBC byte
hashes when present. Its `Read` result is a borrowed view of raw bytes, not a
verified executable graph. The generic ERI1 reader does not establish a
callable ABI or decode the ExecIR section's meaning. ERI1 remains schema 1
because the envelope wire fields have not changed; the payload has its own
EIS1 magic and version.

The reader checks that a nonzero element count has a nonzero element width,
matching the writer's rule. It builds the view privately and publishes it
only after the entire directory and both optional byte hashes pass. Every
failure clears the caller's view; on success, section pointers borrow the
input buffer and remain valid only while those bytes remain alive.

`artifact_exec_ir_scalar.c` adds EIS1, a fixed width canonical graph payload
for one no argument i64 CONSTANT then RETURN function. The writer rejects
nonzero or unencoded graph fields. The decoder builds a temporary
`SZrExecIrModule`, checks its exact shape, runs the ExecIR verifier, and
transfers ownership only after success. The outer
`ZrCore_Module_OpenExecIrArtifact` also checks ABI 17, public identity,
module/function contracts, hashes, and the ZRO section policy.

The existing `ZrCore_ArtifactExecIr_ValidateRelocations` API is separate and
remains a raw relocation prototype. Its `codeOffset` is relative to the
`EXEC_IR` section's byte start; an offset at or beyond that section's length
is invalid even when it lies inside the enclosing ERI1 document. It checks
every row before invoking any resolver callback. The callback receives the
stable target token, kind, expected hash, and expected contract hash; a
failure records the row and target token. Resolved indices are staged and
copied to the caller only after all callbacks succeed. This contract does
not promise rollback of a callback's own side effects, so resolvers must
avoid publishing process-local targets during validation.

Neither this raw API nor the initial EIS1 loader resolves executable
relocations. The raw fixture's resolver maps a token to a test index; it does
not validate a callable ABI or install a VM/native/AOT target.
The EIS1 loader rejects any BINDINGS, STATE_MAPS, RELOCATIONS, or EXEC_BC
section. Full 08.01 relocation validation and loader publication remain
planned work.
