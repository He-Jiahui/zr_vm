static void test_module_constant_pool_accepts_valid_reference(void) {
    SModuleConstantFixture fixture;
    SZrExecIrDiagnostic diagnostic;

    build_constant_module(&fixture, ZR_TRUE);
    expect_true(fixture.module.constantCount == 1u &&
                    fixture.function->instructions[0].layoutId == 0u &&
                    ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic),
                "valid module-owned CONSTANT pool index was rejected");
    free_constant_module(&fixture);
}

static void expect_module_constant_index_failure(TZrUInt32 index,
                                                 const char *message) {
    SModuleConstantFixture fixture;
    SZrExecIrDiagnostic diagnostic;

    build_constant_module(&fixture, ZR_TRUE);
    fixture.function->instructions[0].layoutId = index;
    expect_true(!ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic),
                message);
    expect_true(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
                    diagnostic.functionToken ==
                            fixture.function->functionToken &&
                    diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY &&
                    diagnostic.instructionId == 1u &&
                    diagnostic.sourceId == 701u &&
                    diagnostic.expectedVersion == 1u &&
                    diagnostic.actualVersion == index,
                "module constant index diagnostic lost structured location/range");
    free_constant_module(&fixture);
}

static void test_module_constant_pool_rejects_count_boundary_index(void) {
    expect_module_constant_index_failure(
            1u, "module constant index equal to constantCount was accepted");
}

static void test_module_constant_pool_rejects_uint32_max_index(void) {
    expect_module_constant_index_failure(
            UINT32_MAX, "UINT32_MAX module constant index was accepted");
}

static void test_module_constant_pool_rejects_constant_type_mismatch(void) {
    SModuleConstantFixture fixture;
    SZrExecIrDiagnostic diagnostic;

    build_constant_module(&fixture, ZR_TRUE);
    fixture.module.constants[0].typeToken =
            (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL;
    expect_true(!ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic),
                "module CONSTANT pool entry with a mismatched type was accepted");
    expect_true(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
                    diagnostic.functionToken ==
                            fixture.function->functionToken &&
                    diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY &&
                    diagnostic.instructionId == 1u &&
                    diagnostic.sourceId == 701u &&
                    diagnostic.expectedVersion ==
                            (TZrUInt32)ZR_VALUE_TYPE_INT64 &&
                    diagnostic.actualVersion ==
                            (TZrUInt32)ZR_VALUE_TYPE_BOOL,
                "module constant type diagnostic lost structured location/types");
    free_constant_module(&fixture);
}

static void test_module_without_owned_pool_accepts_external_oracle_pool(void) {
    SModuleConstantFixture fixture;
    SZrExecIrOracleValue externalConstant;
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;

    build_constant_module(&fixture, ZR_FALSE);
    memset(&externalConstant, 0, sizeof(externalConstant));
    externalConstant.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    externalConstant.as.signedInteger = 73;
    memset(&input, 0, sizeof(input));
    input.function = fixture.function;
    input.constants = &externalConstant;
    input.constantCount = 1u;
    ZrCore_ExecIr_OracleResultInit(&execution);

    expect_true(fixture.module.constantCount == 0u &&
                    ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic),
                "module without an owned pool rejected a structurally valid CONSTANT");
    expect_true(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic) &&
                    execution.returned &&
                    execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
                    execution.returnValue.as.signedInteger == 73,
                "separately configured Oracle constant pool was not consumed");

    ZrCore_ExecIr_OracleResultFree(&execution);
    free_constant_module(&fixture);
}

static void test_module_constant_pool_rejects_instruction_type_mismatch(void) {
    SModuleConstantFixture fixture;
    SZrExecIrDiagnostic diagnostic;
    build_constant_module(&fixture, ZR_TRUE);
    fixture.function->instructions[0].typeToken = ZR_VALUE_TYPE_BOOL;
    expect_true(!ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic) &&
                    diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
                    diagnostic.functionToken == fixture.function->functionToken &&
                    diagnostic.blockId == 1u && diagnostic.instructionId == 1u &&
                    diagnostic.sourceId == 701u &&
                    diagnostic.expectedVersion == ZR_VALUE_TYPE_INT64 &&
                    diagnostic.actualVersion == ZR_VALUE_TYPE_BOOL,
                "module CONSTANT accepted a mismatched explicit instruction type");
    free_constant_module(&fixture);
}

