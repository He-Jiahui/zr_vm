#include "zr_vm_parser/exec_ir_builder.h"

static void append_invoke_memory_instruction(SZrExecIrFunction *function,
                                             EZrExecIrOpcode opcode,
                                             TZrExecIrValueId operand,
                                             TZrExecIrValueId result,
                                             TZrUInt32 source) {
    SZrExecIrInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.sourceId = source;
    ok(ZrCore_ExecIr_FunctionAppendOperands(function, &operand, 1u,
                                             &instruction.operands),
       "invoke memory operand");
    if (result != ZR_EXEC_IR_VALUE_ID_INVALID) {
        ok(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u,
                                                &instruction.results),
           "invoke memory result");
    }
    if (opcode == ZR_EXEC_IR_OPCODE_INVOKE)
        instruction.successorRange = function->blocks[0].successorRange;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "invoke memory instruction");
}

static SZrExecIrFunction *build_invoke_memory_successors(
        SZrExecIrModule *module) {
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(module, &id);
    TZrExecIrBlockId successors[2] = {2u, 3u};
    TZrExecIrBlockId entry = 1u;
    TZrExecIrValueId argument = ZrCore_ExecIr_FunctionAddExternalValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    TZrExecIrValueId results[3];
    TZrUInt32 index;
    ok(argument != ZR_EXEC_IR_VALUE_ID_INVALID, "invoke memory argument");
    for (index = 0u; index < 3u; ++index) {
        results[index] = ZrCore_ExecIr_FunctionAddValue(
                function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        ok(results[index] != ZR_EXEC_IR_VALUE_ID_INVALID, "invoke memory values");
    }
    ok(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u,
       "invoke memory normal block");
    ok(ZrCore_ExecIr_FunctionAddBlock(
               function, ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) == 3u,
       "invoke memory exception block");
    ok(ZrCore_ExecIr_FunctionAppendSuccessors(
               function, successors, 2u, &function->blocks[0].successorRange),
       "invoke memory successors");
    for (index = 1u; index < 3u; ++index) {
        ok(ZrCore_ExecIr_FunctionAppendPredecessors(
                   function, &entry, 1u, &function->blocks[index].predecessorRange),
           "invoke memory predecessor");
    }
    append_invoke_memory_instruction(function, ZR_EXEC_IR_OPCODE_INVOKE,
                                     argument, results[0], 1001u);
    append_invoke_memory_instruction(function, ZR_EXEC_IR_OPCODE_CALL,
                                     argument, results[1], 1002u);
    append_invoke_memory_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                                     results[1], 0u, 1003u);
    append_invoke_memory_instruction(function, ZR_EXEC_IR_OPCODE_CALL,
                                     argument, results[2], 1004u);
    append_invoke_memory_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                                     results[2], 0u, 1005u);
    function->blocks[0].instructions.start = 0u;
    function->blocks[0].instructions.count = 1u;
    function->blocks[0].terminatorInstructionId = 1u;
    function->blocks[1].instructions.start = 1u;
    function->blocks[1].instructions.count = 2u;
    function->blocks[1].terminatorInstructionId = 3u;
    function->blocks[2].instructions.start = 3u;
    function->blocks[2].instructions.count = 2u;
    function->blocks[2].terminatorInstructionId = 5u;
    return function;
}

static void assert_invoke_memory_diagnostic(
        const SZrExecIrDiagnostic *diagnostic,
        TZrUInt32 block, TZrUInt32 instruction, TZrUInt32 source,
        TZrUInt32 expected, TZrUInt32 actual) {
    ok(diagnostic->code == ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN &&
           diagnostic->functionToken == 1u && diagnostic->blockId == block &&
           diagnostic->instructionId == instruction && diagnostic->sourceId == source &&
           diagnostic->expectedVersion == expected && diagnostic->actualVersion == actual,
       "invoke memory diagnostic lost code, position, or versions");
}

