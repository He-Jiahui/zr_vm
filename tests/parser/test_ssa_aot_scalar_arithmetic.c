#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "backend_aot_ir_scalar_text.h"

typedef struct ScalarFixture {
    SZrAotIrModule module;
    SZrAotIrFunction function;
    SZrAotIrBlock blocks[2];
    SZrAotIrInstruction instructions[5];
    SZrExecIrConstant constants[2];
    TZrUInt32 results[3];
    TZrUInt32 operands[3];
    TZrUInt32 successors[2];
} ScalarFixture;

static void contract(SZrExecutionContract *value, TZrMetadataToken token,
                     TZrUInt64 signature, TZrUInt64 layout) {
    memset(value, 0, sizeof(*value));
    value->schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    value->abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    value->logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    value->generation = 1u;
    value->targetToken = token;
    value->moduleHash = 33u;
    value->signatureHash = signature;
    value->layoutHash = layout;
}

static void fixture(ScalarFixture *f, TZrUInt32 opcode,
                    int64_t left, int64_t right, int branch) {
    const unsigned first = branch ? 1u : 0u;
    unsigned i;
    memset(f, 0, sizeof(*f));
    f->results[0] = 11u;
    f->results[1] = 29u;
    f->results[2] = 47u;
    f->operands[0] = 11u;
    f->operands[1] = 29u;
    f->operands[2] = 47u;
    f->constants[0].typeToken = 17u;
    f->constants[0].bits = (TZrUInt64)right;
    f->constants[1].typeToken = 17u;
    f->constants[1].bits = (TZrUInt64)left;
    for (i = 0u; i < 4u; ++i) {
        f->instructions[first + i].id = 100u + i;
        f->instructions[first + i].typeToken = 17u;
    }
    f->instructions[first].opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    f->instructions[first].layoutId = 1u;
    f->instructions[first].results.count = 1u;
    f->instructions[first + 1u].opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    f->instructions[first + 1u].results.offset = 1u;
    f->instructions[first + 1u].results.count = 1u;
    f->instructions[first + 2u].opcode = opcode;
    f->instructions[first + 2u].results.offset = 2u;
    f->instructions[first + 2u].results.count = 1u;
    f->instructions[first + 2u].operands.count = 2u;
    f->instructions[first + 3u].opcode = ZR_EXEC_IR_OPCODE_RETURN;
    f->instructions[first + 3u].operands.offset = 2u;
    f->instructions[first + 3u].operands.count = 1u;
    f->blocks[0].id = 3u;
    f->blocks[0].flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    f->blocks[0].instructions.count = 4u;
    f->blocks[0].terminatorInstructionId = 103u;
    if (branch) {
        f->instructions[0].id = 99u;
        f->instructions[0].opcode = ZR_EXEC_IR_OPCODE_BRANCH;
        f->instructions[0].successors.count = 1u;
        f->blocks[0].instructions.count = 1u;
        f->blocks[0].terminatorInstructionId = 99u;
        f->blocks[0].successors.count = 1u;
        f->blocks[1].id = 8u;
        f->blocks[1].instructions.offset = 1u;
        f->blocks[1].instructions.count = 4u;
        f->blocks[1].terminatorInstructionId = 103u;
        f->blocks[1].predecessors.offset = 1u;
        f->blocks[1].predecessors.count = 1u;
        f->successors[0] = 8u;
        f->successors[1] = 3u;
        f->function.successorPool = f->successors;
        f->function.successorCount = 2u;
    }
    f->function.id = 1u;
    f->function.functionToken = 7u;
    f->function.signatureHash = 11u;
    f->function.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64;
    f->function.callableAbi.returnTypeToken = 17u;
    f->function.frameLayout.frameByteSize = 16u;
    f->function.frameLayout.frameByteAlign = 8u;
    f->function.frameLayout.layoutHash = 22u;
    f->function.blocks = f->blocks;
    f->function.blockCount = branch ? 2u : 1u;
    f->function.instructions = f->instructions;
    f->function.instructionCount = first + 4u;
    f->function.resultPool = f->results;
    f->function.resultCount = 3u;
    f->function.operandPool = f->operands;
    f->function.operandCount = 3u;
    contract(&f->function.contract, 7u, 11u, 22u);
    f->module.schemaVersion = ZR_AOT_IR_SCHEMA_VERSION;
    f->module.moduleHash = 33u;
    f->module.functions = &f->function;
    f->module.functionCount = 1u;
    f->module.constantPool = f->constants;
    f->module.constantCount = 2u;
    f->module.target.abiVersion = ZR_AOT_IR_TARGET_ABI_VERSION;
    f->module.target.pointerSize = (TZrUInt32)sizeof(void *);
    f->module.target.targetTripleHash = 67u;
    f->module.target.abiHash = 66u;
    contract(&f->module.contract, 0u, 44u, 55u);
}

