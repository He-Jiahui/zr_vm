#include "debug_internal.h"

static const TZrChar *zr_debug_reference_scope_label(EZrDebugScopeKind scopeKind) {
    switch (scopeKind) {
        case ZR_DEBUG_SCOPE_KIND_ARGUMENTS:
            return "argument";
        case ZR_DEBUG_SCOPE_KIND_LOCALS:
            return "local";
        case ZR_DEBUG_SCOPE_KIND_CLOSURES:
            return "closure";
        case ZR_DEBUG_SCOPE_KIND_GLOBALS:
            return "global";
        case ZR_DEBUG_SCOPE_KIND_PROTOTYPE:
            return "prototype";
        case ZR_DEBUG_SCOPE_KIND_STATICS:
            return "static";
        case ZR_DEBUG_SCOPE_KIND_EXCEPTION:
            return "exception";
        default:
            return "";
    }
}

/** @brief 用 scope 类别生成求值器与变量快照共用的简短来源描述。 */
void zr_debug_reference_summary_from_scope(EZrDebugScopeKind scopeKind,
                                           const TZrChar *name,
                                           TZrChar *buffer,
                                           TZrSize bufferSize) {
    const TZrChar *label;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }
    buffer[0] = '\0';
    if (name == ZR_NULL || name[0] == '\0') {
        return;
    }

    label = zr_debug_reference_scope_label(scopeKind);
    if (label[0] == '\0') {
        return;
    }

    snprintf(buffer, bufferSize, "%s %s", label, name);
    buffer[bufferSize - 1u] = '\0';
}

/** @brief 按兼容求值器的查找顺序定位暂停帧中的标识符来源。 */
TZrBool zr_debug_identifier_reference_summary(ZrDebugAgent *agent,
                                              TZrUInt32 frameId,
                                              const TZrChar *name,
                                              TZrChar *buffer,
                                              TZrSize bufferSize) {
    SZrFunction *function = ZR_NULL;
    SZrCallInfo *callInfo;
    TZrUInt32 pc;
    TZrUInt32 slotIndex;

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || name == ZR_NULL || name[0] == '\0' ||
        buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    /* 与 zr_debug_resolve_identifier_value 保持捕获、局部、内建全局的优先级一致。 */
    callInfo = zr_debug_find_call_info_by_frame_id(agent, frameId == 0 ? 1u : frameId, &function);
    if (callInfo != ZR_NULL && function != ZR_NULL) {
        if (zr_debug_closure_capture_value(agent, function, callInfo, name) != ZR_NULL) {
            zr_debug_reference_summary_from_scope(ZR_DEBUG_SCOPE_KIND_CLOSURES,
                                                  name,
                                                  buffer,
                                                  bufferSize);
            return buffer[0] != '\0' ? ZR_TRUE : ZR_FALSE;
        }

        pc = zr_debug_instruction_offset(callInfo, function);
        for (slotIndex = 0; slotIndex < function->stackSize; slotIndex++) {
            SZrString *localName = ZrCore_Function_GetLocalVariableName(function, slotIndex, pc);
            if (localName != ZR_NULL && strcmp(zr_debug_string_native(localName), name) == 0) {
                zr_debug_reference_summary_from_scope(slotIndex < function->parameterCount
                                                              ? ZR_DEBUG_SCOPE_KIND_ARGUMENTS
                                                              : ZR_DEBUG_SCOPE_KIND_LOCALS,
                                                      name,
                                                      buffer,
                                                      bufferSize);
                return buffer[0] != '\0' ? ZR_TRUE : ZR_FALSE;
            }
        }

    }

    if (strcmp(name, "zr") == 0 ||
        strcmp(name, "loadedModules") == 0 ||
        (strcmp(name, "error") == 0 && agent->state->hasCurrentException)) {
        zr_debug_reference_summary_from_scope(ZR_DEBUG_SCOPE_KIND_GLOBALS, name, buffer, bufferSize);
        return buffer[0] != '\0' ? ZR_TRUE : ZR_FALSE;
    }

    return ZR_FALSE;
}

