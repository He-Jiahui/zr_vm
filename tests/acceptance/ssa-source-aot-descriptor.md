---
related_code:
  - tests/parser/test_ssa_source_aot_descriptor.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_module_contract.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_module_contract.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
implementation_files:
  - tests/parser/test_ssa_source_aot_descriptor.c
plan_sources:
  - .codex/plans/20261006-ssa-source-module-contract.md
  - docs/plans/ssa/07-aot-backends/01-aotir-contract.md
tests:
  - tests/parser/test_ssa_source_aot_descriptor.c
  - docs/acceptance/ssa-source-aot-descriptor.md
doc_type: testing-guide
status: finite-source-aot-descriptor-green
---

# Real Source AOT Descriptor Test Guide

## Actual fixture and modes

Target `zr_vm_ssa_source_aot_descriptor_test` and CTest
`ssa_source_aot_descriptor` link the sole shared literal fixture once, without
source snapshots or `.c` inclusion. The unchanged fixture has three modes:
`--prerequisites-only` runs two real 9/8 cases; `--features-only` runs two
binder feature cases; default runs 20 cases (two prerequisites, two features,
one repeat/replacement and 15 refusal cases containing 16 calls).

The prerequisites build real original metadata/module, compaction, host row,
owned frame, canonical projection and accepted host target, preserving
CONSTANT/NOP/RETURN, IDs/maps and present empty state-map identity. They assert
unbound BuildDescriptor fails specifically with VERSION_MISMATCH.

RED passed both prerequisites and failed both feature binder bool assertions
against the UNSUPPORTED stub. RED did not execute the full default suite.
r2 functional GREEN passed the default suite and existing host14/tokens8/
identity13 regressions, 55 cases total. Actual BuildDescriptor, Core
ValidateModule and RequireExecutableAbi pass using bound original metadata;
projection.runnable remains false. MSVC compiled both changed TUs with exit 0 and zero warnings.

## Preservation and teardown

Refusal cases modify only bounded fields of real independently owned
metadata/module records and restore them immediately after the call, before
any assertion, observation digest or teardown. They compare complete
failure preservation and source observations, repeat binding for idempotence,
and test failed replacement of an already owned successful descriptor.
No arbitrary pointer, out-of-bounds access, fabricated canonical node/module
hash, large allocation or capability execution belongs to this gate.

Global owners are initialized before Unity assertions. Release descriptors
before projections, compacted graphs, original fixtures and runtime. Borrowed
descriptor views do not retain those owners automatically. Temporary existing
hash-provider allocations must be released; no explicit allocator-failure or
GC-stress coverage is claimed by this draft.

## Evidence and limits

The [acceptance record](../../docs/acceptance/ssa-source-aot-descriptor.md)
owns actual source/log/binary/metadata hashes, modes, diagnostic failures,
RED commit, subsequent GREEN and platform limits. Ordinary prerequisite
failures must be retained separately and cannot count as target-feature RED.
Root r2 configure/build/four CTest exited 0; 55 cases passed with zero failures,
zero ignored, zero Clang warnings and no observed UBSan diagnostic. Native
driver 38840 was observed naturally terminal with exit 0. The acceptance record
retains initial signed-comparison warnings and MSVC driver/setup failures.

Descriptor contract validation does not execute source, Oracle, backend or
native code. Real-frame scalar emission, normal returned artifact retention,
Linux/full MSVC matrices, full 07.01 migration and full SSA47 remain OPEN.
No network/FFI/provider/capability/security execution is involved.
