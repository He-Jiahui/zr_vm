#include "exec_ir_verify_constants.h"

#include <string.h>

static TZrBool zr_exec_ir_constant_failure(
        const SZrExecIrFunction *function, TZrUInt32 instructionIndex,
        EZrExecutionDiagnosticCode code, TZrUInt32 expected, TZrUInt32 actual,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 blockIndex;
    if (diagnostic == ZR_NULL) return ZR_FALSE;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function->functionToken;
    diagnostic->instructionId = instructionIndex + 1u;
    diagnostic->sourceId = function->instructions[instructionIndex].sourceId;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        if (instructionIndex >= block->instructionRange.start &&
            instructionIndex - block->instructionRange.start <
                block->instructionRange.count) {
            diagnostic->blockId = block->id;
            break;
        }
    }
    return ZR_FALSE;
}

TZrBool zr_exec_ir_verify_owned_constants(
        const SZrExecIrModule *module, const SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 instructionIndex;
    if (module->constantCount == 0u) return ZR_TRUE;
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount; ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        const SZrExecIrConstant *constant;
        const SZrExecIrValue *result;
        TZrExecIrValueId resultId;
        if (instruction->opcode != ZR_EXEC_IR_OPCODE_CONSTANT) continue;
        if (instruction->layoutId >= module->constantCount) {
            return zr_exec_ir_constant_failure(function, instructionIndex,
                    ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, module->constantCount,
                    instruction->layoutId, diagnostic);
        }
        /* Full verification established one result and valid pool/value IDs. */
        resultId = function->results[instruction->resultRange.start];
        result = &function->values[resultId - 1u];
        constant = &module->constants[instruction->layoutId];
        if (constant->typeToken != result->typeToken) {
            return zr_exec_ir_constant_failure(function, instructionIndex,
                    ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, result->typeToken,
                    constant->typeToken, diagnostic);
        }
        if (instruction->typeToken != 0u &&
            instruction->typeToken != result->typeToken) {
            return zr_exec_ir_constant_failure(function, instructionIndex,
                    ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, result->typeToken,
                    instruction->typeToken, diagnostic);
        }
    }
    return ZR_TRUE;
}

/**
 * @fn TZrBool zr_exec_ir_constant_failure(const SZrExecIrFunction *function, TZrUInt32 instructionIndex, EZrExecutionDiagnosticCode code, TZrUInt32 expected, TZrUInt32 actual, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 为模块常量引用校验的首个错误重建结构化诊断，并统一返回 ZR_FALSE。指令下标转换为从 1 开始的 instructionId；blockId 取 blocks 数组中第一个包含该指令的区间，未找到时保留清零后的 0。
 *
 * ## Parameters
 * @param[in] function 已通过前置验证且在调用期间保持稳定的函数；需要填诊断时，其指令与块存储必须可读。
 * @param[in] instructionIndex instructions 数组中有效的从 0 开始的指令下标；不是从 1 开始的 instructionId。
 * @param[in] code 调用方选定的 INVALID_RANGE 或 INVALID_VALUE 诊断码，本助手不再次校验枚举值。
 * @param[in] expected 写入 expectedVersion 的预期上界或结果 typeToken；字段名不表示此处比较版本。
 * @param[in] actual 写入 actualVersion 的实际 layoutId 或不匹配的 typeToken。
 * @param[out] diagnostic 可为空；非空时必须指向独立可写的 SZrExecIrDiagnostic 记录。
 *
 * ## Returns
 * @return 始终返回 ZR_FALSE；diagnostic 为空时直接返回且不读取 function 内容。
 *
 * ## Constraints and lifecycle
 * 非空诊断先被整体清零，再写入 code、functionToken、instructionId、sourceId、expectedVersion、actualVersion 及可定位的 blockId；未赋值的哈希等字段保留 0。诊断存储不得与仍需读取的 function 记录、instructions 或 blocks 存储重叠，否则清零会破坏输入；本助手不检测别名、下标或存储有效性。
 * 块区间用先比较 instructionIndex >= start、再比较差值 < count 的方式查找，避免为查找形成 start + count。多个区间匹配时采用数组中首个匹配项，不验证区间唯一性。
 * 不分配或释放资源，不改写函数及其池，不保存指针；诊断只携带标量身份。输入借用在调用期间必须有效且不得并发修改或释放；调用结束后不留下函数或池的借用。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:44、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:53、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:59：三种错误的实际直接调用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:10：空诊断快速返回；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:11：整体清零；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:18：块位置扫描。
 * - zr_vm_core/include/zr_vm_core/execution_contract.h:111：诊断记录包含标量身份及哈希字段。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:22、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:53、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:102：索引及两种类型错误的有限诊断断言。
 */

/**
 * @fn TZrBool zr_exec_ir_verify_owned_constants(const SZrExecIrModule *module, const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在模块结构校验及当前函数完整验证成功后，补充检查 CONSTANT 指令对模块非空常量池的引用。按指令数组顺序遇错即返回；先检查 layoutId 上界，再比较池项与结果的 typeToken，最后比较非零显式指令 typeToken。
 *
 * ## Parameters
 * @param[in] module 非空、已通过模块结构校验的模块；提供可读的 constantCount 及对应 constants 池。
 * @param[in] function 非空、已通过完整验证的当前函数；CONSTANT 已具有一个结果、有效 resultRange 及有效结果 ValueId。
 * @param[out] diagnostic 可为空；非空时为与所有输入记录和被读池存储分离的可写诊断槽，失败时整体重写，成功时不改写。
 *
 * ## Returns
 * @return constantCount 为 0 或所有被检查的 CONSTANT 引用满足索引和类型条件时返回 ZR_TRUE；首个越界或类型不匹配时返回 ZR_FALSE。layoutId 越界使用 INVALID_RANGE；两种类型不匹配使用 INVALID_VALUE，预期类型为结果 typeToken。
 *
 * ## Constraints and lifecycle
 * 该内部函数不替代结构或完整验证，也不再次检查空 module/function、池指针、count/capacity、resultRange 或 ValueId。唯一生产直接调用位于 VerifyModule 对当前函数 VerifyFunction(ALL) 成功之后；从此前提建立到本函数完成，输入记录及被读池必须仍然可读、保持一致，且不得并发修改或释放。
 * constantCount 为 0 时不扫描指令、不对 layoutId 或类型施加模块池限制，也不解析外部 Oracle 池。非空池中 typeToken 按标量身份相等比较；instruction.typeToken 为 0 时不施加显式指令类型比较。本函数不解释常量 flags/bits，不证明池的分配来源或任意地址的可读性。
 * 仅失败路径调用诊断助手并重写非空诊断，成功路径保留传入诊断原值；公开 VerifyModule 的前置入口另行清除诊断。diagnostic 不能与 module、function 及仍被读取的池重叠，本函数不执行别名检测。
 * 本函数自身不分配、不释放、不发布或替换池，不改变输入或其借用有效期；调用期间仅借用输入，返回布尔值与可选标量诊断，不保留输入指针。该无分配结论不外推到前置完整验证。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:702、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:706、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:711：模块结构、当前函数完整验证和本函数的实际顺序。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:621、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:639、zr_vm_core/include/zr_vm_core/exec_ir_opcode.def:16：CONSTANT 单结果及结果 ValueId 的前置保证。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:34、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:43、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:49、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:52、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:57：零池、边界和两次类型比较的执行顺序。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:8、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:20、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:51、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:102：有效及错误模块的实际断言。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:85、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:87、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:118、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:120：无模块池与外部 Oracle 配置的有限证据。
 */
