#include "exec_ir_verify_effect_backedges.h"

#include <stdlib.h>
#include <string.h>

static void zr_exec_ir_backedge_diag(SZrExecIrDiagnostic *diagnostic,
                                   EZrExecutionDiagnosticCode code,
                                   const SZrExecIrFunction *function,
                                   TZrUInt32 blockId,
                                   TZrUInt32 instructionId,
                                   TZrUInt32 expected,
                                   TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function->functionToken;
    diagnostic->blockId = blockId;
    diagnostic->instructionId = instructionId;
    diagnostic->sourceId = instructionId != 0u && instructionId <= function->instructionCount
                               ? function->instructions[instructionId - 1u].sourceId
                               : 0u;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
}

/* An edge is loop-carried only when its target dominates its reachable
 * predecessor. Search backward while excluding the target to find any entry
 * path that bypasses it, regardless of block declaration order. */
TZrBool zr_exec_ir_classify_backedges(
        const SZrExecIrFunction *function,
        TZrBool **outBackedges,
        SZrExecIrDiagnostic *diagnostic) {
    TZrBool *backedges = ZR_NULL;
    TZrBool *reachable = ZR_NULL;
    TZrBool *visited = ZR_NULL;
    TZrExecIrBlockId *stack = ZR_NULL;
    TZrUInt32 blockIndex;
    TZrUInt32 iteration;
    TZrBool valid = ZR_TRUE;

    *outBackedges = ZR_NULL;
    if (function->predecessorCount == 0u) return ZR_TRUE;
#if SIZE_MAX <= UINT32_MAX
    if (function->blockCount > SIZE_MAX / sizeof(*stack)) {
        zr_exec_ir_backedge_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                               function, 0u, 0u, UINT32_MAX, function->blockCount);
        return ZR_FALSE;
    }
#endif
    backedges = (TZrBool *)calloc(function->predecessorCount, sizeof(*backedges));
    reachable = (TZrBool *)calloc(function->blockCount, sizeof(*reachable));
    visited = (TZrBool *)calloc(function->blockCount, sizeof(*visited));
    stack = (TZrExecIrBlockId *)malloc((size_t)function->blockCount * sizeof(*stack));
    if (backedges == ZR_NULL || reachable == ZR_NULL || visited == ZR_NULL ||
        stack == ZR_NULL) {
        zr_exec_ir_backedge_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                               function, 0u, 0u, 0u, 0u);
        valid = ZR_FALSE;
        goto cleanup;
    }
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 edgeIndex;
        for (edgeIndex = block->predecessors.start;
             edgeIndex < block->predecessors.start + block->predecessors.count;
             ++edgeIndex) {
            TZrExecIrBlockId predecessor = function->predecessors[edgeIndex];
            if (predecessor == 0u || predecessor > function->blockCount) {
                zr_exec_ir_backedge_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                                       function, block->id, 0u,
                                       function->blockCount, predecessor);
                valid = ZR_FALSE;
                goto cleanup;
            }
        }
    }
    if (function->entryBlockId == ZR_EXEC_IR_BLOCK_ID_INVALID ||
        function->entryBlockId > function->blockCount) goto cleanup;
    reachable[function->entryBlockId - 1u] = ZR_TRUE;
    for (iteration = 0u; iteration < function->blockCount; ++iteration) {
        TZrBool changed = ZR_FALSE;
        for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
            const SZrExecIrBlock *block = &function->blocks[blockIndex];
            TZrUInt32 edgeIndex;
            if (reachable[blockIndex]) continue;
            for (edgeIndex = block->predecessors.start;
                 edgeIndex < block->predecessors.start + block->predecessors.count;
                 ++edgeIndex) {
                if (reachable[function->predecessors[edgeIndex] - 1u]) {
                    reachable[blockIndex] = changed = ZR_TRUE;
                    break;
                }
            }
        }
        if (!changed) break;
    }
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 edgeIndex;
        for (edgeIndex = block->predecessors.start;
             edgeIndex < block->predecessors.start + block->predecessors.count;
             ++edgeIndex) {
            TZrExecIrBlockId predecessor = function->predecessors[edgeIndex];
            TZrUInt32 count = 0u;
            TZrBool bypass = ZR_FALSE;
            if (!reachable[predecessor - 1u]) continue;
            if (predecessor == block->id) {
                backedges[edgeIndex] = ZR_TRUE;
                continue;
            }
            memset(visited, 0, (size_t)function->blockCount * sizeof(*visited));
            visited[predecessor - 1u] = ZR_TRUE;
            stack[count++] = predecessor;
            while (count != 0u && !bypass) {
                TZrExecIrBlockId current = stack[--count];
                const SZrExecIrBlock *currentBlock = &function->blocks[current - 1u];
                TZrUInt32 incomingIndex;
                if (current == function->entryBlockId) {
                    bypass = ZR_TRUE;
                    break;
                }
                for (incomingIndex = currentBlock->predecessors.start;
                     incomingIndex < currentBlock->predecessors.start +
                                         currentBlock->predecessors.count;
                     ++incomingIndex) {
                    TZrExecIrBlockId incoming = function->predecessors[incomingIndex];
                    if (incoming != block->id && !visited[incoming - 1u]) {
                        visited[incoming - 1u] = ZR_TRUE;
                        stack[count++] = incoming;
                    }
                }
            }
            backedges[edgeIndex] = (TZrBool)!bypass;
        }
    }

