#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "backend_aot_ir_scalar_text.h"

typedef struct ConditionalFixture {
    SZrAotIrModule module;
    SZrAotIrFunction function;
    SZrAotIrBlock blocks[3];
    SZrAotIrInstruction instructions[8];
    SZrExecIrConstant constants[4];
    TZrUInt32 results[5];
    TZrUInt32 operands[5];
    TZrUInt32 edges[4];
} ConditionalFixture;

typedef struct ConditionalCase {
    const char *name;
    TZrUInt32 predicate;
    int64_t left;
    int64_t right;
    int reverseOperands;
    int reverseEdges;
    int64_t expected;
} ConditionalCase;

typedef struct ConditionalCounts {
    unsigned positives;
    unsigned preconditionFailures;
    unsigned featureFailures;
    unsigned outputFailures;
    unsigned guards;
    unsigned guardFailures;
    unsigned argumentChecks;
    unsigned argumentFailures;
} ConditionalCounts;

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

static void fixture(ConditionalFixture *f, const ConditionalCase *test) {
    unsigned i;
    memset(f, 0, sizeof(*f));
    f->results[0] = 11u;
    f->results[1] = 29u;
    f->results[2] = 47u;
    f->results[3] = 61u;
    f->results[4] = 83u;
    memcpy(f->operands, f->results, sizeof(f->operands));
    if (test->reverseOperands) {
        f->operands[0] = 29u;
        f->operands[1] = 11u;
    }
    f->constants[0].bits = (TZrUInt64)INT64_C(101);
    f->constants[1].bits = (TZrUInt64)test->right;
    f->constants[2].bits = (TZrUInt64)-INT64_C(202);
    f->constants[3].bits = (TZrUInt64)test->left;
    for (i = 0u; i < 4u; ++i) f->constants[i].typeToken = 17u;
    for (i = 0u; i < 8u; ++i) {
        f->instructions[i].id = 100u + i;
        f->instructions[i].typeToken = 17u;
    }
    f->instructions[0].opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    f->instructions[0].results.count = 1u;
    f->instructions[0].layoutId = 3u;
    f->instructions[1].opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    f->instructions[1].results.offset = 1u;
    f->instructions[1].results.count = 1u;
    f->instructions[1].layoutId = 1u;
    f->instructions[2].opcode = ZR_EXEC_IR_OPCODE_COMPARE;
    f->instructions[2].results.offset = 2u;
    f->instructions[2].results.count = 1u;
    f->instructions[2].operands.count = 2u;
    f->instructions[2].typeToken = test->predicate;
    f->instructions[3].opcode = ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH;
    f->instructions[3].typeToken = 0u;
    f->instructions[3].operands.offset = 2u;
    f->instructions[3].operands.count = 1u;
    f->instructions[3].successors.count = 2u;
    f->instructions[4].opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    f->instructions[4].results.offset = 3u;
    f->instructions[4].results.count = 1u;
    f->instructions[5].opcode = ZR_EXEC_IR_OPCODE_RETURN;
    f->instructions[5].operands.offset = 3u;
    f->instructions[5].operands.count = 1u;
    f->instructions[6].opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    f->instructions[6].results.offset = 4u;
    f->instructions[6].results.count = 1u;
    f->instructions[6].layoutId = 2u;
    f->instructions[7].opcode = ZR_EXEC_IR_OPCODE_RETURN;
    f->instructions[7].operands.offset = 4u;
    f->instructions[7].operands.count = 1u;
    f->blocks[0].id = 3u;
    f->blocks[0].flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    f->blocks[0].instructions.count = 4u;
    f->blocks[0].terminatorInstructionId = 103u;
    f->blocks[0].successors.count = 2u;
    f->blocks[1].id = 8u;
    f->blocks[1].instructions.offset = 4u;
    f->blocks[1].instructions.count = 2u;
    f->blocks[1].terminatorInstructionId = 105u;
    f->blocks[1].predecessors.offset = 2u;
    f->blocks[1].predecessors.count = 1u;
    f->blocks[2].id = 13u;
    f->blocks[2].instructions.offset = 6u;
    f->blocks[2].instructions.count = 2u;
    f->blocks[2].terminatorInstructionId = 107u;
    f->blocks[2].predecessors.offset = 3u;
    f->blocks[2].predecessors.count = 1u;
    f->edges[0] = test->reverseEdges ? 13u : 8u;
    f->edges[1] = test->reverseEdges ? 8u : 13u;
    f->edges[2] = 3u;
    f->edges[3] = 3u;
    f->function.id = 1u;
    f->function.functionToken = 7u;
    f->function.signatureHash = 11u;
    f->function.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64;
    f->function.callableAbi.returnTypeToken = 17u;
    f->function.frameLayout.frameByteSize = 16u;
    f->function.frameLayout.frameByteAlign = 8u;
    f->function.frameLayout.layoutHash = 22u;
    f->function.blocks = f->blocks;
    f->function.blockCount = 3u;
    f->function.instructions = f->instructions;
    f->function.instructionCount = 8u;
    f->function.resultPool = f->results;
    f->function.resultCount = 5u;
    f->function.operandPool = f->operands;
    f->function.operandCount = 5u;
    f->function.successorPool = f->edges;
    f->function.successorCount = 4u;
    contract(&f->function.contract, 7u, 11u, 22u);
    f->module.schemaVersion = ZR_AOT_IR_SCHEMA_VERSION;
    f->module.moduleHash = 33u;
    f->module.functions = &f->function;
    f->module.functionCount = 1u;
    f->module.constantPool = f->constants;
    f->module.constantCount = 4u;
    f->module.target.abiVersion = ZR_AOT_IR_TARGET_ABI_VERSION;
    f->module.target.pointerSize = (TZrUInt32)sizeof(void *);
    f->module.target.targetTripleHash = 67u;
    f->module.target.abiHash = 66u;
    contract(&f->module.contract, 0u, 44u, 55u);
}

