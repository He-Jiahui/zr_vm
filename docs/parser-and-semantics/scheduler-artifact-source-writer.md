---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scheduler_artifact.c
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c
  - tests/parser/test_artifact_schema_source_roundtrip.c
---

# Scheduler source artifact writer

`ZrParser_Writer_WriteSchedulerArtifactFile` creates a canonical `.zri` or
`.zro` document from an already compiled function that contains a resolved
`TaskScheduler.schedule` source fact. It is a separate writer from the legacy
VM `.zrb` stream and always delegates encoding and decoding invariants to the
canonical artifact schema.

## Identity join

The compiler fact records three provider identities: Scheduler, Task, and Job.
Each consists of the provider `TypeDef` token, its signature token and hash,
layout version/hash, and owning module signature hash. The compiler resolves
that identity before publishing the source fact, including deterministic
native-import provenance when a descriptor-backed provider has no module-init
summary. The writer projects fresh artifact-local TypeRef and TypeDef rows
directly from those complete facts; it does not look up a function metadata
record or rediscover an identity by source spelling.

This is intentionally an exact structural projection. Missing identity, an
incomplete layout, or a source fact without Task/Job/Scheduler facts returns
`ZR_ARTIFACT_STATUS_INVALID_ARGUMENT`. The writer never recovers by comparing
a type name, member name, source text, display string, runtime value category,
or a legacy metadata row.

## Contract rows

The document contains exactly the source-projected scheduler contract and Job
domain-transfer rows:

- the Scheduler contract retains ABI, policy, attached/isolated Send/Sync
  requirements, transport hash, and schedule contract hash;
- the Job transfer is `RESOURCE_MOVE`, has `DROP_ON_FAILURE`, and carries the
  resolved schedule member as provider plus the schedule contract hash;
- the public identity, TypeRef, TypeSpec, contract, and layout all share the
  same resolved Scheduler provider facts.

Canonical import re-checks the public identity and sections. Consumers must
reject ABI, policy, requirement, transport, scheduler contract, schema, and
provider mismatches before scheduling work.