static void test_function_only_verifier_does_not_require_an_owned_pool(void) {
    SModuleConstantFixture fixture;
    SZrExecIrDiagnostic diagnostic;
    build_constant_module(&fixture, ZR_FALSE);
    fixture.function->instructions[0].layoutId = UINT32_MAX;
    expect_true(ZrCore_ExecIr_VerifyFunction(
                        fixture.function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic) &&
                    ZrCore_ExecIr_VerifyModule(&fixture.module, &diagnostic),
                "no-pool verification imposed a module pool bound");
    free_constant_module(&fixture);
}

/**
 * @fn void test_module_constant_pool_accepts_valid_reference(void)
 *
 * ## Summary
 * 确认固定 count1 模块以 layoutId0 引用 INT64 池项时通过公共模块验证。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 各项 expect_true 条件成立后正常返回，无 C 返回值；任一条件失败时 expect_true 调用 exit(EXIT_FAILURE) 结束整个测试程序，跳过后续场景及尚未执行的本地尾部释放。这里不使用 Unity longjmp、setUp 或 tearDown。
 *
 * ## Constraints and lifecycle
 * 实际检查 constantCount==1、第一指令 layoutId==0 及 VerifyModule==TRUE。池项与结果均为 INT64，显式 instruction.typeToken 由构造保持 0；该正例不证明非零匹配的显式类型、不检查成功诊断内容，也不执行模块池 bits42 的 Oracle 消费。
 * 本场景只使用局部夹具、诊断和固定构造，诊断槽与输入记录及池分离。正常路径调用 free_constant_module；不把意外断言失败后的进程退出等同于完成显式资源清理。此文档描述已有源码意图和有限断言，本提案未运行测试。
 *
 * ## References
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:100。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:5。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:6。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:8。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:10。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:43。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:52。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:57。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:14、tests/parser/test_ssa_module_constant_pool_verifier.c:17：共用检查失败语义。
 */

/**
 * @fn void expect_module_constant_index_failure(TZrUInt32 index, const char *message)
 *
 * ## Summary
 * 为两个越界索引场景构造同一 count1 模块，把 CONSTANT.layoutId 改为给定索引，并检查公共模块验证的 FALSE 与完整位置/范围诊断。
 *
 * ## Parameters
 * @param[in] index 本助手期望被拒绝的模块池索引；当前实际调用为 1 和 UINT32_MAX，均不小于固定 constantCount1。
 * @param[in] message 验证器意外接受时交给 expect_true 的非空、可读 NUL 结尾消息字符串；不取得所有权或保留地址。
 *
 * ## Returns
 * 各项 expect_true 条件成立后正常返回，无 C 返回值；任一条件失败时 expect_true 调用 exit(EXIT_FAILURE) 结束整个测试程序，跳过后续场景及尚未执行的本地尾部释放。这里不使用 Unity longjmp、setUp 或 tearDown。
 *
 * ## Constraints and lifecycle
 * 正常构造后仅改变 layoutId，不破坏池所有权。先检查 VerifyModule 为 FALSE，再断言 INVALID_RANGE、同一 functionToken、entry block、instructionId1、sourceId701、expectedVersion1 与 actualVersion==index，最后释放夹具。
 * 在固定构造及前置完整验证成功的条件下，源码静态路径是 VerifyModule 调用补充校验后在 layoutId >= constantCount 分支拒绝；此推导不等于本提案观察到运行期分支。该助手不测试任意 index 都应失败，不能以其名字外推零池规则或 instructionCount 的极值。
 * 诊断记录独立于输入存储；专用场景没有 NULL 诊断或诊断别名实验，也未断言其两个哈希字段。
 *
 * ## References
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:35、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:40：两个真实直接调用。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:18、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:19、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:20、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:22、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:29、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:31：构造、突变、FALSE、诊断及释放。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:702、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:706、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:711：前置验证顺序。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:43、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:44、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:11、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:18：实际有意拒绝和诊断重建路径。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:14、tests/parser/test_ssa_module_constant_pool_verifier.c:17：检查失败的程序退出语义。
 */