/**
 * @fn static const TZrChar *zr_debug_reference_scope_label(EZrDebugScopeKind scopeKind)
 *
 * ## Summary
 * 为来源摘要选择固定的 scope 词，保持变量列表和标识符求值使用相同的 argument/local/closure/global/prototype/static/exception 拼写。
 *
 * ## Parameters
 * @param[in] scopeKind 待显示的调试 scope 枚举。
 *
 * ## Returns
 * @return 对应的进程静态 NUL 文本；未知 scope 返回静态空串。
 *
 * ## Constraints and lifecycle
 * 该词只描述传入类别，不检查 scope、帧、名称或值是否实际存在，也不取得任何运行时资源或所有权。
 *
 * ## References
 * - zr_debug_reference_summary_from_scope (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_reference_summary.c:25)。
 */

/**
 * @fn void zr_debug_reference_summary_from_scope(EZrDebugScopeKind scopeKind, const TZrChar *name, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 把已知 scope 与名称格式化为简短来源文字，供变量快照及标识符来源查询复用，而不重新解析名称。
 *
 * ## Parameters
 * @param[in] scopeKind 调用方已知的 scope 类别；未知类别不生成摘要。
 * @param[in] name 调用期间有效的非空 NUL 名称；NULL/空串不生成摘要。
 * @param[out] buffer 可选可写输出；不得与仍需读取的 name 重叠，非空且容量非零时先清空。
 * @param[in] bufferSize 输出字节容量，包含 NUL。
 *
 * ## Constraints and lifecycle
 * buffer 为 NULL 或容量为 0 时不写；有效输出遇到空名称/未知类别保持空串。其余以 label、空格和名称格式化并强制末字节 NUL。
 * 无返回状态；小缓冲区可只保留部分前缀或空串，按字节截断且不承诺完整名称/UTF-8。此处没有字符串分配、名称查找、GC root/pin 或锁。
 *
 * ## References
 * - zr_debug_reference_scope_label (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_reference_summary.c:3)。
 * - ZrDebug_ReadVariables (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2988)。
 * - zr_debug_identifier_reference_summary (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_reference_summary.c:49)。
 */

/**
 * @fn TZrBool zr_debug_identifier_reference_summary(ZrDebugAgent *agent, TZrUInt32 frameId, const TZrChar *name, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 在当前调试停点中为单个标识符选择来源类别，供兼容求值器和已解析读取事实附加人类可读摘要，不计算标识符的值。
 *
 * ## Parameters
 * @param[in] agent 借用调试代理及其 state；本函数不检查 runMode，调用者须保证帧/元数据在读取期间有效且不并发恢复执行。
 * @param[in] frameId 一基帧号；0 在本入口映射为 1。
 * @param[in] name 调用期间有效的非空 NUL 标识符；不与输出区重叠。
 * @param[out] buffer 可选可写摘要缓冲区，非空且容量非零时在参数校验前先清空。
 * @param[in] bufferSize 输出字节容量，包含 NUL。
 *
 * ## Returns
 * @return 已选中类别且最终 buffer 非空时为 TRUE；无有效输入、没有受支持来源或结果为空时为 FALSE。TRUE 不保证名称完整未截断，也不证明标识符求值成功。
 *
 * ## Constraints and lifecycle
 * lookup_order：在可取得的帧/函数中先查 closure 捕获；否则按 slotIndex 从 0 到 stackSize-1，使用当前 PC 的活动局部名匹配，小于 parameterCount 的槽标为 argument，其余标为 local；最后仅接受 zr、loadedModules，以及当前确有异常时的 error。帧缺席仍可进入这三个内建名的回退。
 * 此处的类别优先级与兼容解析约定相同，但没有复制解析器全部局部枚举/值物化逻辑；不把来源文字作为 formal/canonical 名称绑定或求值正确性的证明。
 * 捕获槽、函数局部名和字符串均为停点内借用，函数不复制值、不建立持久 root/pin、不加锁，也不执行用户代码；借用期由上层暂停协议维持。
 * 匹配后立即按 scope 格式化并返回非空判定，不在小缓冲区输出为空时继续试下一来源；普通输入失败不带详细错误原因，截断没有独立状态。
 *
 * ## References
 * - zr_debug_find_call_info_by_frame_id (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:326)。
 * - zr_debug_closure_capture_value (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1912)。
 * - zr_debug_instruction_offset (zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:104)。
 * - ZrCore_Function_GetLocalVariableName (zr_vm_core/src/zr_vm_core/function/function.c:1675)。
 * - zr_debug_resolve_identifier_value (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1991)。
 * - zr_debug_eval_parse_primary (zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:388)。
 * - zr_debug_append_reference_summaries (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:772)。
 * - zr_debug_reference_summary_from_scope (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_reference_summary.c:25)。
 */