static void write_artifact(const char *directory, const char *name,
                           const char *extension, const char *text) {
    char path[1024];
    FILE *file;
    int count = snprintf(path, sizeof(path), "%s/%s.%s", directory, name, extension);
    assert(count > 0 && (size_t)count < sizeof(path));
    file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(text, 1u, strlen(text), file) == strlen(text));
    assert(fclose(file) == 0);
}

static int positive(const char *name, TZrUInt32 opcode, int64_t left,
                    int64_t right, int64_t expected, int branch,
                    const char *directory) {
    ScalarFixture f;
    SZrAotIrDiagnostic diagnostic;
    char c[1024], llvm[1024], runner[512];
    size_t cLength = 0u, llvmLength = 0u;
    EZrAotIrStatus cStatus, llvmStatus;
    fixture(&f, opcode, left, right, branch);
    assert(ZrCore_AotIr_ValidateModule(&f.module, &diagnostic) == ZR_AOT_IR_OK);
    cStatus = backend_aot_ir_c_emit_const_i64(&f.module, 1u, c, sizeof(c), &cLength, &diagnostic);
    llvmStatus = backend_aot_ir_llvm_emit_const_i64(&f.module, 1u, llvm, sizeof(llvm), &llvmLength, &diagnostic);
    if (cStatus != ZR_AOT_IR_OK || llvmStatus != ZR_AOT_IR_OK) {
        fprintf(stderr, "RED %s: valid arithmetic module, C status=%u LLVM status=%u\n",
                name, (unsigned)cStatus, (unsigned)llvmStatus);
        return 1;
    }
    assert(cLength == strlen(c) && llvmLength == strlen(llvm));
    assert(strstr(c, opcode == ZR_EXEC_IR_OPCODE_ADD ? " + " : " - ") != NULL);
    assert(strstr(llvm, opcode == ZR_EXEC_IR_OPCODE_ADD ? "add i64 " : "sub i64 ") != NULL);
    assert(strstr(llvm, "ret i64 %") != NULL);
    assert(strstr(llvm, "nsw") == NULL && strstr(llvm, "nuw") == NULL);
    if (directory != NULL) {
        write_artifact(directory, name, "c", c);
        write_artifact(directory, name, "ll", llvm);
        snprintf(runner, sizeof(runner), "#include <stdint.h>\nextern int64_t zr_aot_scalar_fn_1(void);\nint main(void) { return (uint64_t)zr_aot_scalar_fn_1() == UINT64_C(%llu) ? 0 : 1; }\n", (unsigned long long)(uint64_t)expected);
        write_artifact(directory, name, "runner.c", runner);
    }
    /* The exact source length plus NUL succeeds on the reused buffer. */
    assert(backend_aot_ir_c_emit_const_i64(&f.module, 1u, c, cLength + 1u, &cLength, &diagnostic) == ZR_AOT_IR_OK);
    assert(backend_aot_ir_llvm_emit_const_i64(&f.module, 1u, llvm, llvmLength + 1u, &llvmLength, &diagnostic) == ZR_AOT_IR_OK);
    /* Capacity exactly equal to non-NUL length must fail and clear reused output. */
    assert(backend_aot_ir_c_emit_const_i64(&f.module, 1u, c, cLength, &cLength, &diagnostic) == ZR_AOT_IR_INVALID_RANGE);
    assert(c[0] == '\0' && cLength == 0u);
    assert(backend_aot_ir_llvm_emit_const_i64(&f.module, 1u, llvm, llvmLength, &llvmLength, &diagnostic) == ZR_AOT_IR_INVALID_RANGE);
    assert(llvm[0] == '\0' && llvmLength == 0u);
    printf("PASS %s\n", name);
    return 0;
}

