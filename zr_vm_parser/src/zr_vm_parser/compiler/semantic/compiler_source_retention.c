#include "compiler/semantic/compiler_source_retention_internal.h"
#include "exec_ir/source/exec_ir_source_finalized_module_internal.h"

#include <stdint.h>
#include <string.h>
#include "zr_vm_core/function_aot_ir.h"
#include "zr_vm_core/function_exec_ir.h"
#include "exec_ir/aot_ir_projection_descriptor.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "exec_ir/exec_ir_dead_source_places.h"
#include "exec_ir/exec_ir_host_aot_target.h"
#include "exec_ir/exec_ir_host_primitive_layout.h"
#include "exec_ir/source/exec_ir_source_frame.h"
#include "exec_ir/source/exec_ir_source_module_contract.h"
#include "exec_ir/source/exec_ir_source_scalar_constant.h"
#include "exec_ir/source/exec_ir_source_integer_constants.h"

typedef struct SSourceRetentionProducer {
    SZrExecIrModule original;
    SZrExecIrFunction compacted;
    SZrAotIrProjection projection;
    SZrAotIrTargetContract target;
    SZrAotIrProjectionDescriptor descriptor;
} SSourceRetentionProducer;

static void retention_cleanup(SSourceRetentionProducer *p) {
    ZrParser_AotIrProjection_FreeDescriptor(&p->descriptor);
    ZrParser_AotIrProjection_Free(&p->projection);
    ZrCore_ExecIr_FreeFunction(&p->compacted);
    ZrCore_ExecIr_FreeModule(&p->original);
}

static void retention_result(SZrSourceCompileDiagnostic *d,
        EZrSourceCompileStage stage, EZrSourceCompileResult result) {
    memset(d, 0, sizeof(*d));
    d->stage = stage;
    d->result = result;
}

/* Use the original parser range, not narrowed ExecIR coordinates. */
static void retention_range(const SZrCompilerState *cs, TZrUInt32 sourceId,
        SZrSourceCompileDiagnostic *d) {
    const SZrSemanticIrFunction *s = &cs->preSemanticIr;
    if (sourceId == 0u || sourceId > s->instructions.length) return;
    const SZrArray *maps = &s->sourceMap;
    if (!maps->isValid || maps->elementSize != sizeof(SZrSemanticIrSourceMapEntry) ||
        maps->length > maps->capacity || maps->head == ZR_NULL ||
        maps->capacity > SIZE_MAX / sizeof(SZrSemanticIrSourceMapEntry) ||
        (uintptr_t)maps->head % _Alignof(SZrSemanticIrSourceMapEntry) != 0u ||
        maps->capacity * sizeof(SZrSemanticIrSourceMapEntry) >
                UINTPTR_MAX - (uintptr_t)maps->head) return;
    for (TZrSize row = 0u; row < maps->length; ++row) {
        const SZrSemanticIrSourceMapEntry *map =
                &((const SZrSemanticIrSourceMapEntry *)maps->head)[row];
        if (map->instructionId != sourceId) continue;
        d->hasRange = ZR_TRUE;
        d->startOffset = map->sourceRange.start.offset;
        d->endOffset = map->sourceRange.end.offset;
        d->startLine = map->sourceRange.start.line;
        d->startColumn = map->sourceRange.start.column;
        d->endLine = map->sourceRange.end.line;
        d->endColumn = map->sourceRange.end.column;
        return;
    }
}

static void retention_exec_failure(const SZrCompilerState *cs,
        EZrSourceCompileStage stage, const SZrExecIrDiagnostic *lower,
        SZrSourceCompileDiagnostic *d) {
    EZrSourceCompileResult result = ZR_SOURCE_RESULT_PRODUCER_FAILED;
    if (lower->code == ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY)
        result = ZR_SOURCE_RESULT_OUT_OF_MEMORY;
    else if (lower->code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED)
        result = ZR_SOURCE_RESULT_UNSUPPORTED;
    retention_result(d, stage, result);
    d->hasExecIr = ZR_TRUE;
    memcpy(&d->execIr, lower, sizeof(*lower));
    retention_range(cs, lower->sourceId, d);
}