static int write_artifact(const char *directory, const char *name,
                          const char *extension, const char *text) {
    char path[1024];
    FILE *file;
    size_t length = strlen(text);
    int written, closed;
    const int count = snprintf(path, sizeof(path), "%s/%s.%s", directory, name, extension);
    if (count <= 0 || (size_t)count >= sizeof(path)) return 0;
    file = fopen(path, "wb");
    if (file == NULL) return 0;
    written = fwrite(text, 1u, length, file) == length;
    closed = fclose(file) == 0;
    return written && closed;
}

static void positive(const ConditionalCase *test, const char *directory,
                     ConditionalCounts *counts) {
    ConditionalFixture f;
    SZrAotIrCallableAbi abi;
    SZrAotIrDiagnostic validationDiagnostic, abiDiagnostic, cDiagnostic, llvmDiagnostic;
    EZrAotIrStatus validationStatus, abiStatus, cStatus, llvmStatus;
    char c[2048] = "stale", llvm[2048] = "stale", runner[512];
    size_t cLength = 99u, llvmLength = 99u;
    int outputOk;
    ++counts->positives;
    fixture(&f, test);
    validationStatus = ZrCore_AotIr_ValidateModule(&f.module, &validationDiagnostic);
    abiStatus = ZrCore_AotIr_RequireExecutableAbi(&f.module, 1u, &abi, &abiDiagnostic);
    printf("PRECONDITION %s ValidateModule=%u RequireExecutableAbi=%u ABI=%u returnType=%u\n",
           test->name, (unsigned)validationStatus, (unsigned)abiStatus,
           (unsigned)abi.kind, (unsigned)abi.returnTypeToken);
    if (validationStatus != ZR_AOT_IR_OK || abiStatus != ZR_AOT_IR_OK ||
        abi.kind != ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64 || abi.returnTypeToken != 17u) {
        ++counts->preconditionFailures;
        fprintf(stderr, "PRECONDITION_FAILURE %s moduleSite=%u abiSite=%u\n", test->name,
                (unsigned)validationDiagnostic.instructionId, (unsigned)abiDiagnostic.instructionId);
        return;
    }
    cStatus = backend_aot_ir_c_emit_const_i64(&f.module, 1u, c, sizeof(c), &cLength, &cDiagnostic);
    llvmStatus = backend_aot_ir_llvm_emit_const_i64(&f.module, 1u, llvm, sizeof(llvm), &llvmLength, &llvmDiagnostic);
    printf("EMITTER %s C=%u LLVM=%u C_site=%u LLVM_site=%u expectedBits=%llu\n",
           test->name, (unsigned)cStatus, (unsigned)llvmStatus,
           (unsigned)cDiagnostic.instructionId, (unsigned)llvmDiagnostic.instructionId,
           (unsigned long long)(uint64_t)test->expected);
    if (cStatus != ZR_AOT_IR_OK || llvmStatus != ZR_AOT_IR_OK) {
        ++counts->featureFailures;
        if ((cStatus != ZR_AOT_IR_OK && (c[0] != '\0' || cLength != 0u)) ||
            (llvmStatus != ZR_AOT_IR_OK && (llvm[0] != '\0' || llvmLength != 0u))) {
            ++counts->outputFailures;
            fprintf(stderr, "OUTPUT_FAILURE %s stale failure output\n", test->name);
        }
        return;
    }
    outputOk = cLength == strlen(c) && llvmLength == strlen(llvm) &&
            strstr(c, test->predicate == 1u ? " < " : " > ") != NULL &&
            strstr(c, "if (") != NULL &&
            strstr(llvm, test->predicate == 1u ? "icmp slt i64 " : "icmp sgt i64 ") != NULL &&
            strstr(llvm, "br i1 %") != NULL && strstr(llvm, "phi ") == NULL;
    if (directory != NULL && outputOk) {
        const int count = snprintf(runner, sizeof(runner),
                "#include <stdint.h>\nextern int64_t zr_aot_scalar_fn_1(void);\n"
                "int main(void) { return (uint64_t)zr_aot_scalar_fn_1() == UINT64_C(%llu) ? 0 : 1; }\n",
                (unsigned long long)(uint64_t)test->expected);
        outputOk = count > 0 && (size_t)count < sizeof(runner) &&
                write_artifact(directory, test->name, "c", c) &&
                write_artifact(directory, test->name, "ll", llvm) &&
                write_artifact(directory, test->name, "runner.c", runner);
    }
    /* Exact NUL capacity must succeed; one byte less fails on reused buffers. */
    if (outputOk) {
        const size_t cExact = cLength, llvmExact = llvmLength;
        outputOk = backend_aot_ir_c_emit_const_i64(&f.module, 1u, c, cExact + 1u, &cLength, &cDiagnostic) == ZR_AOT_IR_OK &&
                cLength == cExact &&
                backend_aot_ir_llvm_emit_const_i64(&f.module, 1u, llvm, llvmExact + 1u, &llvmLength, &llvmDiagnostic) == ZR_AOT_IR_OK &&
                llvmLength == llvmExact;
        if (outputOk) {
            outputOk = backend_aot_ir_c_emit_const_i64(&f.module, 1u, c, cExact, &cLength, &cDiagnostic) == ZR_AOT_IR_INVALID_RANGE &&
                    c[0] == '\0' && cLength == 0u &&
                    backend_aot_ir_llvm_emit_const_i64(&f.module, 1u, llvm, llvmExact, &llvmLength, &llvmDiagnostic) == ZR_AOT_IR_INVALID_RANGE &&
                    llvm[0] == '\0' && llvmLength == 0u;
            if (outputOk) {
                outputOk = backend_aot_ir_c_emit_const_i64(&f.module, 1u, c, sizeof(c), &cLength, NULL) == ZR_AOT_IR_OK &&
                        cLength == cExact &&
                        backend_aot_ir_llvm_emit_const_i64(&f.module, 1u, llvm, sizeof(llvm), &llvmLength, NULL) == ZR_AOT_IR_OK &&
                        llvmLength == llvmExact;
            }
        }
    }
    if (!outputOk) {
        ++counts->outputFailures;
        fprintf(stderr, "OUTPUT_FAILURE %s text, artifacts or exact capacity\n", test->name);
    } else {
        printf("PASS %s\n", test->name);
    }
}

