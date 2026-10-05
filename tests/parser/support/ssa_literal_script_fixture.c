#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CRT declarations precede Unity's noreturn macro on Windows. */
#include "unity.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/project_imports.h"
#include "../../../zr_vm_parser/src/zr_vm_parser/compiler/module_init_analysis.h"

#include "ssa_literal_script_fixture.h"

enum { SSA_LITERAL_SCRIPT_ORACLE_STEPS = 32 };

void ZrTests_SsaLiteralScriptFixture_Init(SZrSsaLiteralScriptFixture *fixture, SZrState *state) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->state = state;
    ZrCore_ExecIr_ModuleInit(&fixture->module);
}

void ZrTests_SsaLiteralScriptFixture_Free(SZrSsaLiteralScriptFixture *fixture) {
    SZrState *state = fixture->state;
    ZrCore_ExecIr_FreeModule(&fixture->module);
    free(fixture->constants);
    free(fixture->initialValues);
    if (fixture->compilerInitialized) {
        if (fixture->compiler.currentFunction != ZR_NULL) {
            if (fixture->functionRooted)
                ZrCore_GarbageCollector_UnignoreObject(state->global,
                        &fixture->compiler.currentFunction->super);
            ZrCore_Function_Free(state, fixture->compiler.currentFunction);
            fixture->compiler.currentFunction = ZR_NULL;
        }
        ZrParser_CompilerState_Free(&fixture->compiler);
    }
    if (fixture->ast != ZR_NULL) {
        ZrParser_ModuleInitAnalysis_ClearAstIdentity(state->global, fixture->ast);
        ZrParser_Ast_Free(state, fixture->ast);
    }
    memset(fixture, 0, sizeof(*fixture));
}

static void assert_api(TZrBool success, const SZrExecIrDiagnostic *diagnostic,
                       const char *message) {
    if (!success && diagnostic != ZR_NULL) {
        (void)printf("%s: code=%u token=%u instruction=%u source=%u\n", message,
                (unsigned)diagnostic->code, (unsigned)diagnostic->functionToken,
                (unsigned)diagnostic->instructionId, (unsigned)diagnostic->sourceId);
    }
    TEST_ASSERT_TRUE_MESSAGE(success, message);
}

SZrExecIrFunction *ZrTests_SsaLiteralScriptFixture_Function(SZrSsaLiteralScriptFixture *fixture) {
    return ZrCore_ExecIr_ModuleFunctionAt(&fixture->module, 1u);
}

/* These digests detect in-place changes, including allocation identity and
 * capacities. They are observations within one test, never ABI identities. */
TZrUInt64 ZrTests_SsaLiteralScriptFixture_DigestBytes(TZrUInt64 previous, const void *bytes, size_t size) {
    return previous ^ ZrCore_Hash_CreateStable64WithPrefix(
            (const TZrByte *)&previous, sizeof(previous),
            (const TZrByte *)bytes, size);
}

static TZrUInt64 digest_array(TZrUInt64 hash, const SZrArray *array) {
    return ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, array->head, array->length * array->elementSize);
}

TZrUInt64 ZrTests_SsaLiteralScriptFixture_FunctionDigest(const SZrExecIrFunction *function) {
    TZrUInt64 hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(0u, function, sizeof(*function));
#define HASH_POOL(field, count) \
    hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, function->field, \
            (size_t)function->count * sizeof(*function->field))
    HASH_POOL(values, valueCount);
    HASH_POOL(instructions, instructionCount);
    HASH_POOL(blocks, blockCount);
    HASH_POOL(operandPool, operandCount);
    HASH_POOL(resultPool, resultCount);
    HASH_POOL(memoryTokenPool, memoryTokenCount);
    HASH_POOL(phiPool, phiCount);
    HASH_POOL(phiIncoming, phiIncomingCount);
    HASH_POOL(predecessors, predecessorCount);
    HASH_POOL(successors, successorCount);
    HASH_POOL(gcRoots, gcRootCount);
    HASH_POOL(deoptStates, deoptStateCount);
    HASH_POOL(deoptValues, deoptValueCount);
    HASH_POOL(deoptAggregates, deoptAggregateCount);
    HASH_POOL(deoptAggregateFields, deoptAggregateFieldCount);
    HASH_POOL(sourceMaps, sourceMapCount);
    HASH_POOL(bindingRows, bindingRowCount);