static void test_invoke_memory_versions_on_both_successors(void) {
    SZrExecIrModule module;
    SZrExecIrDiagnostic diagnostic;
    const SZrExecIrOpcodeInfo *call = ZrCore_ExecIr_OpcodeInfo(ZR_EXEC_IR_OPCODE_CALL);
    const SZrExecIrOpcodeInfo *invoke = ZrCore_ExecIr_OpcodeInfo(ZR_EXEC_IR_OPCODE_INVOKE);
    SZrExecIrFunction *function;
    SZrExecIrFunction invokeOnly;
    SZrExecIrBlock entryOnly;
    SZrExecIrRange savedRange;
    TZrUInt32 successor;
    TZrUInt32 regionIndex;
    const EZrExecIrMemoryClass regions[2] = {
        ZR_EXEC_IR_MEMORY_MANAGED_HEAP, ZR_EXEC_IR_MEMORY_NATIVE_FFI};

    ok(invoke != ZR_NULL && call != ZR_NULL && call->memoryReads != 0u &&
           invoke->memoryReads == call->memoryReads &&
           invoke->memoryWrites == call->memoryWrites,
       "invoke schema omitted ordinary call memory effects");
    function = build_invoke_memory_successors(&module);
    ok(ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
       "invoke successor memory synthesis");
    ok(ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
       "invoke successor synthesized memory rejected");
    ok(function->instructions[0].memoryIn.count == 2u &&
           function->instructions[0].memoryOut.count == 2u,
       "invoke synthesis omitted heap/native ranges");

    for (successor = 0u; successor < 2u; ++successor) {
        TZrUInt32 instructionIndex = successor == 0u ? 1u : 3u;
        for (regionIndex = 0u; regionIndex < 2u; ++regionIndex) {
            TZrExecIrMemoryTokenId before = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                    regions[regionIndex], 1u);
            TZrExecIrMemoryTokenId after = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                    regions[regionIndex], 2u);
            TZrUInt32 inputIndex = function->instructions[instructionIndex].memoryIn.start +
                                      regionIndex;
            ok(function->memoryTokenPool[function->instructions[0].memoryIn.start +
                                             regionIndex] == before &&
                   function->memoryTokenPool[function->instructions[0].memoryOut.start +
                                             regionIndex] == after &&
                   function->memoryTokenPool[inputIndex] == after,
               "invoke successor did not inherit heap/native output");
            function->memoryTokenPool[inputIndex] = before;
            ok(!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
               "invoke successor stale heap/native memory accepted");
            assert_invoke_memory_diagnostic(&diagnostic, successor + 2u,
                                             instructionIndex + 1u,
                                             1001u + instructionIndex, after, before);
            function->memoryTokenPool[inputIndex] = after;
        }
    }
    /* Isolate required-range checks from downstream CFG mismatch diagnostics. */
    invokeOnly = *function;
    entryOnly = function->blocks[0];
    entryOnly.successorRange.count = 0u;
    invokeOnly.blocks = &entryOnly;
    invokeOnly.blockCount = 1u;
    invokeOnly.blockCapacity = 1u;
    invokeOnly.instructionCount = 1u;
    savedRange = function->instructions[0].memoryIn;
    function->instructions[0].memoryIn.count = 0u;
    ok(!ZrCore_ExecIr_VerifyEffects(&invokeOnly, &diagnostic),
       "invoke missing memory input accepted");
    assert_invoke_memory_diagnostic(&diagnostic, 1u, 1u, 1001u, 1u, 0u);
    function->instructions[0].memoryIn = savedRange;
    savedRange = function->instructions[0].memoryOut;
    function->instructions[0].memoryOut.count = 0u;
    ok(!ZrCore_ExecIr_VerifyEffects(&invokeOnly, &diagnostic),
       "invoke missing memory output accepted");
    assert_invoke_memory_diagnostic(&diagnostic, 1u, 1u, 1001u, 1u, 0u);
    function->instructions[0].memoryOut = savedRange;
    ok(ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
       "invoke successor memory repair rejected");
    ZrCore_ExecIr_FreeModule(&module);
}