typedef EZrAotIrStatus (*ConditionalEmitter)(const SZrAotIrModule *, TZrUInt32,
        char *, size_t, size_t *, SZrAotIrDiagnostic *);

static void guards(ConditionalCounts *counts) {
    static const char *names[16] = {
        "predicate_le", "comparison_match_type", "comparison_self_operand", "branch_input_operand",
        "true_return_input", "false_return_sibling", "constant_flags", "constant_type",
        "comparison_throw_flag", "function_throw_effect", "duplicate_targets", "wrong_predecessor",
        "false_return_type", "duplicate_value", "unused_constant", "unknown_abi"
    };
    static const EZrAotIrStatus statuses[16] = {
        ZR_AOT_IR_UNSUPPORTED, ZR_AOT_IR_INVALID_ID, ZR_AOT_IR_UNSUPPORTED, ZR_AOT_IR_UNSUPPORTED,
        ZR_AOT_IR_UNSUPPORTED, ZR_AOT_IR_UNSUPPORTED, ZR_AOT_IR_UNSUPPORTED, ZR_AOT_IR_INVALID_SIGNATURE,
        ZR_AOT_IR_UNSUPPORTED, ZR_AOT_IR_UNSUPPORTED, ZR_AOT_IR_INVALID_CFG, ZR_AOT_IR_INVALID_CFG,
        ZR_AOT_IR_INVALID_SIGNATURE, ZR_AOT_IR_UNSUPPORTED, ZR_AOT_IR_UNSUPPORTED, ZR_AOT_IR_UNSUPPORTED
    };
    static const unsigned sites[16] = {102u,102u,102u,103u,105u,107u,100u,101u,102u,0u,0u,0u,107u,102u,0u,0u};
    static const ConditionalEmitter emitters[2] = {
        backend_aot_ir_c_emit_const_i64, backend_aot_ir_llvm_emit_const_i64
    };
    const ConditionalCase base = {"guard_base", 1u, 19, 23, 0, 0, 101};
    unsigned row, emitter;
    for (row = 0u; row < 16u; ++row) {
        ConditionalFixture f;
        SZrExecIrConstant extended[5];
        int ok = 1;
        fixture(&f, &base);
        switch (row) {
            case 0u: f.instructions[2].typeToken = 2u; break;
            case 1u: f.instructions[2].matchTypeToken = 17u; break;
            case 2u: f.operands[0] = 47u; break;
            case 3u: f.operands[2] = 11u; break;
            case 4u: f.operands[3] = 11u; break;
            case 5u: f.operands[4] = 61u; break;
            case 6u: f.constants[3].flags = 1u; break;
            case 7u: f.constants[1].typeToken = 18u; break;
            case 8u: f.instructions[2].flags = ZR_EXEC_IR_FLAG_MAY_THROW; break;
            case 9u: f.function.contract.declaredEffects = ZR_EXECUTION_EFFECT_THROW; break;
            case 10u: f.edges[1] = 8u; break;
            case 11u: f.edges[3] = 8u; break;
            case 12u: f.instructions[7].typeToken = 18u; break;
            case 13u: f.results[2] = 11u; break;
            case 14u:
                memcpy(extended, f.constants, sizeof(f.constants));
                extended[4] = f.constants[0];
                f.module.constantPool = extended;
                f.module.constantCount = 5u;
                break;
            case 15u:
                f.function.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_UNKNOWN;
                f.function.callableAbi.returnTypeToken = 0u;
                break;
        }
        ++counts->guards;
        for (emitter = 0u; emitter < 2u; ++emitter) {
            SZrAotIrDiagnostic diagnostic;
            char text[2048] = "stale";
            size_t length = 99u;
            EZrAotIrStatus status = emitters[emitter](&f.module, 1u, text, sizeof(text), &length, &diagnostic);
            int matches = status == statuses[row] && diagnostic.status == status &&
                    diagnostic.functionId == 1u && diagnostic.instructionId == sites[row] &&
                    text[0] == '\0' && length == 0u;
            if (row == 10u || row == 11u) matches = matches && diagnostic.blockId == 3u;
            printf("GUARD %s emitter=%u status=%u site=%u match=%d\n", names[row], emitter,
                    (unsigned)status, (unsigned)diagnostic.instructionId, matches);
            if (!matches) ok = 0;
        }
        if (!ok) ++counts->guardFailures;
    }
}

