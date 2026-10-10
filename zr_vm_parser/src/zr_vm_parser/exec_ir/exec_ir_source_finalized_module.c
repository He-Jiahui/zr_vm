#include "exec_ir_source_finalized_module_internal.h"

#include <string.h>
#include "zr_vm_core/exec_ir_module_copy.h"
#include "zr_vm_parser/exec_ir_source_frame.h"

#define ERROR(code) source_finalized_module_error(w, code, 0, 0, 0, 0, SOURCE_FINALIZED_NO_ROLE)
#define REQUIRE(condition, code)                                                                                       \
    do {                                                                                                               \
        if (!(condition))                                                                                              \
            return ERROR(code);                                                                                        \
    } while (0)
#define VERSION(actual, expected, code)                                                                                \
    do {                                                                                                               \
        if ((actual) != (expected))                                                                                    \
            return source_finalized_module_error(w, code, expected, actual, 0, 0, SOURCE_FINALIZED_NO_ROLE);           \
    } while (0)
#define HASH(actual, expected, code)                                                                                   \
    do {                                                                                                               \
        if ((actual) != (expected))                                                                                    \
            return source_finalized_module_error(w, code, 0, 0, expected, actual, SOURCE_FINALIZED_NO_ROLE);           \
    } while (0)

static TZrBool token(SSourceFinalizedModuleWitness *w, TZrMetadataToken value, TZrUInt32 table,
                     EZrExecutionDiagnosticCode code) {
    if (!value)
        return source_finalized_module_error(w, code, 1, 0, 0, 0, SOURCE_FINALIZED_NO_ROLE);
    VERSION(ZR_METADATA_TOKEN_TABLE(value), table, code);
    return ZR_TRUE;
}
static TZrBool contract(SSourceFinalizedModuleWitness *w, const SZrExecutionContract *c, TZrMetadataToken target,
                        TZrUInt64 signature, TZrUInt64 generation, TZrBool module) {
    VERSION(c->schemaVersion, ZR_EXECUTION_CONTRACT_SCHEMA_VERSION, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH);
    VERSION(c->abiVersion, ZR_EXECUTION_CONTRACT_ABI_VERSION, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH);
    VERSION(c->logicalVersion, ZR_EXECUTION_CONTRACT_LOGICAL_VERSION, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH);
    VERSION(c->reserved0, 0, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH);
    VERSION(c->reserved1, 0, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH);
    HASH(c->generation, generation, ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION);
    VERSION(c->targetToken, target,
            module ? ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH : ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH);
    if (!signature)
        return source_finalized_module_error(w, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 0, 0, 1, 0,
                                             SOURCE_FINALIZED_NO_ROLE);
    HASH(c->signatureHash, signature, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    HASH(c->moduleHash, w->original->moduleHash, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH);
    VERSION(c->requiredCapabilities, 0, ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH);
    VERSION(c->declaredEffects, 0, ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH);
    return ZR_TRUE;
}
static TZrBool original_contracts(SSourceFinalizedModuleWitness *w) {
    const SZrExecIrModule *m = w->original;
    REQUIRE(m->constantCount == 1 && m->layoutCount == 1 && !m->sourceMapCount, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    if (!token(w, m->moduleToken, ZR_METADATA_TABLE_MODULE, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH))
        return ZR_FALSE;
    if (!m->moduleHash)
        return source_finalized_module_error(w, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH, 0, 0, 1, 0,
                                             SOURCE_FINALIZED_NO_ROLE);
    if (!contract(w, &m->contract, m->moduleToken, m->contract.signatureHash, 1, ZR_TRUE))
        return ZR_FALSE;
    HASH(m->contract.layoutHash, 0, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
    const SZrExecIrFunction *f = w->selected;
    VERSION(f->id, 1, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK);
    if (!token(w, f->functionToken, ZR_METADATA_TABLE_MEMBER_DEF, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH) ||
        !contract(w, &f->contract, f->functionToken, f->signatureHash, m->contract.generation, ZR_FALSE))
        return ZR_FALSE;
    HASH(f->contract.layoutHash, 0, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
    REQUIRE(!f->sealed, ZR_EXEC_IR_DIAGNOSTIC_SEALED);
    REQUIRE(!f->frameLayout, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    return source_finalized_module_metadata(w, f) && source_finalized_module_metadata(w, w->finalized);
}
static TZrBool signatures(SSourceFinalizedModuleWitness *w, const SZrExecIrFunction *f) {
    HASH(f->signatureHash, w->facts.signatureHash, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    HASH(f->contract.signatureHash, w->facts.signatureHash, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    return ZR_TRUE;
}
static TZrBool constants(SSourceFinalizedModuleWitness *w) {
    size_t count = w->compilerConstants->length;
    if (count > UINT32_MAX)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0, 0, UINT32_MAX, count,
                                             SOURCE_FINALIZED_NO_ROLE);
    if (count != (uintmax_t) w->functionConstantCount)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0, 0, count,
                                             w->functionConstantCount, SOURCE_FINALIZED_NO_ROLE);
    if (count != 1 || count != w->original->constantCount)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0, 0, 1, count,
                                             SOURCE_FINALIZED_NO_ROLE);
    const SZrSemanticIrInstruction *semantic = (const SZrSemanticIrInstruction *) w->semantic->instructions.head;
    TZrUInt32 index = semantic[0].constantPoolIndex;
    TZrUInt32 bounds[3] = {(TZrUInt32) count, w->functionConstantCount, w->original->constantCount};
    for (unsigned i = 0; i < 3; ++i)
        if (index >= bounds[i])
            return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, bounds[i], index, 0, 0, 0);
    const SZrExecIrConstant *constant = &w->original->constants[index];
    if (constant->typeToken != w->facts.returnTypeId)
        return source_finalized_module_error(w, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, w->facts.returnTypeId,
                                             constant->typeToken, 0, 0, 0);
    if (constant->flags)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0, constant->flags, 0, 0, 0);
    const SZrTypeValue *compiler = (const SZrTypeValue *) w->compilerConstants->head, *runtime = w->functionConstants;
    if (compiler[index].type != ZR_VALUE_TYPE_INT64)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, ZR_VALUE_TYPE_INT64,
                                             compiler[index].type, 0, 0, 0);
    if (runtime[index].type != ZR_VALUE_TYPE_INT64)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, ZR_VALUE_TYPE_INT64,
                                             runtime[index].type, 0, 0, 0);
    TZrUInt64 bits = (TZrUInt64) compiler[index].value.nativeObject.nativeInt64;
    TZrUInt64 actual = (TZrUInt64) runtime[index].value.nativeObject.nativeInt64;
    if (bits != actual)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 0, 0, bits, actual, 0);
    if (bits != constant->bits)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 0, 0, bits, constant->bits, 0);
    const SZrExecIrLayout *layout = &w->original->layouts[0];
    if (!layout->id)
        return source_finalized_module_error(w, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, 1, 0, 0, 0,
                                             SOURCE_FINALIZED_NO_ROLE);
    VERSION(layout->typeToken, w->facts.returnTypeId, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    if (!layout->layoutHash)
        return source_finalized_module_error(w, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, 0, 0, 1, 0,
                                             SOURCE_FINALIZED_NO_ROLE);
    if (!layout->byteSize)
        return source_finalized_module_error(w, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, 1, 0, 0, 0,
                                             SOURCE_FINALIZED_NO_ROLE);
    if (!layout->byteAlign)
        return source_finalized_module_error(w, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, 1, 0, 0, 0,
                                             SOURCE_FINALIZED_NO_ROLE);
    if (layout->byteAlign & (layout->byteAlign - 1))
        return source_finalized_module_error(w, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, 0, layout->byteAlign, 0, 0,
                                             SOURCE_FINALIZED_NO_ROLE);
    return ZR_TRUE;
}
static TZrBool candidate(SSourceFinalizedModuleWitness *w, const SZrExecIrModule *m) {
    if (!source_finalized_module_candidate_storage(w, m) || !ZrCore_ExecIr_VerifyModule(m, w->diagnostic))
        return ZR_FALSE;
    REQUIRE(m->id == w->original->id && m->moduleToken == w->original->moduleToken &&
                    m->moduleHash == w->original->moduleHash,
            ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    REQUIRE(m->constantCount == 1 && m->layoutCount == 1 && !m->sourceMapCount, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    if (!contract(w, &m->contract, m->moduleToken, w->original->contract.signatureHash, 1, ZR_TRUE))
        return ZR_FALSE;
    HASH(m->contract.layoutHash, 0, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
    const SZrExecIrFunction *f = &m->functions[0];
    VERSION(f->id, 1, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK);
    VERSION(f->functionToken, w->token, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH);
    if (!contract(w, &f->contract, f->functionToken, w->facts.signatureHash, m->contract.generation, ZR_FALSE) ||
        !signatures(w, f))
        return ZR_FALSE;
    REQUIRE(f->sealed == w->finalized->sealed, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    const SZrExecIrConstant *a = &w->original->constants[0], *b = &m->constants[0];
    if (a->typeToken != b->typeToken)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, a->typeToken, b->typeToken, 0, 0,
                                             0);
    if (a->flags != b->flags)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, a->flags, b->flags, 0, 0, 0);
    if (a->bits != b->bits)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 0, 0, a->bits, b->bits, 0);
    const SZrExecIrLayout *la = &w->original->layouts[0], *lb = &m->layouts[0];
    VERSION(lb->id, la->id, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
    VERSION(lb->typeToken, la->typeToken, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
    VERSION(lb->byteSize, la->byteSize, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
    VERSION(lb->byteAlign, la->byteAlign, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
    HASH(lb->layoutHash, la->layoutHash, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
    if (!source_finalized_module_relation(w, f))
        return ZR_FALSE;
    REQUIRE(f->frameLayout != ZR_NULL, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    HASH(f->frameLayout->layoutHash, w->finalized->frameLayout->layoutHash, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
    return ZrParser_ExecIr_ValidatePrimitiveSourceFrame(f, w->context, m->layouts, m->layoutCount, 0, w->diagnostic);
}

TZrBool source_snapshot_finalized_primitive_module(
        const SZrSemanticIrFunction *semantic, const SZrSemanticContext *context, const SZrAstNode *script,
        const SZrFunctionSourceCallableIdentity *sourceIdentity, const SZrArray *compilerConstants,
        const SZrTypeValue *functionConstants, TZrUInt32 functionConstantCount, const SZrExecIrModule *original,
        const SZrExecIrFunction *finalized, SZrExecIrOwnedModule **destination, SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL)
        memset(diagnostic, 0, sizeof(*diagnostic));
    if (semantic == ZR_NULL || context == ZR_NULL || script == ZR_NULL || sourceIdentity == ZR_NULL ||
        compilerConstants == ZR_NULL || original == ZR_NULL || finalized == ZR_NULL || destination == ZR_NULL)
        return source_finalized_module_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    SSourceFinalizedModuleWitness witness = {0};
    SSourceFinalizedModuleWitness *w = &witness;
    w->semantic = semantic;
    w->context = context;
    w->script = script;
    w->identity = sourceIdentity;
    w->compilerConstants = compilerConstants;
    w->functionConstants = functionConstants;
    w->functionConstantCount = functionConstantCount;
    w->original = original;
    w->finalized = finalized;
    w->destination = destination;
    w->diagnostic = diagnostic;
    if (!source_finalized_module_preflight(w) || !original_contracts(w) ||
        !ZrCore_ExecIr_VerifyModule(original, diagnostic) || !source_finalized_module_admit_source(w) ||
        !signatures(w, w->selected) || !signatures(w, finalized) || !source_finalized_module_mapping(w) ||
        !constants(w) || !source_finalized_module_relation(w, finalized) ||
        !ZrParser_ExecIr_ValidatePrimitiveSourceFrame(finalized, context, original->layouts, original->layoutCount, 0,
                                                      diagnostic))
        return ZR_FALSE;
    SZrExecIrModule temporary;
    ZrCore_ExecIr_ModuleInit(&temporary);
    TZrBool success = ZR_FALSE;
    if (!ZrCore_ExecIr_CloneModuleReplacingFunction(original, 1, finalized, &temporary, diagnostic))
        goto cleanup;
    if (!candidate(w, &temporary))
        goto cleanup;
    if (!ZrCore_ExecIr_CloneOwnedModule(&temporary, destination, diagnostic))
        goto cleanup;
    success = ZR_TRUE;
cleanup:
    ZrCore_ExecIr_FreeModule(&temporary);
    return success;
}
