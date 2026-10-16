#include "exec_ir/source/exec_ir_source_integer_constants.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include <stdint.h>
#include <string.h>

static TZrBool header(const void *p, size_t width, size_t align) {
    return (TZrBool)(p && (uintptr_t)p % align == 0u && width <= UINTPTR_MAX - (uintptr_t)p);
}

static TZrBool refuse(SZrExecIrDiagnostic *d, EZrExecutionDiagnosticCode code) {
    if (d) d->code = code;
    return ZR_FALSE;
}
static TZrBool eligible(const SZrCompilerState *c, SZrExecIrDiagnostic *d) {
    const SZrFunction *f = c->currentFunction;
    if (!c->state || !c->state->global || !f || !c->semanticContext || c->hasError ||
        !c->currentAst || c->currentAst != c->scriptAst || c->currentFunctionNode ||
        c->topLevelFunction || c->submissionContext || c->submissionEntryFunction ||
        !c->currentModuleKey || !c->preSemanticIrInitialized || !c->preSemanticIrValidated ||
        !c->preSemanticIrCfgActive || !c->preSemanticIrCfgTerminated || f->ownerFunction ||
        f->childFunctionGraphIsBorrowed || f->moduleVersion || f->hasVariableArguments ||
        !f->hasCallableReturnType || f->callableReturnType.baseType != ZR_VALUE_TYPE_INT64 ||
        f->callableReturnType.isNullable || f->callableReturnType.isArray ||
        f->callableReturnType.typeName || f->callableReturnType.ownershipQualifier ||
        !f->hasSourceCallableIdentity || !ZrCore_String_GetByteLength(c->currentModuleKey))
        return refuse(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
#define ZERO(field) if (f->field) return refuse(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED)
    ZERO(parameterCount); ZERO(staticImportLength); ZERO(moduleEntryEffectLength);
    ZERO(childFunctionLength); ZERO(closureValueLength); ZERO(typedClosureBindingLength);
    ZERO(exportedVariableLength); ZERO(typedExportedSymbolLength); ZERO(exportedCallableSummaryLength);
    ZERO(topLevelCallableBindingLength); ZERO(testManifestDataLength);
#undef ZERO
    if (c->closureVars.length) return refuse(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    if (f->functionName != c->currentModuleKey)
        return refuse(d, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    return ZR_TRUE;
}

/* Semantic SymbolId and metadata MemberDef token are separate identities.
 * Use the same unique module/script-entry relationship as the producer, and
 * validate record storage before selecting the actual diagnostic token. */
static TZrBool entry_token(const SZrFunction *f, TZrMetadataToken *output, SZrExecIrDiagnostic *d) {
    const SZrMetadataTokenRecord *records = f->metadataTokenRecords;
    const SZrMetadataTokenRecord *module = ZR_NULL, *entry = ZR_NULL;
    TZrSize count = f->metadataTokenRecordLength;
    if (!count || !records || count > SIZE_MAX / sizeof(*records) ||
        (uintptr_t)records % _Alignof(SZrMetadataTokenRecord) != 0u ||
        count * sizeof(*records) > UINTPTR_MAX - (uintptr_t)records) {
        if (d) { d->expectedHash = 1u; d->actualHash = count; }
        return refuse(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    }
    for (TZrSize i = 0u; i < count; ++i) {
        const SZrMetadataTokenRecord *r = &records[i];
        if (ZR_METADATA_TOKEN_TABLE(r->token) == ZR_METADATA_TABLE_MODULE) {
            if (module) goto mismatch;
            module = r;
        }
        if (ZR_METADATA_TOKEN_TABLE(r->token) == ZR_METADATA_TABLE_MEMBER_DEF &&
            r->reserved0 == ZR_METADATA_TOKEN_RECORD_SCRIPT_ENTRY) {
            if (entry) goto mismatch;
            entry = r;
        }
    }
    if (!module || !entry || !ZR_METADATA_TOKEN_RID(module->token) ||
        !ZR_METADATA_TOKEN_RID(entry->token) || entry->ownerToken != module->token ||
        !f->moduleSignatureHash) goto mismatch;
    *output = entry->token;
    return ZR_TRUE;
mismatch:
    if (d) d->expectedHash = 1u;
    return refuse(d, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH);
}
TZrBool source_integer_constants_prepare(const SZrCompilerState *c,
        SSourceIntegerProof *output, SZrExecIrDiagnostic *d) {
    SSourceIntegerProof candidate = {0}, empty = {0};
    TZrMetadataToken token = 0u;
    if (d) memset(d, 0, sizeof(*d));
    if (!header(c, sizeof(*c), _Alignof(SZrCompilerState)) ||
        !header(output, sizeof(*output), _Alignof(SSourceIntegerProof)))
        return refuse(d, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    if (memcmp(output, &empty, sizeof(empty))) return refuse(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    if (!header(c->currentFunction, sizeof(*c->currentFunction), _Alignof(SZrFunction)) ||
        !header(c->state, sizeof(*c->state), _Alignof(SZrState)) ||
        !header(c->semanticContext, sizeof(*c->semanticContext), _Alignof(SZrSemanticContext)))
        return refuse(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    if (!eligible(c, d)) return ZR_FALSE;
    if (!entry_token(c->currentFunction, &token, d)) return ZR_FALSE;
    if (!source_integer_prove(&c->preSemanticIr, c->semanticContext, token, &candidate, d) ||
        !source_integer_check_ast(&candidate, c->scriptAst,
                &c->currentFunction->sourceCallableIdentity, d) ||
        !source_integer_check_pools(&candidate, &c->constants,
                c->currentFunction->constantValueList, c->currentFunction->constantValueLength, d)) {
        source_integer_proof_free(&candidate); return ZR_FALSE;
    }
    memcpy(output, &candidate, sizeof(candidate)); return ZR_TRUE;
}