/**
 * @fn static void append_invoke_memory_instruction(SZrExecIrFunction *function, EZrExecIrOpcode opcode, TZrExecIrValueId operand, TZrExecIrValueId result, TZrUInt32 source)
 *
 * ### Summary
 * 为INVOKE后继memory测试追加一个operand、可选result及带source的指令骨架。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Parameters
 * @param[in,out] function 非空可写fixture函数；INVOKE时blocks[0]及其successorRange已就绪。
 * @param[in] opcode 写入opcode；仅INVOKE复制首块successorRange。
 * @param[in] operand 始终追加的一项operand值ID。
 * @param[in] result 非INVALID时追加一项result，否则结果范围保持零。
 * @param[in] source 写入sourceId的位置标记。
 *
 * ### Returns
 * 正常返回表示适用的追加均通过；失败直接退出进程。
 *
 * ### Constraints and lifecycle
 * function为当前测试独占、非空可写函数，池保持有效且不并发修改。传入标量按值复制，临时instruction、token和ID数组通过Append接口复制进function池，不把其栈地址保存到最终函数。追加会重分配池，旧池元素指针/切片不得跨追加保留。任何ok失败直接exit(EXIT_FAILURE)，不回滚此前追加；正常返回后资源仍归函数所属module，最终由FreeModule释放。 不设置memory/effect/动态flags，后续SynthesizeCfgEffects负责生成；不把骨架当完整效果验证成功证明。INVOKE的successor range为池索引值拷贝，仍借用function池。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:3（definition）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:57（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:59（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:61（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:63（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:65（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:9（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:12（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:16（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:22（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:8（assertion failure exits process）。
 */

/**
 * @fn static SZrExecIrFunction *build_invoke_memory_successors( SZrExecIrModule *module)
 *
 * ### Summary
 * 创建entry INVOKE、正常CALL/RETURN和异常CALL/RETURN的三块测试骨架。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Parameters
 * @param[out] module 非空独立可写、未持有资源或已Free的module，接收新函数及全部池所有权。
 *
 * ### Returns
 * @return 正常返回module所拥有的非空函数借用；失败由ok退出。
 *
 * ### Constraints and lifecycle
 * 调用new_function后添加一个external argument、三个result及id2正常/id3异常块，首块successor按2/3，后继各有前驱1；五条指令source1001..1005，设置三块指令范围和terminator ID，但效果token/flags留待SynthesizeCfgEffects。module借用与释放同new_function；不能对返回函数或其浅拷贝分别Free。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:26（definition）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:108（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:29（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:32（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:37（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:39（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:42（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:44（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:46（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:49（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:53（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:57（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:59（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:61（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:63（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:65（call or function-like macro）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:229（module allocation ownership and teardown）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:233（module allocation ownership and teardown）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module allocation ownership and teardown）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:243（module allocation ownership and teardown）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:246（module allocation ownership and teardown）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:250（module allocation ownership and teardown）。
 * - tests/parser/test_ssa_effects_verifier.c:8（assertion failure exits process）。
 */

/**
 * @fn static void assert_invoke_memory_diagnostic( const SZrExecIrDiagnostic *diagnostic, TZrUInt32 block, TZrUInt32 instruction, TZrUInt32 source, TZrUInt32 expected, TZrUInt32 actual)
 *
 * ### Summary
 * 检查INVOKE memory失败诊断中指定的代码、函数及位置/版本字段。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Parameters
 * @param[in] diagnostic 非空、已由对应失败验证调用写出的可读诊断借用。
 * @param[in] block 期望blockId。
 * @param[in] instruction 期望instructionId。
 * @param[in] source 期望sourceId。
 * @param[in] expected 期望expectedVersion。
 * @param[in] actual 期望actualVersion。
 *
 * ### Returns
 * 所有列出字段匹配时返回；任一不匹配调用ok并结束进程。
 *
 * ### Constraints and lifecycle
 * 只读diagnostic且不保存借用；code固定MEMORY_TOKEN、functionToken固定1，&&从左到右短路，不读取或检查其余诊断字段。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:79（definition）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:135（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:153（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:159（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:83（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:8（assertion failure exits process）。
 */

/**
 * @fn static void test_invoke_memory_versions_on_both_successors(void)
 *
 * ### Summary
 * 检查INVOKE与CALL的memory schema一致，并在正常/异常两个后继传播heap/native版本。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：先以短路&&检查CALL/INVOKE schema存在、CALL reads非零且reads/writes相等；SynthesizeCfgEffects及VERIFY_ALL要求成功，INVOKE input/output各2项。对两后继×两区域分别把after2改before1，要求VERIFY_ALL拒绝并逐字段检查MEMORY_TOKEN诊断，再恢复。浅拷贝invokeOnly/entryOnly仅裁出首指令，分别把原共享instruction输入/输出count置0要求拒绝及expected1/actual0，恢复后要求整函数VERIFY_ALL成功。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:90（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1490（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:104（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:109（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:111（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:113（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:126（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:133（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:151（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:157（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:161（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:163（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 * - tests/parser/test_ssa_effects_verifier.c:1487（same-TU text include）。
 */
