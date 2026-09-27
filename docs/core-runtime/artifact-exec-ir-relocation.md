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

`artifact_exec_ir_scalar.c` adds EIS1, a fixed width canonical graph payload
for one no argument i64 CONSTANT then RETURN function. The writer rejects
nonzero or unencoded graph fields. The decoder builds a temporary
`SZrExecIrModule`, checks its exact shape, runs the ExecIR verifier, and
transfers ownership only after success. The outer
`ZrCore_Module_OpenExecIrArtifact` also checks ABI 17, public identity,
module/function contracts, hashes, and the ZRO section policy.

The existing `ZrCore_ArtifactExecIr_ValidateRelocations` API is separate and
remains a raw relocation prototype. It accepts a resolver and publishes
resolved indices only after callbacks succeed, but its code offset check is
against the whole document and its failure diagnostic loses the target token.
Neither this API nor the initial EIS1 loader resolves executable relocations.
The EIS1 loader rejects any BINDINGS, STATE_MAPS, RELOCATIONS, or EXEC_BC
section. Full 08.01 relocation validation and loader publication remain
planned work.