static void arguments(ConditionalCounts *counts) {
    static const ConditionalEmitter emitters[2] = {
        backend_aot_ir_c_emit_const_i64, backend_aot_ir_llvm_emit_const_i64
    };
    const ConditionalCase base = {"argument_base", 1u, 19, 23, 0, 0, 101};
    ConditionalFixture f;
    unsigned emitter, row;
    fixture(&f, &base);
    for (emitter = 0u; emitter < 2u; ++emitter) {
        for (row = 0u; row < 5u; ++row) {
            char text[2048] = "stale";
            size_t length = 99u;
            SZrAotIrDiagnostic diagnostic;
            EZrAotIrStatus status;
            int ok;
            ++counts->argumentChecks;
            status = emitters[emitter](row == 0u ? NULL : &f.module, 1u,
                    row == 1u ? NULL : text, row == 2u ? 0u : sizeof(text),
                    row == 3u ? NULL : &length, row == 4u ? NULL : &diagnostic);
            if (row == 4u) ok = status == ZR_AOT_IR_OK && length == strlen(text) && length > 0u;
            else {
                ok = status == ZR_AOT_IR_INVALID_ARGUMENT && diagnostic.status == status;
                if (row != 3u) ok = ok && length == 0u;
                if (row != 1u && row != 2u) ok = ok && text[0] == '\0';
                if (row == 2u) ok = ok && strcmp(text, "stale") == 0;
            }
            printf("ARGUMENT row=%u emitter=%u status=%u match=%d\n", row, emitter, (unsigned)status, ok);
            if (!ok) ++counts->argumentFailures;
        }
    }
}

