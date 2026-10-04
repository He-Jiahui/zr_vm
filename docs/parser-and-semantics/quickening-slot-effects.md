---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_instruction.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_contiguous_view.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
plan_sources:
  - user: 2026-10-04 全仓库公共注释与调用契约审查，仅补文档与静态证据
  - docs/plans/ssa/05-data-layout/02-arrays-slices.md
  - docs/plans/ssa/03-interpreter-binding/04-generated-fusion.md
tests:
  - tests/core/test_execution_dispatch_callable_metadata.c
  - tests/parser/test_span_core.c
  - tests/parser/test_compiler_w2_performance_quickening.c
  - tests/parser/test_compiler_w2_quickening_array_add.inc
  - tests/acceptance/ssa-quickening-member-slot-effects.md
  - tests/acceptance/ssa-quickening-array-int-add.md
doc_type: module-detail
---

# Quickening Slot Effects

## Purpose

Compiler quickening uses frame-slot read and write classifiers to decide when a
temporary copy can be forwarded, when a destination is dead, and whether an
optimization may cross an instruction. These classifiers describe the
instruction's frame effects; encoded member and cache identifiers are metadata,
even when their numeric values happen to match a frame slot.

## Related Files

- `zr_vm_common/include/zr_vm_common/zr_instruction_conf.h` defines the
  instruction set and operand layouts.
- `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c` contains the
  frame-slot effect classifiers and the copy-forwarding pass that consumes
  them.
- `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_contiguous_view.c`
  emits inline field reads when lowering contiguous-view expressions.
- `tests/parser/test_span_core.c` exercises the Span expression that exposed
  the classifier error.
- `tests/parser/test_compiler_w2_performance_quickening.c` covers neighboring
  member-slot and quickening behavior.

## Behavior Model

For `GET_MEMBER` and `GET_MEMBER_SLOT`, `operand1[0]` is the receiver frame
slot. `operand1[1]` identifies the member or member cache entry and is not a
frame-slot read. Both instructions place their result in
`operandExtra`, which is their only frame-slot write.

`compiler_quickening_instruction_may_read_slot` and
`compiler_quickening_instruction_writes_slot` are shared by liveness checks and
copy-forwarding transformations. The `GET_MEMBER` cases therefore stay
separate from instruction groups whose second operand is another frame slot.
They also stay separate from generic write checks that inspect bytes from the
overlapping operand union: those bytes can encode the receiver or member/cache
metadata and do not imply an additional frame write.

## Failure and Correction

Inline contiguous-view member lowering reads `source`, `start`, and `length`
from a view receiver. When the view is staged from a local into a temporary,
quickening can forward each receiver read back to the local and remove the
temporary copy only after all temporary uses have been handled. The previous
generic classifiers could treat a member/cache index as a read or write of the
same-numbered frame slot. A coincidental index/slot collision made the
forwarding scan stop before the final field read, even though the copy was
removed. The final `length` read then observed the empty temporary and the
following bounds check rejected a valid `view[0]` access.

The fix changes only slot-effect classification. It preserves instruction
encoding, member cache behavior, source maps, runtime view representation, and
all `SET_MEMBER*` effect rules. The regression has no Array growth, so it
isolates quickening's instruction metadata from storage relocation.

## Test Coverage

`test_span_length_arithmetic_preserves_following_index_receiver` creates a
four-element Array-backed Span and evaluates `view.length * 10000000 +
view[0] * 100000`. It expects `41000000`. Before the fix, the source fixture
failed with `Contiguous view index out of range`; afterward it passes as part of
the complete Span core suite.

The adjacent quickening performance suite also checks member-slot forwarding
and direct result stores. Against the original classifiers, that suite reported
18/20: the member-slot direct-result-store test and typed
`Array<int>.add` specialization case failed. With the classifier correction,
19/20 passed; the remaining `Array<int>.add` case was an independent matcher
gap. A follow-up 03.04 slice now recognizes the compiler's direct fused
`KNOWN_NATIVE_MEMBER_CALL` form, checks the typed receiver/value and provider
contract, then lowers it to `SUPER_ARRAY_ADD_INT`. The final quickening suite
passes 20/20. The matcher guard, cache retirement, and separate acceptance
gates are documented in
[`quickening-array-int-add.md`](quickening-array-int-add.md) and
[`ssa-quickening-array-int-add.md`](../../tests/acceptance/ssa-quickening-array-int-add.md).

## Plan Sources

The regression is bounded by the contiguous view and index-lowering contract in
[SSA 05.02](../plans/ssa/05-data-layout/02-arrays-slices.md). Its repair keeps
the existing ExecBC quickening projection and instruction encodings described
by [SSA 03.04](../plans/ssa/03-interpreter-binding/04-generated-fusion.md);
it does not implement or complete either plan.

## Acceptance Record

See [`ssa-quickening-member-slot-effects.md`](../../tests/acceptance/ssa-quickening-member-slot-effects.md)
for the classifier RED/GREEN evidence. The Array add follow-up has its own
acceptance record because it changes the typed native-call matcher and binding
lifecycle.

## 公共指令编码与使用前置条件

`ZR_INSTRUCTION_DECLARE` 是 opcode 行序的共同来源：公共头展开枚举，解释器展开当前函数内的标签表，profile 展开名称。编号保存于旧二进制指令中，重排 schema 会改变既有字节的解释；这与 quickening 的槽读写分类职责分开。枚举封装为 `zr_vm_common/include/zr_vm_common/zr_instruction_conf.h:301`，标签表实际消费为 `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:2374`。

`SZrInstruction` 的 opcode、extra 与四字节 operand 区共同组成指令实体（`zr_vm_common/include/zr_vm_common/zr_instruction_conf.h:403`）。operand 的 8/16/32 位数组重叠同一存储，分类器和构造器须按 opcode 选择视图；extra 的结果、接收者或暂存标记角色也由 opcode 决定，不能仅由数值推断另一个槽读写。

有效推进后的 PC 是调用方前置条件。`FETCH` 与 `FETCH_PREPARE_SHARED` 自身不检查 PC 范围：快速分支直接读取推进后的指令（`zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:2621`），常规分支调用 `ZR_INSTRUCTION_FETCH`（`zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:2631`）处理 trap 后推进。mutator poll 的 safepoint/预算协作不构成每次取指的上界验证。公共 opcode 宏同样不验证编号或操作数。

本段依据当前源码作静态契约说明；既有 Test Coverage 和 Acceptance Record 的运行记录保持原历史范围，本次没有重跑 quickening、解释器或 native 测试。
