#include "compiler/semantic/compiler_source_retention_internal.h"

#include <stdint.h>
#include <string.h>
#include "zr_vm_core/function_aot_ir.h"
#include "exec_ir/aot_ir_projection_descriptor.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "exec_ir/exec_ir_dead_source_places.h"
#include "exec_ir/exec_ir_host_aot_target.h"
#include "exec_ir/exec_ir_host_primitive_layout.h"
#include "exec_ir/source/exec_ir_source_frame.h"
#include "exec_ir/source/exec_ir_source_module_contract.h"
#include "exec_ir/source/exec_ir_source_scalar_constant.h"

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

TZrBool compiler_retain_source_scalar_aot(SZrCompilerState *cs, SZrFunction *selected,
        SZrSourceCompileDiagnostic *diagnostic) {
    SZrSourceCompileDiagnostic local;
    SZrSourceCompileDiagnostic *d = diagnostic != ZR_NULL ? diagnostic : &local;
    SSourceRetentionProducer p = {0};
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
    retention_result(d, ZR_SOURCE_STAGE_COMPILE, ZR_SOURCE_RESULT_INVALID_ARGUMENT);
    if (cs == ZR_NULL || selected == ZR_NULL) goto done;
    retention_result(d, ZR_SOURCE_STAGE_COMPILE, ZR_SOURCE_RESULT_UNSUPPORTED);
    if (cs->state == ZR_NULL || selected != cs->currentFunction ||
        cs->emitTestManifest || cs->testManifestEntries.length != 0u ||
        cs->childFunctions.length != 0u || cs->closureVarCount != 0u ||
        cs->submissionDeclaredCaptureIndices.length != 0u ||
        ZrCore_Function_GetAotIr(selected) != ZR_NULL) goto done;
    /* Bridge owns finite SCRIPT/canonical/geometry/profile validation.
     * A duplicated I64 filter would hide its bool/float diagnostic. */
    if (!ZrParser_ExecIr_MakeSourceScalarConstant(cs, &constant, &e)) {
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
    if (!ZrCore_ExecIr_ModuleAppendConstant(&p.original, &constant, 1u, &range) ||
        range.offset != 0u || range.count != 1u) {
        retention_result(d, ZR_SOURCE_STAGE_BUILD, ZR_SOURCE_RESULT_PRODUCER_FAILED);
        goto done;
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
    if (!ZrCore_ExecIr_ModuleAppendLayout(&p.original, &layout, 1u, &range) ||
        range.count != 1u || range.offset >= p.original.layoutCount) {
        retention_result(d, ZR_SOURCE_STAGE_LAYOUT, ZR_SOURCE_RESULT_PRODUCER_FAILED);
        goto done;
    }
    RETENTION_EXEC(ZR_SOURCE_STAGE_FRAME,
            ZrParser_ExecIr_AttachPrimitiveSourceFrame(&p.compacted, cs->semanticContext,
                    p.original.layouts, p.original.layoutCount, 0u, &e));
    RETENTION_EXEC(ZR_SOURCE_STAGE_FRAME,
            ZrCore_ExecIr_VerifyFunction(&p.compacted, ZR_EXEC_IR_VERIFY_ALL, &e));
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
    retention_cleanup(&p);
    return success;
#undef RETENTION_EXEC
#undef RETENTION_AOT
}
