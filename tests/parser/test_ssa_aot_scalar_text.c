#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "backend_aot_ir_scalar_text.h"

static void fill_contract(SZrExecutionContract *contract,
                          TZrMetadataToken token, TZrUInt64 moduleHash,
                          TZrUInt64 signatureHash, TZrUInt64 layoutHash) {
    memset(contract, 0, sizeof(*contract));
    contract->schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    contract->abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    contract->logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    contract->generation = 1u;
    contract->targetToken = token;
    contract->moduleHash = moduleHash;
    contract->signatureHash = signatureHash;
    contract->layoutHash = layoutHash;
}

static SZrAotIrModule make_module(SZrAotIrFunction *function,
                                 SZrAotIrBlock *block,
                                 SZrAotIrInstruction *instructions,
                                 SZrExecIrConstant *constant,
                                 const TZrUInt32 *valueId) {
    SZrAotIrModule module;
    memset(function, 0, sizeof(*function));
    memset(block, 0, sizeof(*block));
    memset(instructions, 0, sizeof(*instructions) * 3u);
    memset(&module, 0, sizeof(module));
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instructions[0].results.count = 1u;
    instructions[0].typeToken = 17u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instructions[1].operands.count = 1u;
    instructions[1].typeToken = 17u;
    block->id = 1u;
    block->flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    block->instructions.count = 2u;
    block->terminatorInstructionId = 2u;
    function->id = 1u;
    function->functionToken = 7u;
    function->signatureHash = 11u;
    function->callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64;
    function->callableAbi.returnTypeToken = 17u;
    function->frameLayout.frameByteSize = 16u;
    function->frameLayout.frameByteAlign = 8u;
    function->frameLayout.layoutHash = 22u;
    function->blocks = block;
    function->blockCount = 1u;
    function->instructions = instructions;
    function->instructionCount = 2u;
    function->operandPool = valueId;
    function->operandCount = 1u;
    function->resultPool = valueId;
    function->resultCount = 1u;
    fill_contract(&function->contract, 7u, 33u, 11u, 22u);
    constant->typeToken = 17u;
    constant->flags = 0u;
    constant->bits = UINT64_C(42);
    module.schemaVersion = ZR_AOT_IR_SCHEMA_VERSION;
    module.moduleHash = 33u;
    module.functions = function;
    module.functionCount = 1u;
    module.constantPool = constant;
    module.constantCount = 1u;
    module.target.abiVersion = ZR_AOT_IR_TARGET_ABI_VERSION;
    module.target.pointerSize = (TZrUInt32)sizeof(void *);
    module.target.targetTripleHash = 67u;
    module.target.abiHash = 66u;
    fill_contract(&module.contract, 0u, 33u, 44u, 55u);
    return module;
}

static void write_file(const char *directory, const char *name,
                       const char *content, size_t length) {
    char path[512];
    FILE *file;
    int count = snprintf(path, sizeof(path), "%s/%s", directory, name);
    assert(count > 0 && (size_t)count < sizeof(path));
    file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(content, 1u, length, file) == length);
    assert(fclose(file) == 0);
}

