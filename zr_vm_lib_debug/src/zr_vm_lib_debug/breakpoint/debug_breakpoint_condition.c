#include "breakpoint/debug_breakpoint_condition.h"

/* 条件断点沿用调试器自身的真值约定，避免把断点表达式的结果类型限制为 bool。 */
static TZrBool zr_debug_breakpoint_condition_value_truthy(const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            return ZR_FALSE;
        case ZR_VALUE_TYPE_BOOL:
            return value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            return value->value.nativeObject.nativeInt64 != 0 ? ZR_TRUE : ZR_FALSE;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return value->value.nativeObject.nativeDouble != 0.0 ? ZR_TRUE : ZR_FALSE;
        case ZR_VALUE_TYPE_STRING:
            return value->value.object != ZR_NULL ? ZR_TRUE : ZR_FALSE;
        default:
            return value->value.object != ZR_NULL ? ZR_TRUE : ZR_FALSE;
    }
}

/* 断点 hook 在决定是否停住之前读取当前首帧；副作用白名单为空且不回退旧求值器。 */
TZrBool zr_debug_breakpoint_condition_evaluate(ZrDebugAgent *agent,
                                               const TZrChar *condition,
                                               TZrBool *outSatisfied,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize) {
    SZrTypeValue value;

    if (outSatisfied != ZR_NULL) {
        *outSatisfied = ZR_FALSE;
    }
    if (errorBuffer != ZR_NULL && errorBufferSize > 0u) {
        errorBuffer[0] = '\0';
    }
    if (agent == ZR_NULL || outSatisfied == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "invalid breakpoint condition request");
        return ZR_FALSE;
    }
    if (condition == ZR_NULL || condition[0] == '\0') {
        *outSatisfied = ZR_TRUE;
        return ZR_TRUE;
    }

    memset(&value, 0, sizeof(value));
    if (!zr_debug_evaluate_expression_with_capabilities(agent,
                                                        1u,
                                                        condition,
                                                        ZR_DEBUG_EVALUATION_EFFECT_NONE,
                                                        ZR_FALSE,
                                                        &value,
                                                        ZR_NULL,
                                                        ZR_NULL,
                                                        errorBuffer,
                                                        errorBufferSize,
                                                        ZR_NULL,
                                                        0u)) {
        return ZR_FALSE;
    }

    *outSatisfied = zr_debug_breakpoint_condition_value_truthy(&value);
    return ZR_TRUE;
}

/**
 * @fn TZrBool zr_debug_breakpoint_condition_value_truthy(const SZrTypeValue *value)
 * ## Summary
 * 把已成功正式求值的值转换为条件断点最终命中位，保持调试器自己的真值约定，而不把顶层结果强制限定为 bool。
 *
 * ## Parameters
 * @param[in] value 借用值记录；可为 NULL，不转移或延长其所引用 VM 对象的寿命。
 *
 * ## Returns
 * @return NULL 记录及 null 类型为假；bool、共用 64 位整数槽和浮点槽按是否为零判断；string 及其余类型按 object 槽是否非 NULL 判断。
 *
 * ## Constraints and lifecycle
 * 不执行 getter、调用或用户转换，也不分配或释放对象。浮点比较沿用 C 的 != 0.0（NaN 不等于零）；字符串不按长度判断。调用者须先按 type 初始化正确的联合槽，并在本次同步读取期间保持记录及引用对象有效。
 *
 * ## References
 * - zr_debug_breakpoint_condition_evaluate — zr_vm_lib_debug/src/zr_vm_lib_debug/breakpoint/debug_breakpoint_condition.c:72 仅在求值成功后转换结果。
 * - TZrNativeObject — zr_vm_common/include/zr_vm_common/zr_common_conf.h:76 使用共用 64 位标量槽。
 */
/**
 * @fn TZrBool zr_debug_breakpoint_condition_evaluate(ZrDebugAgent *agent, const TZrChar *condition, TZrBool *outSatisfied, TZrChar *errorBuffer, TZrSize errorBufferSize)
 * ## Summary
 * 在 trace 命中路径或现有测试中计算条件并交付满足位；把空条件、普通求值失败和成功但为假的结果明确分开，供上层决定是否停止。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试 agent；必须非 NULL。正式准备过程借用其当前 state 和首帧上下文，可能更新 VM/GC 或临时编译器状态。
 * @param[in] condition 借用 NUL 结尾条件文本；NULL 或空串在有效请求中视为无条件满足。
 * @param[out] outSatisfied 必须非 NULL；进入时先置假，只有空条件或成功转换后的真值才置真。
 * @param[out] errorBuffer 可选诊断缓冲；非 NULL 且容量为正时先置空，普通失败由下层写入可能截断的文本。
 * @param[in] errorBufferSize errorBuffer 的可写字节容量；为零时本层不写该缓冲。
 *
 * ## Returns
 * @return 真表示空条件或表达式已成功求值（outSatisfied 仍可为假）；假表示请求无效或正式求值失败，且有效 outSatisfied 保持假。非局部异常不属于布尔返回契约。
 *
 * ## Constraints and lifecycle
 * 非空条件固定 frameId=1、allowedEffectFlags=NONE、allowLegacyCompatibility=false，不用兼容 parser 绕过 canonical/capability 拒绝。引用表达式依赖调用者保持当前帧事实有效；纯 literal 算式不要求暂停帧元数据。零 capability 不保证零解析分配；普通上下文释放由正式求值器负责，本层不建立 TryRun，也不保证 Throw 后普通清理、状态或输出事务回滚。agent/state 不由此函数释放；本层不提供并发或重入隔离。
 *
 * ## References
 * - zr_debug_breakpoint_condition_satisfied — zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1341 失败时输出诊断并跳过断点。
 * - zr_debug_evaluate_expression_with_capabilities — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:932 正式求值与禁用兼容的门禁。
 * - test_debug_breakpoint_condition_requires_pure_formal_evaluation — tests/debug/test_debug_breakpoint_condition_cases.h:14 比较成功及分配能力拒绝。
 */