static void retention_aot_failure(const SZrCompilerState *cs,
        const SZrExecIrFunction *f, EZrSourceCompileStage stage,
        const SZrAotIrDiagnostic *lower, SZrSourceCompileDiagnostic *d) {
    EZrSourceCompileResult result = ZR_SOURCE_RESULT_PRODUCER_FAILED;
    if (lower->status == ZR_AOT_IR_OUT_OF_MEMORY)
        result = ZR_SOURCE_RESULT_OUT_OF_MEMORY;
    else if (lower->status == ZR_AOT_IR_UNSUPPORTED)
        result = ZR_SOURCE_RESULT_UNSUPPORTED;
    retention_result(d, stage, result);
    d->hasAotIr = ZR_TRUE;
    memcpy(&d->aotIr, lower, sizeof(*lower));
    if (lower->instructionId == 0u ||
        (lower->functionId != 0u && lower->functionId != f->id)) return;
    for (TZrUInt32 i = 0u; i < f->sourceMapCount; ++i) {
        if (f->sourceMaps[i].instructionId == lower->instructionId) {
            retention_range(cs, f->sourceMaps[i].sourceId, d);
            return;
        }
    }
}

static TZrBool retention_metadata(const SZrFunction *f,
        TZrMetadataToken *moduleToken, TZrMetadataToken *entryToken,
        SZrExecIrDiagnostic *d) {
    TZrSize count = f->metadataTokenRecordLength;
    const SZrMetadataTokenRecord *records = f->metadataTokenRecords;
    const SZrMetadataTokenRecord *module = ZR_NULL, *entry = ZR_NULL;
    if (count == 0u || records == ZR_NULL || count > SIZE_MAX / sizeof(*records) ||
        (uintptr_t)records % _Alignof(SZrMetadataTokenRecord) != 0u ||
        count * sizeof(*records) > UINTPTR_MAX - (uintptr_t)records) {
        d->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
        d->expectedHash = 1u;
        d->actualHash = count;
        return ZR_FALSE;
    }
    for (TZrSize i = 0u; i < count; ++i) {
        const SZrMetadataTokenRecord *r = &records[i];
        if (ZR_METADATA_TOKEN_TABLE(r->token) == ZR_METADATA_TABLE_MODULE) {
            if (module != ZR_NULL) goto mismatch;
            module = r;
        }
        if (ZR_METADATA_TOKEN_TABLE(r->token) == ZR_METADATA_TABLE_MEMBER_DEF &&
            r->reserved0 == ZR_METADATA_TOKEN_RECORD_SCRIPT_ENTRY) {
            if (entry != ZR_NULL) goto mismatch;
            entry = r;
        }
    }
    if (module == ZR_NULL || entry == ZR_NULL || module->token == 0u ||
        entry->token == 0u || entry->ownerToken != module->token ||
        f->moduleSignatureHash == 0u) goto mismatch;
    *moduleToken = module->token;
    *entryToken = entry->token;
    return ZR_TRUE;
mismatch:
    d->code = ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH;
    d->expectedHash = 1u;
    return ZR_FALSE;
}

/* Only a genuine checked binary header selects the expression bridge.
 * Literal and malformed child paths retain their original allocation window. */
static TZrBool retention_integer_expression(const SZrCompilerState *cs) {
    const SZrAstNode *script = cs->scriptAst, *returned, *expression;
    const SZrAstNodeArray *statements;
#define HEADER(p, type) ((p) != ZR_NULL && (uintptr_t)(p) % _Alignof(type) == 0u && \
        sizeof(type) <= UINTPTR_MAX - (uintptr_t)(p))
    if (!HEADER(script, SZrAstNode) || script != cs->currentAst || script->type != ZR_AST_SCRIPT)
        return ZR_FALSE;
    statements = script->data.script.statements;
    if (!HEADER(statements, SZrAstNodeArray) || statements->count != 1u ||
        statements->count > statements->capacity || !statements->nodes ||
        statements->capacity > SIZE_MAX / sizeof(*statements->nodes) ||
        (uintptr_t)statements->nodes % _Alignof(SZrAstNode *) != 0u ||
        statements->capacity * sizeof(*statements->nodes) > UINTPTR_MAX - (uintptr_t)statements->nodes)
        return ZR_FALSE;
    returned = statements->nodes[0];
    if (!HEADER(returned, SZrAstNode) || returned->type != ZR_AST_RETURN_STATEMENT ||
        returned->data.returnStatement.isReferenceReturn) return ZR_FALSE;
    expression = returned->data.returnStatement.expr;
    return (TZrBool)(HEADER(expression, SZrAstNode) && expression->type == ZR_AST_BINARY_EXPRESSION);