static void overflow(TZrUInt32 opcode, int64_t left, int64_t right) {
    ScalarFixture f;
    SZrAotIrDiagnostic diagnostic;
    char text[1024] = "stale";
    size_t length = 99u;
    fixture(&f, opcode, left, right, 0);
    assert(ZrCore_AotIr_ValidateModule(&f.module, &diagnostic) == ZR_AOT_IR_OK);
    assert(backend_aot_ir_c_emit_const_i64(&f.module, 1u, text, sizeof(text), &length, &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(text[0] == '\0' && length == 0u);
    assert(diagnostic.functionId == 1u && diagnostic.instructionId == 102u);
    memcpy(text, "stale", 6u);
    length = 99u;
    assert(backend_aot_ir_llvm_emit_const_i64(&f.module, 1u, text, sizeof(text), &length, &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(text[0] == '\0' && length == 0u);
    assert(diagnostic.functionId == 1u && diagnostic.instructionId == 102u);
}

static void rejected(const ScalarFixture *f) {
    SZrAotIrDiagnostic diagnostic;
    char text[1024] = "stale";
    size_t length = 99u;
    assert(backend_aot_ir_c_emit_const_i64(&f->module, 1u, text, sizeof(text), &length, &diagnostic) != ZR_AOT_IR_OK);
    assert(text[0] == '\0' && length == 0u);
    memcpy(text, "stale", 6u);
    length = 99u;
    assert(backend_aot_ir_llvm_emit_const_i64(&f->module, 1u, text, sizeof(text), &length, &diagnostic) != ZR_AOT_IR_OK);
    assert(text[0] == '\0' && length == 0u);
}

static void malformed(void) {
    ScalarFixture f;
    fixture(&f, ZR_EXEC_IR_OPCODE_ADD, 19, 23, 0);
    f.constants[1].flags = 1u;
    rejected(&f);
    fixture(&f, ZR_EXEC_IR_OPCODE_ADD, 19, 23, 0);
    f.constants[1].typeToken = 18u;
    rejected(&f);
    fixture(&f, ZR_EXEC_IR_OPCODE_ADD, 19, 23, 0);
    f.instructions[2].typeToken = 18u;
    rejected(&f);
    fixture(&f, ZR_EXEC_IR_OPCODE_ADD, 19, 23, 0);
    f.operands[0] = 47u; /* Result cannot replace the earlier operand. */
    rejected(&f);
    fixture(&f, ZR_EXEC_IR_OPCODE_ADD, 19, 23, 0);
    f.operands[2] = 11u; /* Do not silently return a different defined value. */
    rejected(&f);
    fixture(&f, ZR_EXEC_IR_OPCODE_ADD, 19, 23, 0);
    f.results[2] = 11u;
    rejected(&f);
    fixture(&f, ZR_EXEC_IR_OPCODE_ADD, 19, 23, 0);
    f.instructions[2].flags = 1u;
    rejected(&f);
    fixture(&f, ZR_EXEC_IR_OPCODE_ADD, 19, 23, 0);
    f.instructions[2].matchTypeToken = 17u;
    rejected(&f);
    fixture(&f, ZR_EXEC_IR_OPCODE_ADD, 19, 23, 0);
    f.function.contract.declaredEffects = ZR_EXECUTION_EFFECT_THROW;
    rejected(&f);
}

int main(int argc, char **argv) {
    const char *directory;
    int failures = 0;
    assert(argc == 1 || argc == 2);
    directory = argc == 2 ? argv[1] : NULL;
#define POS(name, op, a, b, result, branch) failures += positive(name, op, a, b, result, branch, directory)
    POS("add_positive", ZR_EXEC_IR_OPCODE_ADD, 19, 23, 42, 0);
    POS("sub_order", ZR_EXEC_IR_OPCODE_SUB, 19, 23, -4, 0);
    POS("add_negative", ZR_EXEC_IR_OPCODE_ADD, -19, -23, -42, 0);
    POS("sub_negative", ZR_EXEC_IR_OPCODE_SUB, -19, -23, 4, 0);
    POS("add_mixed", ZR_EXEC_IR_OPCODE_ADD, -19, 23, 4, 0);
    POS("sub_mixed", ZR_EXEC_IR_OPCODE_SUB, 19, -23, 42, 0);
    POS("max_zero", ZR_EXEC_IR_OPCODE_ADD, INT64_MAX, 0, INT64_MAX, 0);
    POS("min_zero", ZR_EXEC_IR_OPCODE_SUB, INT64_MIN, 0, INT64_MIN, 0);
    POS("min_add_one", ZR_EXEC_IR_OPCODE_ADD, INT64_MIN, 1, INT64_MIN + 1, 0);
    POS("max_sub_one", ZR_EXEC_IR_OPCODE_SUB, INT64_MAX, 1, INT64_MAX - 1, 0);
    POS("branch_add", ZR_EXEC_IR_OPCODE_ADD, 19, 23, 42, 1);
    POS("branch_sub", ZR_EXEC_IR_OPCODE_SUB, 19, 23, -4, 1);
#undef POS
    if (failures != 0) return 1;
    overflow(ZR_EXEC_IR_OPCODE_ADD, INT64_MAX, 1);
    overflow(ZR_EXEC_IR_OPCODE_ADD, INT64_MIN, -1);
    overflow(ZR_EXEC_IR_OPCODE_SUB, INT64_MIN, 1);
    overflow(ZR_EXEC_IR_OPCODE_SUB, INT64_MAX, -1);
    malformed();
    printf("RESULT positives=12 overflow=4 rejected=9 failures=0\n");
    return 0;
}
