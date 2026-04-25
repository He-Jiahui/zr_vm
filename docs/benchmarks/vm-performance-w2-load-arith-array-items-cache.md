---
title: W2 Load Arithmetic Probe And Array Items Cache
module: benchmarks
status: active
updated: 2026-04-24
related_code:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_optimize.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
---

# W2 Load Arithmetic Probe And Array Items Cache

This slice adds two internal performance mechanisms without changing ZR surface semantics.

## Load Plus Typed Arithmetic Probe

The compiler exposes `ZrParser_Quickening_CollectLoadTypedArithmeticProbeStats()` for post-quickening static inspection of residual `GET_STACK` / `GET_CONSTANT` feeding signed arithmetic.

The runtime profile JSON also writes a `quickening_probes` section when instruction profiling is enabled. It currently counts dynamically adjacent executed patterns:

- `get_stack_typed_arithmetic`
- `get_constant_typed_arithmetic`

This is intentionally a probe, not a new user-visible behavior. It gives the next opcode-fusion pass a real gate: if the dynamic candidate count is below the target share of total executed opcodes, the fusion family should not be emitted for that workload.

## Array<int> Items Cache

The quickening pass now recognizes simple backward-`JUMP` loops whose typed `Array<int>` receiver slots are stable inside the loop and whose bodies do not contain unknown calls, dynamic member/index protocol writes, or throws.

For those loops it inserts `SUPER_ARRAY_BIND_ITEMS` before first loop entry and rewrites typed indexed accesses to cached-items variants:

- `SUPER_ARRAY_GET_INT_ITEMS`
- `SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST`
- `SUPER_ARRAY_SET_INT_ITEMS`

Backedges are remapped to the original loop header after the inserted bind sequence, so cached items are resolved once per loop entry rather than on every iteration. Non-backedge branches that enter the loop header still execute the bind sequence.

The cached opcodes keep existing boxed `SZrTypeValue` array item semantics. They only skip repeated receiver-to-hidden-items resolution and still fall back to the existing `SUPER_ARRAY_GET/SET_INT` path when the compiler cannot prove a safe loop-local receiver.

## Raw Storage Design Boundary

Raw contiguous `Array<int>` storage is deliberately left as the next stage. The intended shape remains:

- attach internal dense signed-int storage to the current hidden items object
- store `kind`, `length`, `capacity`, and native `int64_t *data`
- let typed add/get/set use raw data while dense-int invariants hold
- materialize back to existing boxed node-map pairs on non-int writes, sparse writes, or protocol paths that require boxed pair semantics
- let GC own the storage object while native raw data is allocated and freed outside mark scanning

The loop-local items cache should be measured first because it isolates receiver/cache lookup cost from the larger storage-layout change.