#undef HEADER
}

/* Mirror the Core append reserve's complete numeric refusal conditions.
 * Only after this proof can an expression append's false mean realloc NULL.
 * The old literal bool-API diagnosis remains unchanged. */
static TZrBool retention_append_shape(const void *storage, TZrUInt32 count,
        TZrUInt32 capacity, size_t width, size_t align) {
    TZrSize requested, grown;
    if (count > capacity || count == UINT32_MAX ||
        ((capacity == 0u) != (storage == ZR_NULL)) ||
        (storage != ZR_NULL && (uintptr_t)storage % align != 0u) ||
        capacity > SIZE_MAX / width ||
        (size_t)capacity * width > UINTPTR_MAX - (uintptr_t)storage) return ZR_FALSE;
    requested = (TZrSize)count + 1u;
    grown = requested <= capacity ? capacity : (capacity == 0u ? 4u : capacity);
    while (grown < requested) {
        if (grown > (TZrSize)UINT32_MAX / 2u) { grown = requested; break; }
        grown *= 2u;
    }
    return (TZrBool)(grown <= UINT32_MAX && grown <= SIZE_MAX / width);
}

TZrBool compiler_retain_source_scalar_metadata(SZrCompilerState *cs,
        SZrFunction *selected, EZrSourceRetention mode,
        SZrSourceCompileDiagnostic *diagnostic) {
    SZrSourceCompileDiagnostic local;
    SZrSourceCompileDiagnostic *d = diagnostic != ZR_NULL ? diagnostic : &local;
    SSourceRetentionProducer p = {0};
    SSourceIntegerProof integerProof = {0};
    SZrExecIrDiagnostic e = {0};
    SZrAotIrDiagnostic a = {0};
    SZrExecIrConstant constant = {0};
    SZrExecIrBuildInput build = {0};
    SZrExecIrLayout layout = {0};
    SZrExecIrRange range = {0};
    SZrAotIrCallableAbi abi = {0};
    TZrMetadataToken moduleToken = 0u, entryToken = 0u;
    SZrExecIrFunction *original;
    TZrBool success = ZR_FALSE;
    TZrBool integerExpression = ZR_FALSE;
    retention_result(d, ZR_SOURCE_STAGE_COMPILE, ZR_SOURCE_RESULT_INVALID_ARGUMENT);
    if (cs == ZR_NULL || selected == ZR_NULL ||
        (mode != ZR_SOURCE_RETENTION_REQUIRE_SCALAR_AOT &&
         mode != ZR_SOURCE_RETENTION_REQUIRE_SCALAR_EXECIR)) goto done;
    retention_result(d, ZR_SOURCE_STAGE_COMPILE, ZR_SOURCE_RESULT_UNSUPPORTED);
    if (cs->state == ZR_NULL || selected != cs->currentFunction ||
        cs->emitTestManifest || cs->testManifestEntries.length != 0u ||
        cs->childFunctions.length != 0u || cs->closureVarCount != 0u ||
        cs->submissionDeclaredCaptureIndices.length != 0u ||
        ZrCore_Function_GetAotIr(selected) != ZR_NULL ||
        ZrCore_Function_GetExecIr(selected) != ZR_NULL) goto done;
    /* Bridge owns finite SCRIPT/canonical/geometry/profile validation.
     * A duplicated I64 filter would hide its bool/float diagnostic. */
    integerExpression = (TZrBool)(mode == ZR_SOURCE_RETENTION_REQUIRE_SCALAR_EXECIR &&
            retention_integer_expression(cs));
    if (integerExpression) {
        if (!source_integer_constants_prepare(cs, &integerProof, &e) ||
            !source_integer_constant_at(&integerProof, 0u, &constant, &e)) {
            retention_exec_failure(cs, ZR_SOURCE_STAGE_CONSTANT, &e, d);
            goto done;
        }
    } else if (!ZrParser_ExecIr_MakeSourceScalarConstant(cs, &constant, &e)) {
        retention_exec_failure(cs, ZR_SOURCE_STAGE_CONSTANT, &e, d);
        goto done;
    }
    if (!retention_metadata(selected, &moduleToken, &entryToken, &e)) {
        retention_exec_failure(cs, ZR_SOURCE_STAGE_MODULE_CONTRACT, &e, d);
        goto done;
    }
    ZrCore_ExecIr_ModuleInit(&p.original);
    ZrCore_ExecIr_FunctionInit(&p.compacted);
    p.original.moduleToken = moduleToken;
    p.original.moduleHash = selected->moduleSignatureHash;
    build.semanticFunction = &cs->preSemanticIr;
    build.functionToken = entryToken;
    build.signatureHash = selected->sourceCallableIdentity.canonicalSignatureHash;
    build.options.preserveSourceMaps = ZR_TRUE;
    build.options.preserveDeoptStates = ZR_TRUE;
#define RETENTION_EXEC(stage, expression) \
    do { memset(&e, 0, sizeof(e)); if (!(expression)) { \
        retention_exec_failure(cs, stage, &e, d); goto done; } } while (0)
#define RETENTION_AOT(stage, expression) \
    do { memset(&a, 0, sizeof(a)); if (!(expression)) { \
        retention_aot_failure(cs, &p.compacted, stage, &a, d); goto done; } } while (0)
    RETENTION_EXEC(ZR_SOURCE_STAGE_BUILD,
            ZrParser_ExecIr_BuildModule(&build, &p.original, &e));
    if (p.original.functionCount != 1u || p.original.functions == ZR_NULL) {
        retention_result(d, ZR_SOURCE_STAGE_BUILD, ZR_SOURCE_RESULT_PRODUCER_FAILED);
        goto done;
    }
    original = &p.original.functions[0];
    /* Bool append APIs carry no diagnostic: generic failure is not guessed OOM. */
    if (integerExpression) {
        for (TZrUInt32 i = 0u; i < integerProof.functionConstantCount; ++i) {
            RETENTION_EXEC(ZR_SOURCE_STAGE_CONSTANT,
                    source_integer_constant_at(&integerProof, i, &constant, &e));
            if (!retention_append_shape(p.original.constants, p.original.constantCount,
                    p.original.constantCapacity, sizeof(constant), _Alignof(SZrExecIrConstant))) {
                retention_result(d, ZR_SOURCE_STAGE_BUILD, ZR_SOURCE_RESULT_PRODUCER_FAILED);
                goto done;
            }
            if (!ZrCore_ExecIr_ModuleAppendConstant(&p.original, &constant, 1u, &range)) {
                memset(&e, 0, sizeof(e)); e.code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
                e.functionToken = original->functionToken;
                retention_exec_failure(cs, ZR_SOURCE_STAGE_BUILD, &e, d); goto done;
            }
            if (range.offset != i || range.count != 1u) {
                retention_result(d, ZR_SOURCE_STAGE_BUILD, ZR_SOURCE_RESULT_PRODUCER_FAILED); goto done;
            }
        }
    } else {
        if (!ZrCore_ExecIr_ModuleAppendConstant(&p.original, &constant, 1u, &range) ||
            range.offset != 0u || range.count != 1u) {
            retention_result(d, ZR_SOURCE_STAGE_BUILD, ZR_SOURCE_RESULT_PRODUCER_FAILED);
            goto done;
        }
    }
    RETENTION_EXEC(ZR_SOURCE_STAGE_BUILD,
            ZrCore_ExecIr_VerifyFunction(original, ZR_EXEC_IR_VERIFY_ALL, &e));
    RETENTION_EXEC(ZR_SOURCE_STAGE_MODULE_CONTRACT,
            ZrParser_ExecIr_BindSourceModuleContract(cs, &p.original, &e));
    RETENTION_EXEC(ZR_SOURCE_STAGE_COMPACTION,
            ZrParser_ExecIr_EliminateDeadSourcePlaces(&cs->preSemanticIr,
                    cs->semanticContext, original, &p.compacted, &e));
    if (p.original.layoutCount == UINT32_MAX) {
        retention_result(d, ZR_SOURCE_STAGE_LAYOUT, ZR_SOURCE_RESULT_PRODUCER_FAILED);
        goto done;
    }
    RETENTION_EXEC(ZR_SOURCE_STAGE_LAYOUT,
            ZrParser_ExecIr_MakeHostPrimitiveLayout(cs->semanticContext,
                    constant.typeToken, p.original.layoutCount + 1u, &layout, &e));
    if (integerExpression) {
        if (!retention_append_shape(p.original.layouts, p.original.layoutCount,
                p.original.layoutCapacity, sizeof(layout), _Alignof(SZrExecIrLayout))) {
            retention_result(d, ZR_SOURCE_STAGE_LAYOUT, ZR_SOURCE_RESULT_PRODUCER_FAILED); goto done;
        }
        if (!ZrCore_ExecIr_ModuleAppendLayout(&p.original, &layout, 1u, &range)) {
            memset(&e, 0, sizeof(e)); e.code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
            e.functionToken = original->functionToken;
            retention_exec_failure(cs, ZR_SOURCE_STAGE_LAYOUT, &e, d); goto done;
        }
        if (range.count != 1u || range.offset >= p.original.layoutCount) {
            retention_result(d, ZR_SOURCE_STAGE_LAYOUT, ZR_SOURCE_RESULT_PRODUCER_FAILED); goto done;
        }
    } else {
        if (!ZrCore_ExecIr_ModuleAppendLayout(&p.original, &layout, 1u, &range) ||
            range.count != 1u || range.offset >= p.original.layoutCount) {
            retention_result(d, ZR_SOURCE_STAGE_LAYOUT, ZR_SOURCE_RESULT_PRODUCER_FAILED);
            goto done;
        }
    }
    RETENTION_EXEC(ZR_SOURCE_STAGE_FRAME,
            ZrParser_ExecIr_AttachPrimitiveSourceFrame(&p.compacted, cs->semanticContext,
                    p.original.layouts, p.original.layoutCount, 0u, &e));
    RETENTION_EXEC(ZR_SOURCE_STAGE_FRAME,
            ZrCore_ExecIr_VerifyFunction(&p.compacted, ZR_EXEC_IR_VERIFY_ALL, &e));
    if (mode == ZR_SOURCE_RETENTION_REQUIRE_SCALAR_EXECIR) {
        RETENTION_EXEC(ZR_SOURCE_STAGE_EXECIR_OWNER,
                source_snapshot_finalized_primitive_module(&cs->preSemanticIr,
                        cs->semanticContext, cs->scriptAst, &selected->sourceCallableIdentity,
                        &cs->constants, selected->constantValueList, selected->constantValueLength,
                        &p.original, &p.compacted, &selected->ownedExecIr, &e));
        success = ZR_TRUE;
        memset(d, 0, sizeof(*d));
        goto done;
    }
    RETENTION_EXEC(ZR_SOURCE_STAGE_PROJECTION,
            ZrParser_ExecIr_LowerAotWithCanonicalCallable(&p.compacted,
                    p.original.constants, p.original.constantCount,
                    p.original.layouts, p.original.layoutCount, cs->semanticContext,
                    cs->preSemanticIr.callableTypeId, &p.projection, &e));
    RETENTION_AOT(ZR_SOURCE_STAGE_TARGET,
            ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(cs->semanticContext,
                    cs->preSemanticIr.callableTypeId, &p.original.layouts[range.offset],
                    &p.target, &a));
    RETENTION_AOT(ZR_SOURCE_STAGE_DESCRIPTOR,
            ZrParser_AotIrProjection_BuildDescriptor(&p.projection, &p.target,
                    &p.original.contract, &p.descriptor, &a));
    RETENTION_AOT(ZR_SOURCE_STAGE_DESCRIPTOR,
            ZrCore_AotIr_ValidateModule(&p.descriptor.module, &a) == ZR_AOT_IR_OK);
    RETENTION_AOT(ZR_SOURCE_STAGE_DESCRIPTOR,
            ZrCore_AotIr_RequireExecutableAbi(&p.descriptor.module,
                    p.descriptor.function.id, &abi, &a) == ZR_AOT_IR_OK);
    RETENTION_AOT(ZR_SOURCE_STAGE_OWNER,
            ZrCore_Function_SnapshotAotIr(selected, &p.descriptor.module, &a) == ZR_AOT_IR_OK);
    success = ZR_TRUE;
    memset(d, 0, sizeof(*d));
done:
    source_integer_proof_free(&integerProof);
    retention_cleanup(&p);
    return success;
#undef RETENTION_EXEC
#undef RETENTION_AOT
}