int main(int argc, char **argv) {
    static const ConditionalCase cases[] = {
        {"lt_true", 1u, 19, 23, 0, 0, 101},
        {"lt_false", 1u, 23, 19, 0, 0, -202},
        {"lt_equal", 1u, 7, 7, 0, 0, -202},
        {"lt_negative", 1u, -19, -23, 0, 0, -202},
        {"lt_extremes", 1u, INT64_MIN, INT64_MAX, 0, 0, 101},
        {"lt_reversed_operands", 1u, INT64_MIN, INT64_MAX, 1, 0, -202},
        {"gt_true", 3u, 23, 19, 0, 0, 101},
        {"gt_false", 3u, 19, 23, 0, 0, -202},
        {"gt_equal", 3u, -7, -7, 0, 0, -202},
        {"gt_mixed", 3u, 0, -1, 0, 0, 101},
        {"gt_extremes", 3u, INT64_MAX, INT64_MIN, 0, 0, 101},
        {"gt_reversed_edges", 3u, INT64_MAX, INT64_MIN, 0, 1, -202}
    };
    ConditionalCounts counts = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    size_t i;
    if (argc != 1 && argc != 2) {
        fprintf(stderr, "usage: test_ssa_aot_scalar_conditional [existing-product-directory]\n");
        return 2;
    }
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        positive(&cases[i], argc == 2 ? argv[1] : NULL, &counts);
    }
    guards(&counts);
    arguments(&counts);
    printf("RESULT positives=%u precondition_failures=%u feature_failures=%u output_failures=%u guards_executed=%u guard_failures=%u argument_checks=%u argument_failures=%u\n",
           counts.positives, counts.preconditionFailures, counts.featureFailures, counts.outputFailures,
           counts.guards, counts.guardFailures, counts.argumentChecks, counts.argumentFailures);
    return counts.preconditionFailures == 0u && counts.featureFailures == 0u && counts.outputFailures == 0u &&
            counts.guardFailures == 0u && counts.argumentFailures == 0u ? 0 : 1;
}
