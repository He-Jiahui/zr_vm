#include "zr_vm_parser/exec_ir_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(TZrBool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void make_defined_operand(SZrExecIrFunction *function) {
    TZrExecIrValueId value, result = 0u;
    TZrExecIrInstructionId instruction = 0u;
    SZrExecIrInstruction constant;

    ZrCore_ExecIr_FunctionInit(function);
    function->functionToken = (TZrMetadataToken)77u;
    function->entryBlockId = ZrCore_ExecIr_FunctionAddBlock(function,
                                                           ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    check(function->entryBlockId == 1u, "could not create entry block");
    value = ZrCore_ExecIr_FunctionAddValue(function, (TZrMetadataToken)1u,
                                          ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                                          ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(value == 1u, "could not create defined value");
    result = value;
    memset(&constant, 0, sizeof(constant));
    constant.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    check(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u,
                                              &constant.results) &&
              ZrCore_ExecIr_FunctionAppendInstruction(function, &constant,
                                                      &instruction) &&
              instruction == 1u,
          "could not define the operand");
    check(ZrCore_ExecIr_FunctionAppendOperands(function, &value, 1u, NULL),
          "could not add the operand");
}

static void test_rejects_range_past_logical_operand_count(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction drop;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrInstructionId id = 0u;

    make_defined_operand(&function);
    check(function.operandCapacity > function.operandCount,
          "fixture needs spare physical operand capacity");
    function.operands[function.operandCount] = 1u;
    memset(&drop, 0, sizeof(drop));
    drop.opcode = ZR_EXEC_IR_OPCODE_DROP;
    drop.operands.start = function.operandCount;
    drop.operands.count = 1u;
    check(ZrCore_ExecIr_FunctionAppendInstruction(&function, &drop, &id) && id == 2u,
          "could not add the operation with an invalid logical range");
    check(!ZrParser_ExecIr_BuildSsa(&function, &diagnostic),
          "SSA pass accepted operands beyond the logical side-pool end");
    check(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.instructionId == 2u && diagnostic.functionToken == 77u,
          "invalid SSA operand range lost the function and instruction location");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_accepts_defined_operand_in_range(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction drop;
    SZrExecIrDiagnostic diagnostic;

    make_defined_operand(&function);
    memset(&drop, 0, sizeof(drop));
    drop.opcode = ZR_EXEC_IR_OPCODE_DROP;
    drop.operands.count = 1u;
    check(ZrCore_ExecIr_FunctionAppendInstruction(&function, &drop, NULL),
          "could not add the valid operand operation");
    check(ZrParser_ExecIr_BuildSsa(&function, &diagnostic),
          "SSA pass rejected an in-range defined operand");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_accepts_explicit_external_entry_operand(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction drop;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId external;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = (TZrMetadataToken)78u;
    external = ZrCore_ExecIr_FunctionAddExternalValue(
            &function, (TZrMetadataToken)1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(external == 1u, "could not create external entry value");
    check(ZrCore_ExecIr_FunctionAppendOperands(
                  &function, &external, 1u, NULL),
          "could not add external entry operand");
    memset(&drop, 0, sizeof(drop));
    drop.opcode = ZR_EXEC_IR_OPCODE_DROP;
    drop.operands.count = 1u;
    check(ZrCore_ExecIr_FunctionAppendInstruction(&function, &drop, NULL),
          "could not add external entry use");
    check(ZrParser_ExecIr_BuildSsa(&function, &diagnostic),
          "SSA pass rejected an explicit external entry value");
    check(ZrCore_ExecIr_VerifyFunction(
                  &function, ZR_EXEC_IR_VERIFY_SSA, &diagnostic),
          "SSA verifier rejected an explicit external entry value");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_core_verifier_rejects_unmarked_undefined_operand(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction drop;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId undefined;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = (TZrMetadataToken)79u;
    undefined = ZrCore_ExecIr_FunctionAddValue(
            &function, (TZrMetadataToken)1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(ZrCore_ExecIr_FunctionAppendOperands(
                  &function, &undefined, 1u, NULL),
          "could not add undefined operand");
    memset(&drop, 0, sizeof(drop));
    drop.opcode = ZR_EXEC_IR_OPCODE_DROP;
    drop.operands.count = 1u;
    check(ZrCore_ExecIr_FunctionAppendInstruction(&function, &drop, NULL),
          "could not add undefined value use");
    check(!ZrCore_ExecIr_VerifyFunction(
                  &function, ZR_EXEC_IR_VERIFY_SSA, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
              diagnostic.instructionId == 1u,
          "SSA verifier accepted an unmarked undefined operand");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_external_entry_cannot_be_instruction_result(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction constant;
    TZrExecIrValueId external;

    ZrCore_ExecIr_FunctionInit(&function);
    external = ZrCore_ExecIr_FunctionAddExternalValue(
            &function, (TZrMetadataToken)1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    memset(&constant, 0, sizeof(constant));
    constant.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    check(ZrCore_ExecIr_FunctionAppendResults(
                  &function, &external, 1u, &constant.results),
          "could not add external result fixture");
    check(!ZrCore_ExecIr_FunctionAppendInstruction(
                  &function, &constant, NULL) &&
              function.instructionCount == 0u,
          "ordinary instruction redefined an external entry value");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_external_entry_cannot_be_phi_result(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrPhi phi;
    TZrExecIrValueId external;
    TZrExecIrBlockId entry;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = (TZrMetadataToken)81u;
    external = ZrCore_ExecIr_FunctionAddExternalValue(
            &function, (TZrMetadataToken)1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    entry = ZrCore_ExecIr_FunctionAddBlock(
            &function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    check(external == 1u && entry == ZR_EXEC_IR_BLOCK_ID_ENTRY,
          "could not create external phi-result fixture");
    memset(&phi, 0, sizeof(phi));
    phi.result = external;
    check(ZrCore_ExecIr_FunctionAppendPhis(
                  &function, &phi, 1u, &function.blocks[0].phis),
          "could not append external phi-result fixture");
    check(!ZrCore_ExecIr_VerifyFunction(
                  &function, ZR_EXEC_IR_VERIFY_SSA, &diagnostic) &&
              diagnostic.code ==
                      ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION,
          "SSA verifier accepted an external entry as a phi result");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_rejects_unknown_value_flags(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId value;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = (TZrMetadataToken)80u;
    value = ZrCore_ExecIr_FunctionAddValue(
            &function, (TZrMetadataToken)1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    function.values[value - 1u].flags = (TZrUInt32)1u << 31u;
    check(!ZrParser_ExecIr_BuildSsa(&function, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
              diagnostic.instructionId == 0u,
          "SSA pass accepted unknown flags on an unused value");
    check(!ZrCore_ExecIr_VerifyFunction(
                  &function, ZR_EXEC_IR_VERIFY_SSA, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
          "verifier accepted an unknown value flag");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_rejects_promotable_value_without_place_address(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId value;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = (TZrMetadataToken)82u;
    value = ZrCore_ExecIr_FunctionAddValue(
            &function, (TZrMetadataToken)1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    function.values[value - 1u].flags =
            ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE;
    check(!ZrParser_ExecIr_BuildSsa(&function, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
          "SSA pass accepted promotable metadata on a non-place value");
    check(!ZrCore_ExecIr_VerifyFunction(
                  &function, ZR_EXEC_IR_VERIFY_SSA, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
          "verifier accepted promotable metadata on a non-place value");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_rejects_wrapped_operand_range(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction drop;
    SZrExecIrDiagnostic diagnostic;

    make_defined_operand(&function);
    memset(&drop, 0, sizeof(drop));
    drop.opcode = ZR_EXEC_IR_OPCODE_DROP;
    drop.operands.start = UINT32_MAX;
    drop.operands.count = 1u;
    check(ZrCore_ExecIr_FunctionAppendInstruction(&function, &drop, NULL),
          "could not add the wrapped operand range");
    check(!ZrParser_ExecIr_BuildSsa(&function, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.instructionId == 2u,
          "SSA pass accepted an overflowing operand range");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_rejects_missing_operand_storage(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId *savedOperands;

    make_defined_operand(&function);
    savedOperands = function.operands;
    function.operands = NULL;
    check(!ZrParser_ExecIr_BuildSsa(&function, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.functionToken == 77u,
          "SSA pass accepted a missing operand backing array");
    function.operands = savedOperands;
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_rejects_unknown_opcode_with_location(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction unknown;
    SZrExecIrDiagnostic diagnostic;

    make_defined_operand(&function);
    memset(&unknown, 0, sizeof(unknown));
    unknown.opcode = ZR_EXEC_IR_OPCODE_COUNT;
    check(ZrCore_ExecIr_FunctionAppendInstruction(&function, &unknown, NULL),
          "could not add the unknown opcode");
    check(!ZrParser_ExecIr_BuildSsa(&function, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE &&
              diagnostic.functionToken == 77u && diagnostic.instructionId == 2u,
          "SSA pass lost the unknown opcode location");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_ssa_promotion_validates_rows_before_noop_return(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_FunctionInit(&function);
    function.functionToken = (TZrMetadataToken)83u;
    function.bindingRowsSchemaVersion = ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED;
    function.bindingRowCount = 1u;
    function.bindingRowCapacity = 1u;
    /* No LOAD/STORE makes place promotion a no-op.  Its direct SSA entry must
     * still reject malformed owned rows before taking that fast path. */
    check(!ZrParser_ExecIr_BuildSsa(&function, &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
          "SSA promotion skipped binding-row validation on its no-op path");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_rejects_undefined_operand_with_source_location(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction drop;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId undefined;

    make_defined_operand(&function);
    undefined = ZrCore_ExecIr_FunctionAddValue(
            &function, (TZrMetadataToken)1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(undefined == 2u, "could not create undefined operand fixture");
    function.operands[0] = undefined;
    memset(&drop, 0, sizeof(drop));
    drop.opcode = ZR_EXEC_IR_OPCODE_DROP;
    drop.sourceId = 247u;
    drop.operands.count = 1u;
    check(ZrCore_ExecIr_FunctionAppendInstruction(&function, &drop, NULL),
          "could not append undefined operand use");
    function.blocks[0].instructionRange.count = 2u;
    check(!ZrParser_ExecIr_BuildSsa(&function, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
              diagnostic.functionToken == 77u &&
              diagnostic.blockId == 1u && diagnostic.instructionId == 2u &&
              diagnostic.sourceId == 247u,
          "SSA operand error lost its source and block location");
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    test_accepts_defined_operand_in_range();
    test_accepts_explicit_external_entry_operand();
    test_core_verifier_rejects_unmarked_undefined_operand();
    test_external_entry_cannot_be_instruction_result();
    test_external_entry_cannot_be_phi_result();
    test_rejects_unknown_value_flags();
    test_rejects_promotable_value_without_place_address();
    test_rejects_range_past_logical_operand_count();
    test_rejects_wrapped_operand_range();
    test_rejects_missing_operand_storage();
    test_rejects_unknown_opcode_with_location();
    test_ssa_promotion_validates_rows_before_noop_return();
    test_rejects_undefined_operand_with_source_location();
    puts("ssa value validation PASS");
    return EXIT_SUCCESS;
}

/**
 * @fn void check(TZrBool condition, const char *message)
 *
 * ## Summary
 * 将value validation的布尔条件转为消息和进程退出，用于setup与语义断言。
 *
 * ## Parameters
 * @param[in] condition 已求值的TZrBool检查结果；零为失败。
 * @param[in] message 失败时借用的诊断消息。
 *
 * ## Returns
 * 无C返回值；真条件正常返回，假条件exit(EXIT_FAILURE)。
 *
 * ## Constraints and lifecycle
 * message失败期间必须可读且NUL结尾；不复制消息。成功无额外状态，失败无栈展开/Unity清理，由exit终止可执行进程。其他TU同名check不是此实体。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:7
 * @see tests/parser/test_ssa_value_validation.c:8
 * @see tests/parser/test_ssa_value_validation.c:9
 * @see tests/parser/test_ssa_value_validation.c:10
 */

/**
 * @fn void make_defined_operand(SZrExecIrFunction *function)
 *
 * ## Summary
 * 清零初始化一个用于直接BuildSsa预校验的最小Function：token77、entry块、一条定义value1的CONSTANT以及一项operand池。
 *
 * ## Parameters
 * @param[out] function 输出fixture记录；必须非NULL、可写且无需要保留/释放的旧owned池。
 *
 * ## Returns
 * 无C返回值；构造检查均满足后留下上述最小fixture；失败由check终止进程，不返回部分状态给调用方。
 *
 * ## Constraints and lifecycle
 * function须为独立可写记录且不得持有待保留的分配，FunctionInit不会先Free旧内容。生成的function.id仍为零，block指令范围也未补成可执行CFG，因此不能当作通用VerifyFunction/VM正例fixture。成功后调用者拥有各池，须FreeFunction；中途check失败直接退出进程，部分池不会由本助手回滚。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:14
 * @see tests/parser/test_ssa_value_validation.c:19
 * @see tests/parser/test_ssa_value_validation.c:21
 * @see tests/parser/test_ssa_value_validation.c:24
 * @see tests/parser/test_ssa_value_validation.c:31
 * @see tests/parser/test_ssa_value_validation.c:37
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c:53
 */

/**
 * @fn void test_rejects_range_past_logical_operand_count(void)
 *
 * ## Summary
 * 在实际operand备用容量内写入一个元素，却让DROP从逻辑operandCount开始读取，检查BuildSsa按逻辑上界拒绝而不把物理容量当作有效元素数。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * make_defined_operand生成的最小function仅为Parser预校验fixture，不能据此视为完整可执行CFG。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:41
 * @see tests/parser/test_ssa_value_validation.c:47
 * @see tests/parser/test_ssa_value_validation.c:48
 * @see tests/parser/test_ssa_value_validation.c:50
 * @see tests/parser/test_ssa_value_validation.c:55
 * @see tests/parser/test_ssa_value_validation.c:57
 * @see tests/parser/test_ssa_value_validation.c:59
 * @see tests/parser/test_ssa_value_validation.c:62
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c:87
 */

/**
 * @fn void test_accepts_defined_operand_in_range(void)
 *
 * ## Summary
 * 给最小fixture加入从索引0读取已定义value1的DROP，检查直接BuildSsa正例。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * make_defined_operand生成的最小function仅为Parser预校验fixture，不能据此视为完整可执行CFG。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:65
 * @see tests/parser/test_ssa_value_validation.c:70
 * @see tests/parser/test_ssa_value_validation.c:74
 * @see tests/parser/test_ssa_value_validation.c:76
 * @see tests/parser/test_ssa_value_validation.c:78
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c:99
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c:107
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c:898
 */

/**
 * @fn void test_accepts_explicit_external_entry_operand(void)
 *
 * ## Summary
 * 创建显式EXTERNAL_ENTRY value作为DROP输入，检查BuildSsa和Core SSA验证均接受无需普通instruction definition的入口值。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:81
 * @see tests/parser/test_ssa_value_validation.c:88
 * @see tests/parser/test_ssa_value_validation.c:90
 * @see tests/parser/test_ssa_value_validation.c:103
 * @see tests/parser/test_ssa_value_validation.c:105
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c:96
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:598
 */

/**
 * @fn void test_core_verifier_rejects_unmarked_undefined_operand(void)
 *
 * ## Summary
 * 直接让Core SSA入口读取无EXTERNAL_ENTRY标记、也无定义的普通value，检查INVALID_VALUE和instruction1。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:111
 * @see tests/parser/test_ssa_value_validation.c:120
 * @see tests/parser/test_ssa_value_validation.c:124
 * @see tests/parser/test_ssa_value_validation.c:132
 * @see tests/parser/test_ssa_value_validation.c:134
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:704
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:708
 */

/**
 * @fn void test_external_entry_cannot_be_instruction_result(void)
 *
 * ## Summary
 * 将external value放入CONSTANT result池，检查AppendInstruction拒绝并保持instructionCount零。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:140
 * @see tests/parser/test_ssa_value_validation.c:146
 * @see tests/parser/test_ssa_value_validation.c:152
 * @see tests/parser/test_ssa_value_validation.c:155
 * @see tests/parser/test_ssa_value_validation.c:157
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:773
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:779
 */

/**
 * @fn void test_external_entry_cannot_be_phi_result(void)
 *
 * ## Summary
 * 把external value作为entry block的PHI result，检查Core SSA入口返回DUPLICATE_DEFINITION。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:162
 * @see tests/parser/test_ssa_value_validation.c:172
 * @see tests/parser/test_ssa_value_validation.c:176
 * @see tests/parser/test_ssa_value_validation.c:182
 * @see tests/parser/test_ssa_value_validation.c:185
 * @see tests/parser/test_ssa_value_validation.c:187
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:598
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:675
 */

/**
 * @fn void test_rejects_unknown_value_flags(void)
 *
 * ## Summary
 * 给未使用value设置高位未知flag，分别检查Parser BuildSsa和Core SSA公开入口拒绝；Parser还须把instructionId保留为零。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:193
 * @see tests/parser/test_ssa_value_validation.c:201
 * @see tests/parser/test_ssa_value_validation.c:205
 * @see tests/parser/test_ssa_value_validation.c:206
 * @see tests/parser/test_ssa_value_validation.c:210
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c:67
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:386
 */

/**
 * @fn void test_rejects_promotable_value_without_place_address(void)
 *
 * ## Summary
 * 仅设置PROMOTABLE_PLACE而缺少PLACE_ADDRESS，检查Parser与Core入口均以INVALID_VALUE拒绝。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:217
 * @see tests/parser/test_ssa_value_validation.c:229
 * @see tests/parser/test_ssa_value_validation.c:231
 * @see tests/parser/test_ssa_value_validation.c:234
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c:68
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:388
 */

/**
 * @fn void test_rejects_wrapped_operand_range(void)
 *
 * ## Summary
 * 把DROP operandRange设置为startUINT32_MAX/count1，检查BuildSsa拒绝并定位instruction2。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * make_defined_operand生成的最小function仅为Parser预校验fixture，不能据此视为完整可执行CFG。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:241
 * @see tests/parser/test_ssa_value_validation.c:246
 * @see tests/parser/test_ssa_value_validation.c:249
 * @see tests/parser/test_ssa_value_validation.c:251
 * @see tests/parser/test_ssa_value_validation.c:253
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c:87
 */

/**
 * @fn void test_rejects_missing_operand_storage(void)
 *
 * ## Summary
 * 暂存真实operand指针后将其置NULL，检查非空逻辑池在BuildSsa入口被拒绝，并恢复指针后Free。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * make_defined_operand生成的最小function仅为Parser预校验fixture，不能据此视为完整可执行CFG。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:260
 * @see tests/parser/test_ssa_value_validation.c:265
 * @see tests/parser/test_ssa_value_validation.c:266
 * @see tests/parser/test_ssa_value_validation.c:267
 * @see tests/parser/test_ssa_value_validation.c:268
 * @see tests/parser/test_ssa_value_validation.c:272
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c:59
 */

/**
 * @fn void test_rejects_unknown_opcode_with_location(void)
 *
 * ## Summary
 * 在已定义operand fixture后保存COUNT sentinel未知opcode，检查BuildSsa报告UNKNOWN_OPCODE/function77/instruction2。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * make_defined_operand生成的最小function仅为Parser预校验fixture，不能据此视为完整可执行CFG。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:276
 * @see tests/parser/test_ssa_value_validation.c:281
 * @see tests/parser/test_ssa_value_validation.c:283
 * @see tests/parser/test_ssa_value_validation.c:284
 * @see tests/parser/test_ssa_value_validation.c:286
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c:81
 */

/**
 * @fn void test_ssa_promotion_validates_rows_before_noop_return(void)
 *
 * ## Summary
 * 构造typed binding rowCount/capacity1但缺失row池、且无LOAD/STORE的function，检查直接BuildSsa不会以promotion无操作路径绕过row形状校验。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:293
 * @see tests/parser/test_ssa_value_validation.c:297
 * @see tests/parser/test_ssa_value_validation.c:299
 * @see tests/parser/test_ssa_value_validation.c:304
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c:895
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa_promotion.c:898
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:149
 */

/**
 * @fn void test_rejects_undefined_operand_with_source_location(void)
 *
 * ## Summary
 * 把DROP输入改为未定义value2、设置source247并显式覆盖块范围，检查Parser早期错误保留function/block/instruction/source四维定位。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * make_defined_operand生成的最小function仅为Parser预校验fixture，不能据此视为完整可执行CFG。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:310
 * @see tests/parser/test_ssa_value_validation.c:316
 * @see tests/parser/test_ssa_value_validation.c:317
 * @see tests/parser/test_ssa_value_validation.c:322
 * @see tests/parser/test_ssa_value_validation.c:325
 * @see tests/parser/test_ssa_value_validation.c:329
 * @see tests/parser/test_ssa_value_validation.c:330
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c:99
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c:17
 * @see zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c:30
 */

/**
 * @fn int main(void)
 *
 * ## Summary
 * 以普通C调用顺序运行13个value-validation case，全部到达末尾后打印PASS并返回成功。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 顺序检查正常完成返回EXIT_SUCCESS；任一check失败由exit(EXIT_FAILURE)退出，未必返回到main。
 *
 * ## Constraints and lifecycle
 * 没有RUN_TEST、Unity hook或失败恢复；check失败直接exit(EXIT_FAILURE)，跳过后续case。各case正常路径自行FreeFunction，main不维护跨case fixture或回收中途退出的局部池。测试函数数量与运行场景数不能扩成生产验证器完整覆盖。
 *
 * ## References
 * @see tests/parser/test_ssa_value_validation.c:339
 * @see tests/parser/test_ssa_value_validation.c:340
 * @see tests/parser/test_ssa_value_validation.c:352
 * @see tests/parser/test_ssa_value_validation.c:353
 * @see tests/cmake/ssa-tests.cmake:215
 * @see tests/cmake/ssa-tests.cmake:239
 */