#undef HASH_POOL
    if (function->stateMap != ZR_NULL) {
        const SZrExecIrStateMap *map = function->stateMap;
        hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, map, sizeof(*map));
        hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, map->entries, (size_t)map->entryCount * sizeof(*map->entries));
        hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, map->valuePool, (size_t)map->valueCount * sizeof(*map->valuePool));
        hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, map->rootPool, (size_t)map->rootCount * sizeof(*map->rootPool));
        hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, map->ownerStatePool,
                (size_t)map->ownerStateCount * sizeof(*map->ownerStatePool));
    }
    return hash;
}

TZrUInt64 ZrTests_SsaLiteralScriptFixture_SourceDigest(const SZrSsaLiteralScriptFixture *fixture) {
    const SZrSemanticIrFunction *semantic = &fixture->compiler.preSemanticIr;
    const SZrSemanticContext *context = fixture->compiler.semanticContext;
    TZrUInt64 hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(0u, semantic, sizeof(*semantic));
#define HASH_SEM(field) hash = digest_array(hash, &semantic->field)
    HASH_SEM(places.places);
    HASH_SEM(cfg.blocks);
    HASH_SEM(locals);
    HASH_SEM(values);
    HASH_SEM(instructions);
    HASH_SEM(valueOperands);
    HASH_SEM(regions);
    HASH_SEM(cleanupScopes);
    HASH_SEM(sourceMap);
    HASH_SEM(loanFacts);
    HASH_SEM(escapeFacts);
    HASH_SEM(contiguousViewFacts);
    HASH_SEM(boundsFacts);
    HASH_SEM(scalarScratchProofs);
#undef HASH_SEM
    hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, context, sizeof(*context));
#define HASH_CONTEXT(field) hash = digest_array(hash, &context->field)
    HASH_CONTEXT(canonicalTypes);
    HASH_CONTEXT(canonicalTypeHashBuckets);
    HASH_CONTEXT(canonicalTypeHashNext);
    HASH_CONTEXT(canonicalTypeDefinitions);
    HASH_CONTEXT(types);
    HASH_CONTEXT(symbols);
    HASH_CONTEXT(scopeFacts);
    HASH_CONTEXT(visibleSymbolFacts);
    HASH_CONTEXT(overloadSets);
    HASH_CONTEXT(cleanupPlan);
    HASH_CONTEXT(templateSegments);
    HASH_CONTEXT(queryDiagnostics);
    HASH_CONTEXT(expressionFacts);
    HASH_CONTEXT(referenceFacts);
    HASH_CONTEXT(numericFacts);
    HASH_CONTEXT(reachabilityFacts);
    HASH_CONTEXT(logicalFacts);
    HASH_CONTEXT(ownershipFacts);
    HASH_CONTEXT(ownershipIntrinsicFacts);
    HASH_CONTEXT(receiverGuardFacts);
    HASH_CONTEXT(diagnosticFacts);
    HASH_CONTEXT(propertyContracts);
    HASH_CONTEXT(relationFacts);
    HASH_CONTEXT(callEdgeFacts);
    HASH_CONTEXT(typeDisplayAliasFacts);
    HASH_CONTEXT(documentationFacts);
#undef HASH_CONTEXT
    return digest_array(hash, &fixture->compiler.constants);
}

static TZrBool literal_place_resolver(void *userData,
        const SZrExecIrInstruction *instruction, const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount, SZrExecIrOracleValue *result) {
    SZrSsaLiteralScriptFixture *fixture = (SZrSsaLiteralScriptFixture *)userData;
    const SZrSemanticIrInstruction *source;
    const SZrParserPlace *place;
    if (fixture == ZR_NULL || instruction == ZR_NULL || result == ZR_NULL ||
        instruction->opcode != ZR_EXEC_IR_OPCODE_PLACE_BASE ||
        instruction->sourceId == 0u || operandCount != 1u || operands == ZR_NULL)
        return ZR_FALSE;
    source = ZrParser_SemanticIr_InstructionAt(
            &fixture->compiler.preSemanticIr, instruction->sourceId - 1u);
    if (source == ZR_NULL || source->opcode != ZR_SEMANTIC_IR_PLACE_BASE)
        return ZR_FALSE;
    place = ZrParser_PlaceGraph_Get(&fixture->compiler.preSemanticIr.places, source->placeId);
    if (place == ZR_NULL || place->base.kind != ZR_PARSER_PLACE_BASE_TEMPORARY ||
        operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED ||
        operands[0].as.signedInteger != (TZrInt64)place->base.identity)
        return ZR_FALSE;
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = (TZrInt64)place->id;
    ++fixture->placeCalls;
    return ZR_TRUE;
}

