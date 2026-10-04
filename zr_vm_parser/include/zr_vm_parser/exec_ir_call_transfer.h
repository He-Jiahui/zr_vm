#ifndef ZR_VM_PARSER_EXEC_IR_CALL_TRANSFER_H
#define ZR_VM_PARSER_EXEC_IR_CALL_TRANSFER_H

#include "zr_vm_parser/exec_ir_frame_layout.h"

/** 由所有权要求和表示类别共同决定的计划标签；标签本身不授权执行或消费值。 */
typedef enum EZrExecIrTransferKind {
    ZR_EXEC_IR_TRANSFER_SCALAR_COPY = 0,
    ZR_EXEC_IR_TRANSFER_SPAN_COPY,
    ZR_EXEC_IR_TRANSFER_MOVE,
    ZR_EXEC_IR_TRANSFER_BORROW,
    ZR_EXEC_IR_TRANSFER_BOXED_BRIDGE,
    ZR_EXEC_IR_TRANSFER_KIND_COUNT
} EZrExecIrTransferKind;

/**
 * 为一个源逻辑值选择两侧的物理槽描述符。sourceSlot/targetSlot 不是逻辑序号或 valueId。
 * valueId 必须对应源槽的唯一逻辑 occupant；目标可使用不同逻辑 ID。
 * byteSize 必须与两侧槽精确相同，flags 在当前表示计划契约中必须为零。
 */
typedef struct SZrExecIrCallTransferValue {
    TZrExecIrValueId valueId;
    EZrExecIrOwnership ownership;
    EZrExecIrPackedSlotClass slotClass;
    /* Physical descriptor indices, not logical ordinals or value IDs.
     * valueId identifies the source's sole logical occupant. */
    TZrUInt32 sourceSlot;
    TZrUInt32 targetSlot;
    TZrUInt32 byteSize;
    TZrUInt32 flags; /* Must be zero in the representation-plan contract. */
} SZrExecIrCallTransferValue;

/**
 * 一次同步规划所借用的布局和值列表；调用期间须保持有效、稳定且数组足以覆盖计数。
 * valueCount 为零时 values 可为 NULL，但两侧布局仍会完整核验。
 * returnMayAlias 只形成别名提示，requiresWriteback 当前不构成返回提交/转发证明。
 */
typedef struct SZrExecIrCallTransferRequest {
    const SZrExecIrPackedFrameLayout *sourceLayout;
    const SZrExecIrPackedFrameLayout *targetLayout;
    const SZrExecIrCallTransferValue *values;
    TZrUInt32 valueCount;
    TZrBool returnMayAlias;
    TZrBool requiresWriteback;
} SZrExecIrCallTransferRequest;

/* 经核验的表示元数据快照；不授权复制、所有权消费、借用逃逸或 GC 根变更。 */
typedef struct SZrExecIrCallTransferSpan {
    TZrUInt32 sourceByteOffset;
    TZrUInt32 targetByteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 sourceByteAlign;
    TZrUInt32 targetByteAlign;
    TZrExecIrTypeToken typeToken;
} SZrExecIrCallTransferSpan;

/**
 * 独占四组长度为 valueCount 的元数据数组；用 PlanFree 结束或清空其生命周期。
 * 浅复制可作只读快照，不能使两份对象分别释放同一数组。
 * layoutHash 只是输入快照；compatibleLayout 仅说明所选参数表示匹配。
 * noAliasConflict 来自调用方的 returnMayAlias 提示，forwardReturn 当前恒为 false。
 */
typedef struct SZrExecIrCallTransferPlan {
    EZrExecIrTransferKind *kinds;
    TZrUInt32 *sourceSlots;
    TZrUInt32 *targetSlots;
    SZrExecIrCallTransferSpan *spans;
    TZrUInt64 sourceLayoutHash;
    TZrUInt64 targetLayoutHash;
    TZrUInt32 valueCount;
    TZrBool forwardReturn; /* False: no return descriptors/commit proof. */
    TZrBool noAliasConflict;
    TZrBool compatibleLayout; /* Selected argument representations only. */
} SZrExecIrCallTransferPlan;

/**
 * @brief 为新对象或已经 PlanFree 的对象建立空计划。
 * @param plan 调用方独占的计划；NULL 可用。已有数组须先 Free，Init 不代替释放。
 */
ZR_PARSER_API void ZrParser_ExecIr_CallTransferPlanInit(SZrExecIrCallTransferPlan *plan);
/**
 * @brief 释放计划拥有的四组元数据并清空对象；输入布局和实际帧不在释放范围。
 * @param plan 已初始化的独占计划；NULL 或空计划可用。清空后可再次 Prepare 或 Free。
 */
ZR_PARSER_API void ZrParser_ExecIr_CallTransferPlanFree(SZrExecIrCallTransferPlan *plan);
/**
 * @brief 为所选参数构建经核验的表示元数据，成功替换旧计划，失败保留旧计划及数组内容。
 * @param request 借用且调用期间稳定的布局和值列表；其数组长度须覆盖各自计数。
 * @param plan 已初始化并由调用方串行访问的计划；成功所得数组最终由 PlanFree 释放。
 * @param diagnostic 可选诊断。行级失败的 actualVersion 是从零开始的请求行索引。
 * @return 布局与所选行核验、元数据分配和计划发布均成功时返回 true；不表示实际转移已执行。
 * 所选槽须各有唯一逻辑 occupant，即使复用生命周期互不重叠也不能选入。
 * kinds/spans 不授权内存复制、所有权消费、借用逃逸或 GC 根变更，也不证明返回转发。
 */
ZR_PARSER_API TZrBool ZrParser_ExecIr_PrepareCallTransfer(
        const SZrExecIrCallTransferRequest *request,
        SZrExecIrCallTransferPlan *plan,
        SZrExecIrDiagnostic *diagnostic);

#endif
