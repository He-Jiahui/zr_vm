---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_call_transfer.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_call_transfer.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_layout.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_call_transfer.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_call_transfer.c
plan_sources:
  - docs/plans/ssa/04-frame-native/02-call-return-tail.md
tests:
  - tests/core/test_ssa_call_return_tail.c
  - tests/acceptance/ssa-call-transfer-packed-layout.md
doc_type: module-detail
---

# Packed call-transfer representation plans

`ZrParser_ExecIr_PrepareCallTransfer` prepares checked argument representation
metadata. It has no production caller. It does not read or write frame memory,
retain or release owners, install a callee frame, or execute a tail call. The
04.02 call/return/tail implementation remains open.

## Endpoint identity and producer evidence

`sourceSlot` and `targetSlot` index their respective physical `frame.slots`
arrays. They are not logical ordinals, logical value IDs, or descriptor
`slotId`s. `valueId` must identify the source endpoint's unique logical
occupant; the callee's corresponding logical ID can differ. Logical maps and
slot classes come from `ZrParser_ExecIr_LayoutPackedFrame`.

The producer can reuse one physical slot for several nonoverlapping logical
values. It keeps the first occupant's type/size/alignment in the physical
descriptor, and does not retain those fields for subsequent logical occupants.
Therefore a selected endpoint must have exactly one logical occupant. A
selected reused slot is rejected even if the request names only one occupant.
Reuse elsewhere in either layout is allowed. Supporting selected reuse needs
additional producer evidence and call-site availability; this API does not
guess an active occupant.

## Checked representation and output

Layouts must provide actual descriptor and logical-map arrays consistent with
their counts. Frame and slot alignment must be powers of two. Every physical
span must be nonempty, aligned, and contained in the frame using checked
subtraction rather than an overflowing offset-plus-size comparison. Logical
IDs must be valid and unique, mappings in range, and classes consistent with
the physical value/reference kind.

For each selected pair, the request class must agree with both logical
classes; source and target type tokens must be nonzero and equal, and sizes
and alignment must agree. Zero tokens in unselected slots remain allowed:
the producer itself does not require nonzero tokens. This is selected
representation proof, not a metadata-manager lookup. The
request's `byteSize` must equal both descriptor sizes. This slice provides
whole-representation transfer records, not truncation or conversion. Flags
must be zero. Repeated target slots and repeated Unique source slots are
rejected. Ownership and class enums are validated by their defined members,
including rejection of negative casts.

The plan owns one checked span per row: source/target byte offsets, byte size,
source/target alignment and type token. It also snapshots both layout hashes.
Existing kind and physical-index arrays remain available. Later consumers
must ensure their layouts still match and provide memory, alias, commit and
lifecycle proof before acting on these records; hashes are not authentication
or a substitute for those contracts.

`SCALAR_COPY`, `SPAN_COPY`, `MOVE`, `BORROW` and `BOXED_BRIDGE` remain descriptive
classification metadata. None authorizes `memcpy`, ownership consumption,
retention, borrow escape or GC-root installation. In particular, `UNKNOWN`
ownership is not evidence of trivial data. Boxed/reference and owned values
require their actual runtime lifecycle consumer.

## Compatibility, failure and empty input

`compatibleLayout` describes the selected argument representations only. Equal
parameter-prefix counts do not prove representation compatibility, and an
empty argument plan does not prove a return ABI. `forwardReturn` is always
false: the request lacks actual return descriptors and return commit proof.
`noAliasConflict` records the request's declaration, not computed alias proof.

Zero rows permit a null values array, validate both layouts, and produce no
owned row arrays. Nonempty count-only forged layouts are rejected. Errors use
`INVALID_RANGE` for structural request/layout errors and `INVALID_VALUE` for
row errors, with `actualVersion` naming the failing row. Allocation overflow
and exhaustion use the existing capacity/OOM diagnostics.

Preparation builds a separate candidate and replaces the initialized old plan
only after every row succeeds. Any failure preserves all old pointers, row
contents, counts, flags, spans and hash snapshots. Call `CallTransferPlanFree`
to release all owned arrays; neither request nor layout memory is owned by the
plan.

调用方须先对新对象或已 Free 的对象调用 PlanInit，并串行独占访问计划。Init 只清零，不能替代释放已有数组；PlanFree 释放 `kinds/sourceSlots/targetSlots/spans` 四组元数据再清空对象。浅复制只可作为原计划生命周期内的只读视图，不能产生另一份可独立 Free 的 owner。该生命周期不涵盖输入布局或实际值，既有 selected-occupant 与 copied-hash 限制继续适用。

## Reference principles and remaining consumers

Local references constrain this design: Lua `lua/src/ldo.c` result placement
keeps cleanup and close-hook ordering explicit; `lua/testes/calls.lua` checks
tail calls and argument windows. Rust `rustc_abi/src/callconv.rs` classifies from
actual layouts, while `tests/ui/abi/large-byval-align.rs` and
`tests/ui/explicit-tail-calls/ctfe-arg-bad-borrow.rs` expose alignment and lifetime
boundaries. QuickJS `quickjs.c` job argument retention and release demonstrate
that byte representation does not encode ownership protocol. These are
principles, not adoption of a host ABI.

The existing ownership call-result contract also distinguishes a reused slot's
inventory from the value reaching a particular instruction. See
[ownership across AOT call results](ownership-aot-call-result.md).

Runtime transfers, overlap staging, return forwarding, cleanup, tail reuse,
ownership execution and performance acceptance require separate integration
and validation. This representation slice makes no claim for those gates.
