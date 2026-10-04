#ifndef ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_INTERNAL_H
#define ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_INTERNAL_H

#include "backend_aot_ir_scalar_text.h"

/**
 * @brief 两种文本发射器共用的已校验标量计划；只复制 ID 和位模式，不持有输入指针。
 * @note 仅 prepare 返回 OK 后可读取。branchTargetBlockId 为零表示单块，
 *       非零为已校验的目标块 ID；bits 按 i64 的二进制补码生成目标文本。
 */
typedef struct SZrBackendAotIrScalarTextPlan {
    TZrUInt32 functionId;
    TZrUInt64 bits;
    TZrUInt32 branchTargetBlockId; /* 零表示单块形式。 */
} SZrBackendAotIrScalarTextPlan;

/**
 * @brief 先执行共享 AOTIR/ABI 校验，再筛出文本发射器能完整保留的常量返回形状。
 * @pre plan 非空；module 数组为调用期间有效的借用视图。
 * @return OK 时发布完整计划；失败返回共享校验错误或 UNSUPPORTED。
 * @note 入口先清零 plan，但失败中途可能已写入分支目标，调用方不能消费失败计划。
 */
EZrAotIrStatus backend_aot_ir_scalar_text_prepare(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        SZrBackendAotIrScalarTextPlan *plan,
        SZrAotIrDiagnostic *diagnostic);

/**
 * @brief 为本层拒绝路径返回状态，并覆盖可选诊断，避免保留上次失败的位置。
 * @note 本层只填写函数、指令及 expected/actual；块和索引字段归零。
 */
EZrAotIrStatus backend_aot_ir_scalar_text_fail(
        SZrAotIrDiagnostic *diagnostic, EZrAotIrStatus status,
        TZrUInt32 functionId, TZrUInt32 instructionId,
        TZrUInt64 expected, TZrUInt64 actual);

/**
 * @brief 在读取 IR 前建立空文本/零长度的失败基线，并检查必需的输出参数。
 * @return 输出指针、正容量及长度指针齐全时返回 OK，否则返回 INVALID_ARGUMENT。
 * @note 只在对应存储可写时清零；成功时不更新 diagnostic，后续共享校验负责诊断。
 */
EZrAotIrStatus backend_aot_ir_scalar_text_check_output(
        char *output, size_t capacity, size_t *outLength,
        SZrAotIrDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_BACKEND_AOT_IR_SCALAR_TEXT_INTERNAL_H */