static void attach_actual_constants(SZrSsaLiteralScriptFixture *fixture) {
    SZrCompilerState *compiler = &fixture->compiler;
    TZrSize index;
    TEST_ASSERT_GREATER_THAN_UINT32(0u, (TZrUInt32)compiler->constants.length);
    fixture->constants = (SZrExecIrOracleValue *)calloc(
            compiler->constants.length, sizeof(*fixture->constants));
    TEST_ASSERT_NOT_NULL(fixture->constants);
    for (index = 0u; index < compiler->constants.length; ++index) {
        const SZrTypeValue *value = (const SZrTypeValue *)ZrCore_Array_Get(&compiler->constants, index);
        const SZrSemanticIrInstruction *source = ZrParser_SemanticIr_InstructionAt(
                &compiler->preSemanticIr, 0u);
        const SZrCanonicalTypeNode *type;
        SZrExecIrConstant constant = {0};
        SZrExecIrRange range;
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, value->type);
        TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_CONSTANT, source->opcode);
        TEST_ASSERT_TRUE(source->hasConstantPoolIndex);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)index, source->constantPoolIndex);
        type = ZrParser_CanonicalType_Find(compiler->semanticContext, source->typeId);
        TEST_ASSERT_NOT_NULL(type);
        TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_PRIMITIVE, type->kind);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, type->data.primitive.valueType);
        constant.typeToken = source->typeId;
        constant.bits = (TZrUInt64)value->value.nativeObject.nativeInt64;
        TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendConstant(&fixture->module, &constant, 1u, &range));
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)index, range.offset);
        fixture->constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        fixture->constants[index].as.signedInteger = value->value.nativeObject.nativeInt64;
    }
}

void ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(const SZrSsaLiteralScriptFixture *fixture,
                               const SZrExecIrFunction *function) {
    const SZrSemanticIrFunction *semantic = &fixture->compiler.preSemanticIr;
    TZrUInt32 index;
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)semantic->sourceMap.length, function->sourceMapCount);
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        const SZrSemanticIrInstruction *source;
        const SZrExecIrSourceMap *map = ZR_NULL;
        TZrUInt32 mapIndex;
        TEST_ASSERT_TRUE(instruction->sourceId > 0u && instruction->sourceId <= semantic->instructions.length);
        source = ZrParser_SemanticIr_InstructionAt(semantic, instruction->sourceId - 1u);
        for (mapIndex = 0u; mapIndex < function->sourceMapCount; ++mapIndex) {
            if (function->sourceMaps[mapIndex].instructionId != index + 1u) continue;
            TEST_ASSERT_NULL_MESSAGE(map, "PRECONDITION: unique instruction source map");
            map = &function->sourceMaps[mapIndex];
        }
        TEST_ASSERT_NOT_NULL(map);
        TEST_ASSERT_EQUAL_UINT32(source->id, map->sourceId);
        TEST_ASSERT_EQUAL_UINT32(source->sourceRange.start.offset, map->startOffset);
        TEST_ASSERT_EQUAL_UINT32(source->sourceRange.end.offset, map->endOffset);
        TEST_ASSERT_EQUAL_UINT32(source->sourceRange.start.line, map->startLine);
        TEST_ASSERT_EQUAL_UINT32(source->sourceRange.start.column, map->startColumn);
        TEST_ASSERT_EQUAL_UINT32(source->sourceRange.end.line, map->endLine);
        TEST_ASSERT_EQUAL_UINT32(source->sourceRange.end.column, map->endColumn);
    }
}

void ZrTests_SsaLiteralScriptFixture_AssertOracle(SZrSsaLiteralScriptFixture *fixture,
                          const SZrExecIrFunction *function, TZrUInt32 expectedPlaceCalls,
                          SZrExecIrOracleExecutionResult *oracle) {
    SZrExecIrOracleInput input = {0};
    SZrExecIrDiagnostic diagnostic = {0};
    fixture->placeCalls = 0u;
    input.function = function;
    input.constants = fixture->constants;
    input.constantCount = fixture->module.constantCount;
    input.initialValues = fixture->initialValues;
    input.initialValueCount = function->valueCount;
    input.place = literal_place_resolver;
    input.placeUserData = fixture;
    input.maxSteps = SSA_LITERAL_SCRIPT_ORACLE_STEPS;
    assert_api(ZrCore_ExecIr_RunOracleEx(&input, oracle, &diagnostic), &diagnostic,
            "PRECONDITION: actual canonical graph Core Oracle");
    TEST_ASSERT_TRUE(oracle->returned);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_ORACLE_VALUE_SIGNED, oracle->returnValue.kind);
    TEST_ASSERT_EQUAL_INT64(fixture->expectedReturn, oracle->returnValue.as.signedInteger);
    TEST_ASSERT_EQUAL_UINT32(0u, oracle->eventCount);
    TEST_ASSERT_EQUAL_UINT32(expectedPlaceCalls, fixture->placeCalls);
}

