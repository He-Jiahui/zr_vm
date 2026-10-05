#ifndef ZR_VM_CORE_EXECUTION_CALL_TRANSFER_H
#define ZR_VM_CORE_EXECUTION_CALL_TRANSFER_H

#include "zr_vm_core/conf.h"

/** @brief 帧参数 staging 及调用转移诊断使用的状态词汇。
 * @note 当前 staging 只产生其中部分状态；枚举和资格判定不构成已执行的帧转移状态机。
 * 数值顺序属于公开 C 接口，新增注释不得改变其布局或编号。
 */
typedef enum EZrExecutionTransferStatus {
    /** 正常处理结果，亦包括没有选中待搬运参数。 */
    ZR_EXECUTION_TRANSFER_OK = 0,
    /** 参数、范围、对齐或不允许的诊断别名不成立。 */
    ZR_EXECUTION_TRANSFER_INVALID_ARGUMENT,
    /** 不同目标写入重叠且所需字节不一致。 */
    ZR_EXECUTION_TRANSFER_ALIAS_CONFLICT,
    /** 为布局兼容性拒绝保留的诊断名称；当前 staging 未产生。 */
    ZR_EXECUTION_TRANSFER_INCOMPATIBLE_LAYOUT,
    /** 为未完成清理保留的诊断名称；当前 staging 未产生。 */
    ZR_EXECUTION_TRANSFER_PENDING_CLEANUP,
    /** 为逃逸帧别名保留的诊断名称；当前 staging 未产生。 */
    ZR_EXECUTION_TRANSFER_ESCAPING_ALIAS,
    /** 为调试帧策略保留的诊断名称；当前 staging 未产生。 */
    ZR_EXECUTION_TRANSFER_DEBUG_FRAME_REQUIRED,
    /** 当前 staging 无法证明表示/所有权可按该路径复制，供上层选择旧路径。 */
    ZR_EXECUTION_TRANSFER_UNSUPPORTED,
    /** 普通 CRT staging 工作区分配失败，不表示谓词本身会分配。 */
    ZR_EXECUTION_TRANSFER_NO_MEMORY,
    /** 调用方提供的 staging 工作区缺失或容量不足。 */
    ZR_EXECUTION_TRANSFER_SCRATCH_TOO_SMALL
} EZrExecutionTransferStatus;

/** @brief 可选调用方诊断输出；无资源所有权，字段只按返回状态解释。 */
typedef struct SZrExecutionArgumentStagingDiagnostic {
    /** 实际返回状态；诊断存储与运行时/栈重叠时可能不写。 */
    EZrExecutionTransferStatus status;
    /** 所报告目标形参序号；无具体问题位置时实现置0。 */
    TZrUInt32 parameterIndex;
    /** 所报告源逻辑栈槽；无具体问题位置时实现置0。 */
    TZrUInt32 sourceStackSlot;
    /** 别名冲突的先前形参；一般结果置UINT32_MAX。 */
    TZrUInt32 relatedParameterIndex;
} SZrExecutionArgumentStagingDiagnostic;

/** @brief 调用方提供的返回直传资格摘要；不持有值，不在此计算证明或执行搬运。 */
typedef struct SZrExecutionReturnTransfer {
    /** 调用方已确认没有返回写入别名冲突。 */
    TZrBool noAliasConflict;
    /** 调用方已确认返回表示/布局兼容。 */
    TZrBool compatibleLayout;
    /** 调用方已确认直传不改变提交顺序。 */
    TZrBool commitOrderPreserved;
    /** 仍需写回时禁止直传；不是完成写回的记录。 */
    TZrBool requiresWriteback;
} SZrExecutionReturnTransfer;

/** @brief 调用方提供的尾帧复用资格摘要；不扫描清理链/别名/调试状态，也不执行复用。 */
typedef struct SZrExecutionTailEligibility {
    /** 调用方已确认没有尚需执行的清理。 */
    TZrBool noPendingCleanup;
    /** 调用方已确认没有逃逸的当前帧别名。 */
    TZrBool noEscapingFrameAlias;
    /** 调用方已确认续行与返回约定允许复用。 */
    TZrBool compatibleContinuation;
    /** 调用方已确认调试策略允许丢弃/复用帧。 */
    TZrBool debugPolicyAllows;
} SZrExecutionTailEligibility;

/** @brief 判断调用方给定的返回摘要是否允许不经写回直接转交。
 * @pre 四项摘要须由调用方按真实值/布局/提交顺序提供；本函数不检查帧。
 * @return 指针为空或资格条件不全满足为假；不分配、不取得摘要所有权。
 * @note 当前仓内直接调用仅见资格单元测试，不代表解释器尾调用路径已采用此接口。
 */
ZR_CORE_API TZrBool ZrCore_Execution_CanForwardReturn(
        const SZrExecutionReturnTransfer *transfer);
/** @brief 判断调用方给定的尾调用摘要是否允许复用当前帧。
 * @pre 四项摘要须由调用方按清理/别名/续行/调试策略提供；本函数不执行复用。
 * @return 指针为空或资格条件不全满足为假；不分配、不取得摘要所有权。
 * @note 当前仓内直接调用仅见资格单元测试，不代表解释器尾调用路径已采用此接口。
 */
ZR_CORE_API TZrBool ZrCore_Execution_CanReuseTailFrame(
        const SZrExecutionTailEligibility *eligibility);
/** @brief 把转移状态映射为可借用的静态诊断名称；未列出的值返回 "unknown"。
 * @return 静态字符串，调用方不得释放或修改；此接口不分配、不改变执行状态。
 * @note 仓内当前未发现调用或函数指针注册；公开ABI的仓外消费者未确定。
 */
ZR_CORE_API const TZrChar *ZrCore_Execution_TransferStatusName(
        EZrExecutionTransferStatus status);

#endif