/**
 * @fn void test_module_constant_pool_rejects_count_boundary_index(void)
 *
 * ## Summary
 * 通过共享索引助手确认 layoutId 等于固定 constantCount1 的排他上界时被拒绝。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 各项 expect_true 条件成立后正常返回，无 C 返回值；任一条件失败时 expect_true 调用 exit(EXIT_FAILURE) 结束整个测试程序，跳过后续场景及尚未执行的本地尾部释放。这里不使用 Unity longjmp、setUp 或 tearDown。
 *
 * ## Constraints and lifecycle
 * 向索引助手传入 1；FALSE 和完整 INVALID_RANGE 诊断由该助手断言，预期与实际索引字段均为 1。该场景本身不重复创建另一套断言，也不测试大 count 或任意无效池指针。固定构造成功时的有意首拒绝位置由源码静态推导为 layoutId >= constantCount。
 * 本场景只使用局部夹具、诊断和固定构造，诊断槽与输入记录及池分离。正常路径调用 free_constant_module；不把意外断言失败后的进程退出等同于完成显式资源清理。此文档描述已有源码意图和有限断言，本提案未运行测试。
 *
 * ## References
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:101。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:35。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:36。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:20。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:22。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:29。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:31。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:43。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:44。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:14、tests/parser/test_ssa_module_constant_pool_verifier.c:17：共用检查失败语义。
 */

/**
 * @fn void test_module_constant_pool_rejects_uint32_max_index(void)
 *
 * ## Summary
 * 通过共享索引助手确认 count1 模块不能接受 UINT32_MAX 的 CONSTANT.layoutId。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 各项 expect_true 条件成立后正常返回，无 C 返回值；任一条件失败时 expect_true 调用 exit(EXIT_FAILURE) 结束整个测试程序，跳过后续场景及尚未执行的本地尾部释放。这里不使用 Unity longjmp、setUp 或 tearDown。
 *
 * ## Constraints and lifecycle
 * 传入 UINT32_MAX，检查 FALSE 和同一组 INVALID_RANGE 位置诊断，其中 actualVersion 精确等于该索引。固定构造及前置验证成功时，静态拒绝在 layoutId 上界检查，早于常量池项读取；专用断言不独立观测内存访问，也不是 instructionCount、blockCount 或 allocation size 达 UINT32_MAX 的测试。
 * 本场景只使用局部夹具、诊断和固定构造，诊断槽与输入记录及池分离。正常路径调用 free_constant_module；不把意外断言失败后的进程退出等同于完成显式资源清理。此文档描述已有源码意图和有限断言，本提案未运行测试。
 *
 * ## References
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:102。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:40。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:41。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:20。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:22。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:29。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:43。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:44。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:51。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:14、tests/parser/test_ssa_module_constant_pool_verifier.c:17：共用检查失败语义。
 */

/**
 * @fn void test_module_constant_pool_rejects_constant_type_mismatch(void)
 *
 * ## Summary
 * 保持结果为 INT64，把模块池项 typeToken 改为 BOOL，确认拒绝并保留具体类型与位置诊断。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 各项 expect_true 条件成立后正常返回，无 C 返回值；任一条件失败时 expect_true 调用 exit(EXIT_FAILURE) 结束整个测试程序，跳过后续场景及尚未执行的本地尾部释放。这里不使用 Unity longjmp、setUp 或 tearDown。
 *
 * ## Constraints and lifecycle
 * layoutId0 仍有效，显式指令 typeToken 仍为 0。检查 FALSE、INVALID_VALUE、相同函数 token、entry block、instruction1、source701，以及 expected INT64/actual BOOL；静态有意首拒绝在池项与结果 typeToken 比较。只用这两个固定 token 作身份不等例，不验证 flags/bits、所有 token 域或后续显式指令类型比较。
 * 本场景只使用局部夹具、诊断和固定构造，诊断槽与输入记录及池分离。正常路径调用 free_constant_module；不把意外断言失败后的进程退出等同于完成显式资源清理。此文档描述已有源码意图和有限断言，本提案未运行测试。
 *
 * ## References
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:103。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:48。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:49。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:51。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:53。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:59。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:61。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:64。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:52。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:53。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:14、tests/parser/test_ssa_module_constant_pool_verifier.c:17：共用检查失败语义。
 */