void ZrTests_SsaLiteralScriptFixture_Prepare(SZrSsaLiteralScriptFixture *fixture,
        EZrSsaLiteralScriptSource source) {
    static const char *const sources[2] = {"return 9;\n", "return 8;\n"};
    static char names[2][40] = {
        "ssa_dead_source_places_nine.zr", "ssa_dead_source_places_eight.zr"};
    TZrUInt32 index = (TZrUInt32)source;
    SZrState *state = fixture->state;
    SZrCompilerState *compiler = &fixture->compiler;
    SZrString *name, *moduleKey = ZR_NULL;
    TZrChar error[ZR_PARSER_ERROR_BUFFER_LENGTH] = {0};
    SZrFileRange errorRange = {0};
    SZrExecIrBuildInput build = {0};
    SZrExecIrDiagnostic diagnostic = {0};
    const SZrMetadataTokenRecord *entry = ZR_NULL, *module = ZR_NULL;
    const SZrCanonicalTypeNode *callable;
    SZrExecIrFunction *function;
    const SZrSemanticIrInstruction *placeSource;
    const SZrParserPlace *place;
    TZrUInt32 row;
    TEST_ASSERT_TRUE(source == ZR_TEST_SSA_LITERAL_SCRIPT_NINE ||
            source == ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT);
    TEST_ASSERT_NOT_NULL(state);
    fixture->expectedReturn = source == ZR_TEST_SSA_LITERAL_SCRIPT_NINE ? 9 : 8;
    name = ZrCore_String_CreateFromNative(state, names[index]);
    TEST_ASSERT_NOT_NULL(name);
    fixture->ast = ZrParser_Parse(state, sources[index], strlen(sources[index]), name);
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture->ast, "PRECONDITION: real source AST");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ProjectImports_CanonicalizeAst(state, fixture->ast,
            name, &moduleKey, error, sizeof(error), &errorRange), error);
    TEST_ASSERT_NOT_NULL(moduleKey);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(
            state, moduleKey, fixture->ast), "PRECONDITION: ModuleInit Prepare");
    ZrParser_CompilerState_Init(compiler, state);
    fixture->compilerInitialized = ZR_TRUE;
    compiler->currentAst = fixture->ast;
    compiler->currentModuleKey = moduleKey;
    compiler->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);
    fixture->functionRooted = ZrCore_GarbageCollector_IgnoreObject(state,
            &compiler->currentFunction->super);
    TEST_ASSERT_TRUE(fixture->functionRooted);
    compile_script(compiler, fixture->ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler->hasError, "PRECONDITION: production compile_script");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_Compiler_ValidatePreSemanticIr(compiler),
            "PRECONDITION: source semantic validation");
    TEST_ASSERT_TRUE(compiler->preSemanticIrCfgActive);
    TEST_ASSERT_TRUE(compiler->preSemanticIrCfgTerminated);
    TEST_ASSERT_TRUE_MESSAGE(compiler_assemble_final_function(compiler,
            compiler->currentFunction, fixture->ast, ZR_TRUE, ZR_FALSE),
            "PRECONDITION: production assembly before Finalize");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ModuleInitAnalysis_FinalizeCurrentSourceModule(
            compiler, moduleKey, compiler->currentFunction), "PRECONDITION: ModuleInit Finalize");
    TEST_ASSERT_TRUE_MESSAGE(compiler->currentFunction->hasSourceCallableIdentity,
            "PRECONDITION: production literal SCRIPT canonical identity");
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&compiler->preSemanticIr));
    callable = ZrParser_CanonicalType_Find(compiler->semanticContext,
            compiler->preSemanticIr.callableTypeId);
    TEST_ASSERT_NOT_NULL(callable);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callable->kind);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, callable->structuralHash);
    TEST_ASSERT_EQUAL_UINT64(callable->structuralHash,
            compiler->currentFunction->sourceCallableIdentity.canonicalSignatureHash);
    for (row = 0u; row < compiler->currentFunction->metadataTokenRecordLength; ++row) {
        const SZrMetadataTokenRecord *record = &compiler->currentFunction->metadataTokenRecords[row];
        if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_MODULE) {
            TEST_ASSERT_NULL(module);
            module = record;
        }
        if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_MEMBER_DEF &&
            record->reserved0 == ZR_METADATA_TOKEN_RECORD_SCRIPT_ENTRY) {
            TEST_ASSERT_NULL(entry);
            entry = record;
        }
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(entry, "PRECONDITION: actual SCRIPT_ENTRY MEMBER_DEF");
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_UINT32(module->token, entry->ownerToken);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, compiler->currentFunction->moduleSignatureHash);
    fixture->module.moduleToken = module->token;
    fixture->module.moduleHash = compiler->currentFunction->moduleSignatureHash;
    fixture->module.contract.moduleHash = compiler->currentFunction->moduleSignatureHash;
    build.semanticFunction = &compiler->preSemanticIr;
    build.functionToken = entry->token;
    build.signatureHash = callable->structuralHash;
    build.options.preserveSourceMaps = ZR_TRUE;
    build.options.preserveDeoptStates = ZR_TRUE;
    assert_api(ZrParser_ExecIr_BuildModule(&build, &fixture->module, &diagnostic), &diagnostic,
            "PRECONDITION: BuildModule with production identity");
    function = ZrTests_SsaLiteralScriptFixture_Function(fixture);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(entry->token, function->functionToken);
    TEST_ASSERT_EQUAL_UINT64(callable->structuralHash, function->signatureHash);
    TEST_ASSERT_EQUAL_UINT32(entry->token, function->contract.targetToken);
    TEST_ASSERT_EQUAL_UINT64(callable->structuralHash, function->contract.signatureHash);
    attach_actual_constants(fixture);
    assert_api(ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "PRECONDITION: input VERIFY_ALL");
    TEST_ASSERT_EQUAL_UINT32(3u, function->instructionCount);
    TEST_ASSERT_EQUAL_UINT32(3u, function->valueCount);
    TEST_ASSERT_EQUAL_UINT32(1u, function->blockCount);
    TEST_ASSERT_EQUAL_UINT32(0u, function->phiCount);
    TEST_ASSERT_EQUAL_UINT32(0u, function->memoryTokenCount);
    TEST_ASSERT_EQUAL_UINT32(0u, function->deoptStateCount);
    TEST_ASSERT_EQUAL_UINT32(0u, function->bindingRowCount);
    TEST_ASSERT_EQUAL_UINT32(0u, function->gcMapCount);
    TEST_ASSERT_NULL(function->frameLayout);
    TEST_ASSERT_FALSE(function->sealed);
    ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(fixture, function);
    placeSource = ZrParser_SemanticIr_InstructionAt(&compiler->preSemanticIr, 1u);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_PLACE_BASE, placeSource->opcode);
    place = ZrParser_PlaceGraph_Get(&compiler->preSemanticIr.places, placeSource->placeId);
    TEST_ASSERT_NOT_NULL(place);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_PLACE_BASE_TEMPORARY, place->base.kind);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)place->projections.length);
    TEST_ASSERT_EQUAL_UINT32(placeSource->typeId, place->typeId);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_PLACE_BASE, function->instructions[1].opcode);
    TEST_ASSERT_EQUAL_UINT32(placeSource->id, function->instructions[1].sourceId);
    TEST_ASSERT_EQUAL_UINT32(2u, function->resultPool[function->instructions[1].results.offset]);
    TEST_ASSERT_EQUAL_UINT32(3u, function->operandPool[function->instructions[1].operands.offset]);
    TEST_ASSERT_TRUE((function->values[1].flags & ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS) != 0u);
    TEST_ASSERT_TRUE((function->values[2].flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u);
    TEST_ASSERT_EQUAL_UINT32(2u, function->values[1].definition);
    TEST_ASSERT_EQUAL_UINT32(0u, function->values[2].definition);
    if (function->stateMap != ZR_NULL) {
        TEST_ASSERT_EQUAL_UINT32(0u, function->stateMap->entryCount);
        TEST_ASSERT_EQUAL_UINT32(0u, function->stateMap->valueCount);
        TEST_ASSERT_EQUAL_UINT32(0u, function->stateMap->rootCount);
        TEST_ASSERT_EQUAL_UINT32(0u, function->stateMap->ownerStateCount);
    }
    fixture->initialValues = (SZrExecIrOracleValue *)calloc(function->valueCount,
            sizeof(*fixture->initialValues));
    TEST_ASSERT_NOT_NULL(fixture->initialValues);
    fixture->initialValues[2].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    fixture->initialValues[2].as.signedInteger = (TZrInt64)place->base.identity;
}