static void test_branch_const_i64(const char *directory) {
    const TZrUInt32 valueId = 1u;
    TZrUInt32 successors[3] = {2u, 1u, 1u};
    SZrExecIrConstant constant;
    SZrAotIrInstruction instructions[4] = {0};
    SZrAotIrInstruction savedReturn;
    SZrAotIrBlock blocks[2] = {0};
    SZrAotIrFunction function;
    SZrAotIrModule module = make_module(&function, blocks, instructions,
                                       &constant, &valueId);
    SZrAotIrCallableAbi abi;
    SZrAotIrDiagnostic diagnostic;
    char cText[256];
    char llvmText[256];
    size_t cLength = 0u;
    size_t llvmLength = 0u;

    instructions[2] = instructions[1];
    instructions[2].id = 3u;
    memset(&instructions[0], 0, sizeof(instructions[0]));
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_EXEC_IR_OPCODE_BRANCH;
    instructions[0].successors.count = 1u;
    memset(&instructions[1], 0, sizeof(instructions[1]));
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instructions[1].results.count = 1u;
    instructions[1].typeToken = 17u;
    blocks[0].instructions.count = 1u;
    blocks[0].successors.count = 1u;
    blocks[0].terminatorInstructionId = 1u;
    blocks[1].id = 2u;
    blocks[1].instructions.offset = 1u;
    blocks[1].instructions.count = 2u;
    blocks[1].predecessors.offset = 1u;
    blocks[1].predecessors.count = 1u;
    blocks[1].terminatorInstructionId = 3u;
    function.blockCount = 2u;
    function.instructionCount = 3u;
    function.successorPool = successors;
    function.successorCount = 2u;

    assert(ZrCore_AotIr_ValidateModule(&module, &diagnostic) == ZR_AOT_IR_OK);
    assert(ZrCore_AotIr_RequireExecutableAbi(&module, 1u, &abi,
                                             &diagnostic) == ZR_AOT_IR_OK);
    function.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_UNKNOWN;
    function.callableAbi.returnTypeToken = 0u;
    cText[0] = 'x';
    cLength = 99u;
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(cText[0] == '\0' && cLength == 0u);
    function.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64;
    function.callableAbi.returnTypeToken = 17u;

    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_OK);
    assert(strstr(cText, "goto zr_aot_block_2;") != NULL);
    assert(strstr(cText, "zr_aot_block_2:") != NULL);
    assert(strstr(cText, "return INT64_C(42);") != NULL);
    assert(cLength == strlen(cText));
    assert(backend_aot_ir_llvm_emit_const_i64(&module, 1u, llvmText,
                                              sizeof(llvmText), &llvmLength,
                                              &diagnostic) == ZR_AOT_IR_OK);
    assert(strstr(llvmText, "br label %block_2") != NULL);
    assert(strstr(llvmText, "block_2:") != NULL);
    assert(strstr(llvmText, "ret i64 42") != NULL);
    assert(llvmLength == strlen(llvmText));
    if (directory != NULL) {
        write_file(directory, "branch_42.c", cText, cLength);
        write_file(directory, "branch_42.ll", llvmText, llvmLength);
    }

    constant.bits = UINT64_C(0x8000000000000000);
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_OK);
    assert(strstr(cText, "return INT64_MIN;") != NULL);
    assert(backend_aot_ir_llvm_emit_const_i64(&module, 1u, llvmText,
                                              sizeof(llvmText), &llvmLength,
                                              &diagnostic) == ZR_AOT_IR_OK);
    assert(strstr(llvmText, "ret i64 -9223372036854775808") != NULL);
    if (directory != NULL) {
        write_file(directory, "branch_min.c", cText, cLength);
        write_file(directory, "branch_min.ll", llvmText, llvmLength);
    }

    cText[0] = 'x';
    cLength = 99u;
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText, 8u,
                                           &cLength, &diagnostic) ==
           ZR_AOT_IR_INVALID_RANGE);
    assert(cText[0] == '\0' && cLength == 0u);
    llvmText[0] = 'x';
    llvmLength = 99u;
    assert(backend_aot_ir_llvm_emit_const_i64(&module, 1u, llvmText, 8u,
                                              &llvmLength, &diagnostic) ==
           ZR_AOT_IR_INVALID_RANGE);
    assert(llvmText[0] == '\0' && llvmLength == 0u);
    instructions[0].successors.offset = 1u;
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(cText[0] == '\0' && cLength == 0u);
    instructions[0].successors.offset = 0u;
    successors[1] = 2u;
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_INVALID_CFG);
    assert(cText[0] == '\0' && cLength == 0u);
    successors[1] = 1u;
    function.successorCount = 3u;
    assert(backend_aot_ir_llvm_emit_const_i64(&module, 1u, llvmText,
                                              sizeof(llvmText), &llvmLength,
                                              &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(llvmText[0] == '\0' && llvmLength == 0u);
    function.successorCount = 2u;
    blocks[0].terminatorInstructionId = 0u;
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(cText[0] == '\0' && cLength == 0u);
    blocks[0].terminatorInstructionId = 1u;
    function.contract.declaredEffects = ZR_EXECUTION_EFFECT_THROW;
    assert(backend_aot_ir_llvm_emit_const_i64(&module, 1u, llvmText,
                                              sizeof(llvmText), &llvmLength,
                                              &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(llvmText[0] == '\0' && llvmLength == 0u);
    function.contract.declaredEffects = 0u;
    savedReturn = instructions[2];
    instructions[3] = savedReturn;
    instructions[3].id = 4u;
    memset(&instructions[2], 0, sizeof(instructions[2]));
    instructions[2].id = 3u;
    instructions[2].opcode = ZR_EXEC_IR_OPCODE_NOP;
    blocks[1].instructions.count = 3u;
    blocks[1].terminatorInstructionId = 4u;
    function.instructionCount = 4u;
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(cText[0] == '\0' && cLength == 0u);
}

int main(int argc, char **argv) {
    const TZrUInt32 valueId = 1u;
    const TZrUInt32 extraValueIds[2] = {1u, 2u};
    SZrExecIrConstant constant;
    SZrAotIrInstruction instructions[3];
    SZrAotIrBlock block;
    SZrAotIrFunction function;
    SZrAotIrModule module = make_module(&function, &block, instructions,
                                       &constant, &valueId);
    SZrAotIrDiagnostic diagnostic;
    char cText[256];
    char llvmText[256];
    size_t cLength = 0u;
    size_t llvmLength = 0u;

    assert(argc == 1 || argc == 2);
    function.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_UNKNOWN;
    function.callableAbi.returnTypeToken = 0u;
    memcpy(cText, "stale", sizeof("stale"));
    cLength = 99u;
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(cText[0] == '\0' && cLength == 0u);
    assert(diagnostic.status == ZR_AOT_IR_UNSUPPORTED);
    assert(diagnostic.functionId == 1u);
    memcpy(llvmText, "stale", sizeof("stale"));
    llvmLength = 99u;
    assert(backend_aot_ir_llvm_emit_const_i64(&module, 1u, llvmText,
                                              sizeof(llvmText), &llvmLength,
                                              &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(llvmText[0] == '\0' && llvmLength == 0u);
    function.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64;
    function.callableAbi.returnTypeToken = 17u;

    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_OK);
    assert(strstr(cText, "int64_t zr_aot_scalar_fn_1(void)") != NULL);
    assert(strstr(cText, "return INT64_C(42);") != NULL);
    assert(cLength == strlen(cText));
    assert(backend_aot_ir_llvm_emit_const_i64(&module, 1u, llvmText,
                                              sizeof(llvmText), &llvmLength,
                                              &diagnostic) == ZR_AOT_IR_OK);
    assert(strstr(llvmText, "define i64 @zr_aot_scalar_fn_1()") != NULL);
    assert(strstr(llvmText, "ret i64 42") != NULL);
    assert(llvmLength == strlen(llvmText));
    if (argc == 2) {
        write_file(argv[1], "constant_42.c", cText, cLength);
        write_file(argv[1], "constant_42.ll", llvmText, llvmLength);
    }

    constant.bits = UINT64_C(0x8000000000000000);
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_OK);
    assert(strstr(cText, "return INT64_MIN;") != NULL);
    assert(backend_aot_ir_llvm_emit_const_i64(&module, 1u, llvmText,
                                              sizeof(llvmText), &llvmLength,
                                              &diagnostic) == ZR_AOT_IR_OK);
    assert(strstr(llvmText, "ret i64 -9223372036854775808") != NULL);
    if (argc == 2) {
        write_file(argv[1], "constant_min.c", cText, cLength);
        write_file(argv[1], "constant_min.ll", llvmText, llvmLength);
        write_file(argv[1], "runner.c",
                   "#include <stdint.h>\nextern int64_t zr_aot_scalar_fn_1(void);\n"
                   "int main(void) { return zr_aot_scalar_fn_1() == EXPECTED ? 0 : 1; }\n",
                   strlen("#include <stdint.h>\nextern int64_t zr_aot_scalar_fn_1(void);\n"
                          "int main(void) { return zr_aot_scalar_fn_1() == EXPECTED ? 0 : 1; }\n"));
    }

    constant.bits = UINT64_MAX;
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_OK);
    assert(strstr(cText, "return -INT64_C(1);") != NULL);
    assert(backend_aot_ir_llvm_emit_const_i64(&module, 1u, llvmText,
                                              sizeof(llvmText), &llvmLength,
                                              &diagnostic) == ZR_AOT_IR_OK);
    assert(strstr(llvmText, "ret i64 -1") != NULL);
    if (argc == 2) {
        write_file(argv[1], "constant_neg1.c", cText, cLength);
        write_file(argv[1], "constant_neg1.ll", llvmText, llvmLength);
    }

    cText[0] = 'x';
    cLength = 99u;
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText, 8u,
                                           &cLength, &diagnostic) ==
           ZR_AOT_IR_INVALID_RANGE);
    assert(cText[0] == '\0' && cLength == 0u);
    assert(diagnostic.status == ZR_AOT_IR_INVALID_RANGE);
    llvmText[0] = 'x';
    llvmLength = 99u;
    assert(backend_aot_ir_llvm_emit_const_i64(&module, 1u, llvmText, 8u,
                                              &llvmLength, &diagnostic) ==
           ZR_AOT_IR_INVALID_RANGE);
    assert(llvmText[0] == '\0' && llvmLength == 0u);
    cText[0] = 'x';
    cLength = 99u;
    assert(backend_aot_ir_c_emit_const_i64(NULL, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_INVALID_ARGUMENT);
    assert(cText[0] == '\0' && cLength == 0u);
    function.resultPool = extraValueIds;
    function.resultCount = 2u;
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(cText[0] == '\0' && cLength == 0u);
    function.resultPool = &valueId;
    function.resultCount = 1u;
    function.operandPool = extraValueIds;
    function.operandCount = 2u;
    assert(backend_aot_ir_llvm_emit_const_i64(&module, 1u, llvmText,
                                              sizeof(llvmText), &llvmLength,
                                              &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(llvmText[0] == '\0' && llvmLength == 0u);
    function.operandPool = &valueId;
    function.operandCount = 1u;
    block.terminatorInstructionId = 0u;
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(cText[0] == '\0' && cLength == 0u);
    block.terminatorInstructionId = instructions[1].id;
    block.flags |= ZR_EXEC_IR_BLOCK_FLAG_COLD;
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(cText[0] == '\0' && cLength == 0u);
    block.flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    constant.flags = 1u;
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(cText[0] == '\0' && cLength == 0u);
    assert(diagnostic.instructionId == instructions[0].id);
    constant.flags = 0u;
    function.contract.declaredEffects = ZR_EXECUTION_EFFECT_THROW;
    assert(backend_aot_ir_llvm_emit_const_i64(&module, 1u, llvmText,
                                              sizeof(llvmText), &llvmLength,
                                              &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(llvmText[0] == '\0' && llvmLength == 0u);
    function.contract.declaredEffects = 0u;
    instructions[2] = instructions[1];
    instructions[2].id = 3u;
    instructions[1].opcode = ZR_EXEC_IR_OPCODE_NOP;
    instructions[1].operands.count = 0u;
    function.instructionCount = 3u;
    block.instructions.count = 3u;
    block.terminatorInstructionId = 3u;
    assert(backend_aot_ir_c_emit_const_i64(&module, 1u, cText,
                                           sizeof(cText), &cLength,
                                           &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(cText[0] == '\0' && cLength == 0u);
    test_branch_const_i64(argc == 2 ? argv[1] : NULL);
    return 0;
}
