---
related_code:
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir.h
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir_scalar.h
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis3.h
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis4.h
  - zr_vm_core/include/zr_vm_core/module.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis3.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis4.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/01-schema-relocation.md
  - user: 2026-09-27 first persistent canonical ExecIR slice
  - user: 2026-09-28 EIS4 fixed scalar ADD payload
tests:
  - tests/library/test_ssa_exec_ir_artifact_v6.c
  - tests/library/test_ssa_exec_ir_artifact_v6_add.inc
  - tests/library/test_ssa_schema_relocation.c
  - tests/acceptance/ssa-artifact-v6-canonical-exec-ir.md
  - tests/acceptance/ssa-artifact-v6-eis3-counted-cfg.md
  - tests/acceptance/ssa-artifact-v6-eis4-scalar-add.md
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
because the envelope wire fields have not changed; each EIS1, EIS2, EIS3, or
EIS4 payload has its own magic and version.

The reader checks that a nonzero element count has a nonzero element width,
matching the writer's rule. It builds the view privately and publishes it
only after the entire directory and both optional byte hashes pass. Every
failure clears the caller's view; on success, section pointers borrow the
input buffer and remain valid only while those bytes remain alive.

`artifact_exec_ir_scalar.c` dispatches among fixed width canonical graph
payload codecs. EIS1 remains the original 412 byte one block CONSTANT then
RETURN encoding. EIS2 is a distinct 564 byte payload for one no argument i64
function whose entry block has an unconditional BRANCH to a second block with
CONSTANT then RETURN. Each block records its instruction, predecessor,
successor, dominator, and terminator fields; the pools store one result,
operand, successor, and predecessor ID. EIS3 is implemented in
`artifact_exec_ir_scalar_eis3.c`; it encodes one fixed three-block conditional
fork with eight explicit counts, three constants, three values, three blocks,
six instructions, and ordered successor/predecessor pools in 996 bytes. It is
not a general CFG format. EIS4 is implemented in
`artifact_exec_ir_scalar_eis4.c`; it encodes one exact 716 byte graph with
constants 20 and 22 followed by ADD and RETURN, three values, and one block
with empty edge ranges. It is not general arithmetic serialization. The writer
rejects nonzero or unencoded graph fields
and selects a version only after verifying its exact shape. Each decoder
builds a temporary `SZrExecIrModule`, checks its exact shape, runs the ExecIR
verifier, and transfers ownership only after success. The outer
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

Neither this raw API nor the EIS1/EIS2/EIS3/EIS4 loader resolves executable
relocations. The raw fixture's resolver maps a token to a test index; it does
not validate a callable ABI or install a VM/native/AOT target.
The canonical loader rejects any BINDINGS, STATE_MAPS, RELOCATIONS, or EXEC_BC
section. Full 08.01 relocation validation and loader publication remain
planned work.