/**
 * @fn void test_module_without_owned_pool_accepts_external_oracle_pool(void)
 *
 * ## Summary
 * 确认模块无自有常量池时仍能通过验证，并由另行配置的 Oracle 常量数组得到 SIGNED73 返回值。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 各项 expect_true 条件成立后正常返回，无 C 返回值；任一条件失败时 expect_true 调用 exit(EXIT_FAILURE) 结束整个测试程序，跳过后续场景及尚未执行的本地尾部释放。这里不使用 Unity longjmp、setUp 或 tearDown。
 *
 * ## Constraints and lifecycle
 * 固定模块保持 constantCount0、CONSTANT.layoutId0；用局部 externalConstant 的 SIGNED73 配置 input.constants/count1，input.function 借用夹具函数。实际断言空模块池与 VerifyModule成功，继而 RunOracleEx成功、returned、返回 kind 为 SIGNED 且值为73。
 * execution 先经 OracleResultInit 初始化；正常路径先 OracleResultFree，再 free_constant_module。externalConstant 与 input 是栈上借用对象，在同步 Oracle 调用期间保持有效，不转移给模块；不能单独 free 它们，也不延长其作用域。这里不是从 module.constants 读取 bits42 的执行断言。
 * 任何检查意外失败会 exit，可能跳过尚未执行的 OracleResultFree 和模块 Free；下层 RunOracleEx 自身某些失败清理不代表本场景保证清理所有局部资源。
 * 本场景只使用局部夹具、诊断和固定构造，诊断槽与输入记录及池分离。正常路径调用 free_constant_module；不把意外断言失败后的进程退出等同于完成显式资源清理。此文档描述已有源码意图和有限断言，本提案未运行测试。
 *
 * ## References
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:106。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:74。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:76。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:77。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:79。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:80。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:81。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:82。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:84。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:85。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:87。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:90。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:93。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:94。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:34。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_run.c:9。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_run.c:16。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_run.c:166。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c:413。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:14、tests/parser/test_ssa_module_constant_pool_verifier.c:17：共用检查失败语义。
 */

/**
 * @fn void test_module_constant_pool_rejects_instruction_type_mismatch(void)
 *
 * ## Summary
 * 保持池项和结果同为 INT64，仅把显式指令 typeToken 改成 BOOL，确认类型错误及完整位置诊断。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 各项 expect_true 条件成立后正常返回，无 C 返回值；任一条件失败时 expect_true 调用 exit(EXIT_FAILURE) 结束整个测试程序，跳过后续场景及尚未执行的本地尾部释放。这里不使用 Unity longjmp、setUp 或 tearDown。
 *
 * ## Constraints and lifecycle
 * layoutId0 有效且池项与结果匹配，故固定构造与前置验证成功时，静态有意拒绝在非零 instruction.typeToken 与结果不等分支。一个组合检查断言 FALSE、INVALID_VALUE、functionToken、block1、instruction1、source701、expected INT64/actual BOOL；不宣称此用例覆盖非零且匹配的显式类型或零模块池时的类型限制。
 * 本场景只使用局部夹具、诊断和固定构造，诊断槽与输入记录及池分离。正常路径调用 free_constant_module；不把意外断言失败后的进程退出等同于完成显式资源清理。此文档描述已有源码意图和有限断言，本提案未运行测试。
 *
 * ## References
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:104。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:100。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:101。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:102。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:103。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:104。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:105。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:106。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:107。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:108。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:110。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:52。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:57。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:59。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:14、tests/parser/test_ssa_module_constant_pool_verifier.c:17：共用检查失败语义。
 */

/**
 * @fn void test_function_only_verifier_does_not_require_an_owned_pool(void)
 *
 * ## Summary
 * 在空模块池中把 layoutId 设为 UINT32_MAX，区分函数完整验证与模块非空池索引验证的职责。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 各项 expect_true 条件成立后正常返回，无 C 返回值；任一条件失败时 expect_true 调用 exit(EXIT_FAILURE) 结束整个测试程序，跳过后续场景及尚未执行的本地尾部释放。这里不使用 Unity longjmp、setUp 或 tearDown。
 *
 * ## Constraints and lifecycle
 * 使用 hasModulePool=false 的固定夹具，仅改变 layoutId。一个短路组合检查先要求 VerifyFunction(ALL) 为TRUE，再要求 VerifyModule 为TRUE；若第一项失败，第二次调用不会执行。模块补充校验的静态路径在 constantCount0 时直接成功，因此接受不证明 UINT32_MAX 对任意外部池有效。
 * 本场景不调用 Oracle，不检查诊断字段，也不表示非空模块池接受该索引。若前置分析分配或其他验证失败，检查会结束程序，不能以场景名推定所有输入都已运行到零池快速路径。
 * 本场景只使用局部夹具、诊断和固定构造，诊断槽与输入记录及池分离。正常路径调用 free_constant_module；不把意外断言失败后的进程退出等同于完成显式资源清理。此文档描述已有源码意图和有限断言，本提案未运行测试。
 *
 * ## References
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:105。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:116。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:117。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:118。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:119。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:120。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:122。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:719。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:711。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:34。
 * - tests/parser/test_ssa_module_constant_pool_verifier.c:14、tests/parser/test_ssa_module_constant_pool_verifier.c:17：共用检查失败语义。
 */