cleanup:
    free(reachable);
    free(visited);
    free(stack);
    if (valid) *outBackedges = backedges;
    else free(backedges);
    return valid;
}

TZrBool ZrCore_ExecIr_ClassifyBackedges(
        const SZrExecIrFunction *function, TZrBool **outBackedges,
        SZrExecIrDiagnostic *diagnostic) {
    return zr_exec_ir_classify_backedges(function, outBackedges, diagnostic);
}

/**
 * @fn static void zr_exec_ir_backedge_diag(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code, const SZrExecIrFunction *function, TZrUInt32 blockId, TZrUInt32 instructionId, TZrUInt32 expected, TZrUInt32 actual)
 *
 * ### Summary
 * 在回边分类错误分支重建可选诊断，记录函数、位置和expected/actual上下文。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Parameters
 * @param[out] diagnostic 可空；非空须独立可写，不覆盖function及其池，先整体清零。
 * @param[in] code 写入诊断的错误分类。
 * @param[in] function 非空只读借用；读取functionToken，instructionId有效时还读取instructions对应sourceId。
 * @param[in] blockId 直接写入blockId的上下文值。
 * @param[in] instructionId 直接写入instructionId；仅1..instructionCount时读取对应sourceId，否则sourceId为0。
 * @param[in] expected 写入expectedVersion的32位上下文值。
 * @param[in] actual 写入actualVersion的32位上下文值。
 *
 * ### Returns
 * 无返回值；diagnostic为空时直接返回，其余情况完成诊断字段写入。
 *
 * ### Constraints and lifecycle
 * diagnostic为NULL时不读function；非空时先memset整个诊断，再借用非空function读取functionToken及适用sourceId。诊断存储须独立于function及其输入池和其它输出槽，未检查任意重叠。instructionId仅按1..instructionCount选择sourceId，不验证池的真实可读范围；池和描述符由caller保证。函数不分配、不保留参数、不提供同步。三个现有调用传instructionId0，因此sourceId置0，未有直接测试覆盖有效instructionId读取。
 *
 * ### References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:6（definition）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:47（direct_call）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:58（direct_call）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:71（direct_call）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:13（diagnostic write or caller）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:16（diagnostic write or caller）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:18（diagnostic write or caller）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:21（diagnostic write or caller）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:24（diagnostic write or caller）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:25（diagnostic write or caller）。
 */

