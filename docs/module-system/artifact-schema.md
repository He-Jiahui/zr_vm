---
related_code:
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir.h
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir_scalar.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_parser/include/zr_vm_parser/artifact_exec_ir.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/artifact_encoding.c
  - zr_vm_core/src/zr_vm_core/artifact_identity.c
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/01-schema-relocation.md
  - user: 2026-09-27 first persistent canonical ExecIR slice
tests:
  - tests/library/test_ssa_exec_ir_artifact_v6.c
  - tests/library/test_ssa_schema_relocation.c
  - tests/parser/test_artifact_schema.c
  - tests/acceptance/ssa-artifact-v6-canonical-exec-ir.md
doc_type: module-detail
status: partial
---

# Canonical ExecIR artifact schema

The current ZRAF container schema is version 6. ZRO section 22,
`EXEC_IR_BUNDLE`, stores a nested ERI1 document containing one `EXEC_IR`
payload. The nested document carries AOT ABI 17, the module hash, and a hash
of the payload. ERI1 remains version 1 because its header and directory wire
layout did not change. EIS1 is the first graph payload version and has a fixed
412 byte little endian layout. It encodes IDs, metadata tokens, contracts,
source IDs, a scalar constant, a value, a block, and complete CONSTANT and
RETURN instructions. Runtime pointers and C struct padding are never written.

`ZrParser_ExecIr_WriteCanonicalZroFile` accepts a validated ZRO metadata
document with seven identity sections and an `SZrExecIrModule`. It supports
exactly one no argument i64 function with one entry block and CONSTANT then
RETURN. Every unsupported graph side table, map, binding, relocation,
additional function, or opcode is rejected. Encoding and validation finish
before a file is opened. The writer creates an exclusive temporary file in
the target directory, closes it, and publishes it by same directory rename;
a failed write leaves the previous target bytes intact.

The zero parameter count is an explicit contract row field, and the i64
return is checked in the decoded graph. Neither the signature token nor its
hash is interpreted as a native calling convention. This slice executes
through the ExecIR Oracle; native callable ABI lowering belongs to 07.02.

`ZrCore_Module_OpenExecIrArtifact` is the dedicated ZRAF entry. It requires
the caller's expected public identity, validates the outer ZRO and each
required section, checks the nested ABI and hashes, decodes EIS1 into a
temporary model, runs `ZrCore_ExecIr_VerifyModule`, compares the decoded
module/function contract with the outer metadata, then publishes the graph.
All failed reads leave the caller's empty graph empty. A schema 5 ZRAF is
rejected with `UNSUPPORTED_VERSION` and diagnostic expected/actual versions.
The separate historical `01ZR` `.zro` binary path remains handled by
`ZrCore_Module_ImportByPath`; this API rejects its magic.

This is one vertical slice of plan 08.01. It does not provide general CFG,
maps, binding, relocation resolution, ExecBC, package copy, AOT projection,
or `ImportByPath` migration. The legacy ERI1 raw section codec and relocation
unit test remain available for their existing callers; accepting raw bytes
there does not make those bytes an executable canonical graph.
