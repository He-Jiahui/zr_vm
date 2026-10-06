---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_host_aot_target.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_host_primitive_layout.h
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - tests/parser/test_ssa_host_noargs_i64_aot_target.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_host_aot_target.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_aot_target.c
plan_sources:
  - .codex/plans/20261005-ssa-host-aot-target.md
  - docs/plans/ssa/07-aot-backends/01-aotir-contract.md
  - docs/plans/ssa/04-frame-native/03-native-abi.md
tests:
  - tests/parser/test_ssa_host_noargs_i64_aot_target.c
  - tests/parser/test_ssa_host_primitive_layout.c
  - tests/parser/test_ssa_primitive_source_frame.c
  - tests/acceptance/ssa-host-aot-target.md
doc_type: module-detail
status: finite-host-aot-target-green
---

# Host No-Argument I64 AOT Target

## Purpose and current state

`ZrParser_ExecIr_MakeHostNoArgsI64AotTarget` is a finite target-record producer
for the current Win64 MSVC x64 host and an actual canonical no-argument INT64
callable. Its result is the existing Core `SZrAotIrTargetContract`, containing
ABI version, pointer size, byte order, required capabilities, triple hash and
ABI hash. This record has no maximum-alignment field.

The producer passed its finite Win64 gate after an actual linkable UNSUPPORTED
stub established genuine RED. The frozen [implementation plan](../../.codex/plans/20261005-ssa-host-aot-target.md)
and [acceptance record](../acceptance/ssa-host-aot-target.md) preserve that order,
the pre-compiler driver failure, initial MSVC warnings and final r2 evidence.
Three suites passed 48 Unity cases; MSVC compiled the new TU without warnings.

## Canonical input and layout witness

The caller supplies a live, independently owned semantic interner snapshot,
a callable ID from that same snapshot, an actual return layout row and
independent writable output/optional diagnostic storage. Canonical node IDs
must remain sorted. Reachable source storage must be valid, readable, aligned
and nonoverlapping with output/diagnostic; the caller cannot reset, grow, free
or concurrently rewrite the snapshot during the call. Shape, length/capacity,
product and address-span checks constrain metadata; they do not prove arbitrary
pointer readability.

The producer admits a real FUNCTION node with nonzero structural hash,
zero parameter-contract length, receiver NONE and effects NONE. Its return ID
must identify an actual canonical PRIMITIVE INT64 node. A real zero-parameter
interner can retain a nonnull parameter buffer or spare capacity.

The supplied row must have nonzero row ID and the actual return type token.
The producer recomputes a witness with the existing
`ZrParser_ExecIr_MakeHostPrimitiveLayout`, using the same return ID and row ID,
then compares all five fields: ID, type token, byte size, alignment and layout
hash. A caller's nonzero hash alone is insufficient.

The candidate is initialized locally and published once, only after all checks
and `ZrCore_AotIr_ValidateTarget`. Failure preserves every output byte and all
source/context/row bytes. Diagnostics may be cleared at entry. No allocation,
owner transfer, retained input pointer or cancellation work is introduced.
The scalar result does not prolong the life of an AOT projection.

## Actual host restriction

The host whitelist requires `_WIN32`, `_WIN64`, `_MSC_VER`, and
`_M_X64` or `_M_AMD64`, excluding ARM64 and ARM64EC. Actual CHAR_BIT must be 8,
pointer size 8, INT64 size/alignment both 8, and byte order little endian.
The fixed triple is `x86_64-pc-windows-msvc`; no caller triple is accepted.
The result uses the existing target ABI version, pointer size 8, endianness 0
and required capabilities 0. A zero capability field grants no runtime power.

Local Rust Win64 target/callconv and Mono amd64 I8/U8 return-register rules
motivate the finite direct signed-I64 return in RAX. These references establish
ABI design facts, not execution acceptance for this project or another host.
Unsupported-host source branches have no dynamic acceptance until actually run.

## Frozen Stable64 schema 1

Both hashes consume one continuous sequence of explicit ASCII and little-endian
fixed-width bytes, without NUL terminators, struct padding or addresses.

| Hash | Ordered encoding |
| --- | --- |
| `targetTripleHash` | ASCII `zr.aotir.target-triple`; u32 schema 1; u32 triple ASCII byte count; ASCII `x86_64-pc-windows-msvc` |
| `abiHash` prefix | ASCII `zr.aotir.host.noargs-i64.abi`; u32 schema 1; u32 existing target ABI version; u64 triple hash |
| `abiHash` facts, all u32 | pointer size 8; CHAR_BIT 8; little-endian tag 0; existing NOARGS_I64 callable-kind constant; explicit parameter count 0; implicit parameter count 0; signed return tag 1; return bit width 64; return byte size 8; return alignment 8; Win64 C direct-I64-in-RAX rule tag 1 |

The fixed triple has 22 ASCII bytes. Its hash payload is 52 bytes: 22-byte
domain, two u32 fields and 22 triple bytes. The ABI payload is 88 bytes:
28-byte domain, two u32 fields, one u64 triple hash and eleven u32 facts. Definition-bound
domains/schema/widths/tags use named local encoding constants; existing Core
ABI and callable-kind constants are reused. A zero hash is failure and is never
rewritten to 1. Context-local type IDs, row IDs, structural hashes, literal bits,
frame hash, salt and time are excluded. This is a restricted AOTIR callable ABI
fingerprint, independent of Common's full platform ABI. The producer does not
call Common DetectHostAbi/ComputeAbiHash or modify Common platform state.

## Real prerequisites and remaining dependencies

The [test guide](../../tests/acceptance/ssa-host-aot-target.md) preserves the
real 9/8 source pipeline, actual canonical IDs, compaction, appended host row,
owned frame and canonical AOT projection. Real NOP, instruction IDs, source
maps, tables and empty state-map identity remain intact. Target equality across
different literal bits is independent of graph/context identity.

Module descriptor contract version/binding is a separate known gap. This target
does not copy a function contract or synthesize module IDs. The scalar emitter
still refuses real frame/layout/state metadata and expects two instructions;
removing the true NOP, clipping metadata or rebuilding the graph is not evidence
for that dependency. Borrowed descriptor views require projection owner life and
a separate retention contract.

Descriptor binding, real-frame scalar emission, source/native execution,
retention, Linux GCC/Clang, full MSVC matrix, normal returned artifact retention and full SSA47 remain OPEN.
No network/import loader, FFI, provider, capability, hotpatch or security
execution follows from target-record production.