/**
 * @fn TZrBool zr_exec_ir_classify_backedges( const SZrExecIrFunction *function, TZrBool **outBackedges, SZrExecIrDiagnostic *diagnostic)
 *
 * ### Summary
 * 按predecessor全局槽标记目标块支配可达源块的自然回边。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Parameters
 * @param[in] function 调用期间保持有效且只读的CFG描述符与池；完整前提见下节。
 * @param[out] outBackedges 非空独立可写指针槽，入口先置NULL；成功分配的数组由caller free。
 * @param[out] diagnostic 可空独立诊断输出，仅错误分支清零重建；成功不承诺清空旧值。
 *
 * ### Returns
 * @return TRUE表示完成本实现的分类/约定早退，并非完整CFG验证通过；零predecessor时*outBackedges为NULL，分配后成功时为由调用者free的数组。FALSE表示容量、分配或引用的predecessor ID失败，输出为NULL，非空diagnostic带本次错误。
 *
 * ### Constraints and lifecycle
 * function与outBackedges须非空；outBackedges须是独立可写且可丢弃旧值的指针槽，先由调用者释放旧分配，不能覆盖function描述符、输入池或diagnostic。diagnostic可空，非空须独立可写。调用期间只借用function及各池，禁止并发修改/释放，不保存输入借用或提供锁。调用方先保证blocks可读、块id为下标加1、predecessor池形状和每块range合法且加法不回绕；本函数只再检查所引用predecessor ID落在1..blockCount，不核successor对称性或完整CFG。零predecessor立即TRUE且输出NULL；非零时分配全局predecessorCount项，未被range引用的槽保持FALSE。entry无效/越界并不令valid变FALSE：分配成功并完成predecessor检查后，返回TRUE及全FALSE数组；有意义的支配分类要求调用方提供有效entry。reachable从entry单调传播，最多blockCount轮；只分类可达predecessor，self-edge直接TRUE，其余反向排除目标块，找到绕行entry则FALSE。visited入栈前置位，每个块最多入栈一次，栈容量为blockCount。显式SIZE_MAX/sizeof(stack元素)检查仅在SIZE_MAX<=UINT32_MAX时编译。三个calloc和一个malloc均尝试后统一判空；失败释放全部已取得对象且输出保持NULL；成功转移唯一backedges堆所有权，调用者用free释放。成功不主动清diagnostic，不保证任意输出别名安全。
 *
 * ### References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:31（definition）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:151（direct_call）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:847（direct_call）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:43（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:44（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:46（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:47（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:52（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:53（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:54（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:55（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:56（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:58（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:63（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:70（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:71（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:79（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:81（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:82（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:98（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:99（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:108（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:109（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:114（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:120（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:129（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:135（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:139（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:140（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:141（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:142（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:143（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:144（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:145（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:148（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:841（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:844（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:850（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:851（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:854（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:855（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:858（Core effect verifier caller, frees output）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:34（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:35（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:37（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:38（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:156（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:157（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:158（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:159（Parser loop collector caller/ownership）。
 * - tests/parser/test_ssa_cfg_effects_builder.c:888（finite direct API assertion）。
 * - tests/parser/test_ssa_cfg_effects_builder.c:891（finite output assertion）。
 * - tests/parser/test_ssa_cfg_effects_builder.c:894（free output）。
 * - tests/parser/test_ssa_effects_verifier.c:250（indirect effect loop test）。
 * - tests/parser/test_ssa_effects_verifier.c:321（indirect loop assertion）。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_ClassifyBackedges( const SZrExecIrFunction *function, TZrBool **outBackedges, SZrExecIrDiagnostic *diagnostic)
 *
 * ### Summary
 * 将公共ExecIR回边分类请求委托给私有支配分类实现，转交其输出所有权与失败结果。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Parameters
 * @param[in] function 调用期间保持有效且只读的CFG描述符与池；完整前提见下节。
 * @param[out] outBackedges 非空独立可写指针槽，入口先置NULL；成功分配的数组由caller free。
 * @param[out] diagnostic 可空独立诊断输出，仅错误分支清零重建；成功不承诺清空旧值。
 *
 * ### Returns
 * @return TRUE表示完成本实现的分类/约定早退，并非完整CFG验证通过；零predecessor时*outBackedges为NULL，分配后成功时为由调用者free的数组。FALSE表示容量、分配或引用的predecessor ID失败，输出为NULL，非空diagnostic带本次错误。
 *
 * ### Constraints and lifecycle
 * function与outBackedges须非空；outBackedges须是独立可写且可丢弃旧值的指针槽，先由调用者释放旧分配，不能覆盖function描述符、输入池或diagnostic。diagnostic可空，非空须独立可写。调用期间只借用function及各池，禁止并发修改/释放，不保存输入借用或提供锁。调用方先保证blocks可读、块id为下标加1、predecessor池形状和每块range合法且加法不回绕；本函数只再检查所引用predecessor ID落在1..blockCount，不核successor对称性或完整CFG。零predecessor立即TRUE且输出NULL；非零时分配全局predecessorCount项，未被range引用的槽保持FALSE。entry无效/越界并不令valid变FALSE：分配成功并完成predecessor检查后，返回TRUE及全FALSE数组；有意义的支配分类要求调用方提供有效entry。reachable从entry单调传播，最多blockCount轮；只分类可达predecessor，self-edge直接TRUE，其余反向排除目标块，找到绕行entry则FALSE。visited入栈前置位，每个块最多入栈一次，栈容量为blockCount。显式SIZE_MAX/sizeof(stack元素)检查仅在SIZE_MAX<=UINT32_MAX时编译。三个calloc和一个malloc均尝试后统一判空；失败释放全部已取得对象且输出保持NULL；成功转移唯一backedges堆所有权，调用者用free释放。成功不主动清diagnostic，不保证任意输出别名安全。
 *
 * ### References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:148（definition）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:35（direct_call）。
 * - tests/parser/test_ssa_cfg_effects_builder.c:888（direct_call）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:31（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:43（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:44（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:46（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:47（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:52（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:53（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:54（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:55（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:56（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:58（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:63（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:70（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:71（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:79（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:81（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:82（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:98（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:99（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:108（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:109（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:114（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:120（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:129（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:135（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:139（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:140（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:141（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:142（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:143（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:144（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:145（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:151（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:841（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:844（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:847（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:850（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:851（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:854（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:855（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:858（Core effect verifier caller, frees output）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:34（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:37（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:38（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:156（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:157（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:158（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:159（Parser loop collector caller/ownership）。
 * - tests/parser/test_ssa_cfg_effects_builder.c:891（finite output assertion）。
 * - tests/parser/test_ssa_cfg_effects_builder.c:894（free output）。
 * - tests/parser/test_ssa_effects_verifier.c:250（indirect effect loop test）。
 * - tests/parser/test_ssa_effects_verifier.c:321（indirect loop assertion）。
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:702（public declaration is dependency, excluded unit）。
 */
