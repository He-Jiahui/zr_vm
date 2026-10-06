#include "zr_vm_parser/exec_ir_source_module_contract.h"

#include "zr_vm_parser/canonical_type.h"
#include "../compiler/compiler_metadata_signature.h"
#include "../compiler/compiler_script_entry_metadata.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/zrp_metadata.h"

#include <stdint.h>
#include <string.h>

/* Definition-bound current MODULE and noargs primitive METHOD_SIG wire facts.
 * These decode the existing writer's bytes; they introduce no hash domain. */
enum {
    SOURCE_CONTRACT_INITIAL_GENERATION = 1,
    SOURCE_MODULE_SINGLE_ENTRY_ID = 1,
    SOURCE_SIGNATURE_U32_BYTES = 4,
    SOURCE_SIGNATURE_OCTET_BITS = 8,
    SOURCE_MODULE_NAME_OFFSET = 1,
    SOURCE_MODULE_VERSION_OFFSET = SOURCE_MODULE_NAME_OFFSET + SOURCE_SIGNATURE_U32_BYTES,
    SOURCE_MODULE_SIGNATURE_BYTES = SOURCE_MODULE_VERSION_OFFSET + SOURCE_SIGNATURE_U32_BYTES,
    SOURCE_METHOD_SCHEMA_OFFSET = 1,
    SOURCE_METHOD_ARITY_OFFSET = 2,
    SOURCE_METHOD_RESERVED_OFFSET = 3,
    SOURCE_METHOD_RETURN_NODE_OFFSET = SOURCE_METHOD_RESERVED_OFFSET + SOURCE_SIGNATURE_U32_BYTES,
    SOURCE_METHOD_RETURN_TYPE_OFFSET = SOURCE_METHOD_RETURN_NODE_OFFSET + 1,
    SOURCE_METHOD_PARAMETER_OFFSET = SOURCE_METHOD_RETURN_TYPE_OFFSET + SOURCE_SIGNATURE_U32_BYTES,
    SOURCE_METHOD_SIGNATURE_BYTES = SOURCE_METHOD_PARAMETER_OFFSET + SOURCE_SIGNATURE_U32_BYTES,
    SOURCE_METHOD_SCHEMA = 1
};

typedef struct SSourceModuleRows {
    const SZrMetadataTokenRecord *module;
    const SZrMetadataTokenRecord *entry;
    TZrUInt64 moduleBlobHash;
} SSourceModuleRows;

static TZrBool source_contract_fail(SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code, TZrUInt64 expected, TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->code = code;
        diagnostic->expectedHash = expected;
        diagnostic->actualHash = actual;
    }
    return ZR_FALSE;
}

static TZrBool source_contract_version(SZrExecIrDiagnostic *diagnostic,
        TZrUInt32 expected, TZrUInt32 actual, TZrBool allowAbsent) {
    if (actual == expected || (allowAbsent && actual == 0U)) return ZR_TRUE;
    if (diagnostic != ZR_NULL) {
        diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH;
        diagnostic->expectedVersion = expected;
        diagnostic->actualVersion = actual;
    }
    return ZR_FALSE;
}

/* Counts and address arithmetic only; readable independent owners remain a
 * caller precondition. Check capacity as well as length before array lookup. */
static TZrBool source_contract_pool(const void *head, TZrSize length,
        TZrSize capacity, TZrSize width, SZrExecIrDiagnostic *diagnostic) {
    TZrSize span;
    uintptr_t start = (uintptr_t)head;
    if (length > capacity)
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                capacity, length);
    if (capacity != 0U && head == ZR_NULL)
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                ZR_TRUE, ZR_FALSE);
    if (width == 0U)
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                ZR_TRUE, width);
    if (capacity > SIZE_MAX / width)
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                SIZE_MAX / width, capacity);
    span = capacity * width;
    if ((uintmax_t)span > (uintmax_t)UINTPTR_MAX)
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                UINTPTR_MAX, span);
    if (start > UINTPTR_MAX - (uintptr_t)span)
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                UINTPTR_MAX - (uintptr_t)span, start);
    return ZR_TRUE;
}

static TZrBool source_contract_array(const SZrArray *array, TZrSize width,
        SZrExecIrDiagnostic *diagnostic) {
    if (!array->isValid)
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                ZR_TRUE, array->isValid);
    if (array->elementSize != width)
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                width, array->elementSize);
    return source_contract_pool(array->head, array->length, array->capacity,
            width, diagnostic);
}

static TZrBool source_contract_storage(const SZrFunction *source,
        const SZrExecIrModule *module, SZrExecIrDiagnostic *diagnostic) {
#define SOURCE_POOL(field, count) \
    if (!source_contract_pool(source->field, source->count, source->count, \
            sizeof(*source->field), diagnostic)) return ZR_FALSE
    SOURCE_POOL(metadataTokenRecords, metadataTokenRecordLength);
    SOURCE_POOL(moduleMetadataTokenRecords, moduleMetadataTokenRecordLength);
    SOURCE_POOL(signatureBlobHeap, signatureBlobHeapLength);
    SOURCE_POOL(metadataStringHeap, metadataStringHeapLength);
#undef SOURCE_POOL
#define MODULE_POOL(field, count, capacity) \
    if (!source_contract_pool(module->field, module->count, module->capacity, \
            sizeof(*module->field), diagnostic)) return ZR_FALSE
    MODULE_POOL(functions, functionCount, functionCapacity);
    MODULE_POOL(constants, constantCount, constantCapacity);
    MODULE_POOL(layouts, layoutCount, layoutCapacity);
    MODULE_POOL(sourceMaps, sourceMapCount, sourceMapCapacity);
#undef MODULE_POOL
    /* All source record Blob bounds are checked before following any pair. */
    for (TZrUInt32 i = 0U; i < source->metadataTokenRecordLength; ++i) {
        const SZrMetadataTokenRecord *row = &source->metadataTokenRecords[i];
        if (row->signatureBlobOffset > source->signatureBlobHeapLength)
            return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                    source->signatureBlobHeapLength, row->signatureBlobOffset);
        if (row->signatureBlobLength > source->signatureBlobHeapLength - row->signatureBlobOffset)
            return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                    source->signatureBlobHeapLength - row->signatureBlobOffset, row->signatureBlobLength);
    }
    return ZR_TRUE;
}

static TZrBool source_contract_eligible(const SZrCompilerState *compiler,
        const SZrFunction *source, SZrExecIrDiagnostic *diagnostic) {
    if (compiler->state == ZR_NULL || compiler->state->global == ZR_NULL || compiler->hasError ||
        compiler->currentAst == ZR_NULL || compiler->currentAst->type != ZR_AST_SCRIPT ||
        compiler->currentAst != compiler->scriptAst || compiler->currentFunctionNode != ZR_NULL ||
        compiler->topLevelFunction != ZR_NULL || compiler->submissionContext != ZR_NULL ||
        compiler->submissionEntryFunction != ZR_NULL || compiler->currentModuleKey == ZR_NULL ||
        compiler->semanticContext == ZR_NULL || !compiler->preSemanticIrInitialized ||
        !compiler->preSemanticIrValidated || !compiler->preSemanticIrCfgActive ||
        !compiler->preSemanticIrCfgTerminated || source->moduleVersion != ZR_NULL ||
        source->hasVariableArguments || !source->hasCallableReturnType ||
        source->callableReturnType.baseType != ZR_VALUE_TYPE_INT64 ||
        source->callableReturnType.isNullable || source->callableReturnType.isArray ||
        source->callableReturnType.typeName != ZR_NULL ||
        source->callableReturnType.ownershipQualifier != 0U || !source->hasSourceCallableIdentity)
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                ZR_TRUE, ZR_FALSE);
#define SOURCE_ZERO(field) \
    if (source->field != 0U) \
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0U, source->field)
    SOURCE_ZERO(parameterCount);
    SOURCE_ZERO(staticImportLength);
    SOURCE_ZERO(moduleEntryEffectLength);
    SOURCE_ZERO(childFunctionLength);
    SOURCE_ZERO(closureValueLength);
    SOURCE_ZERO(typedClosureBindingLength);
    SOURCE_ZERO(exportedVariableLength);
    SOURCE_ZERO(typedExportedSymbolLength);
    SOURCE_ZERO(exportedCallableSummaryLength);
    SOURCE_ZERO(topLevelCallableBindingLength);
#undef SOURCE_ZERO
    if (compiler->closureVars.length != 0U)
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                0U, compiler->closureVars.length);
    if (source->functionName != compiler->currentModuleKey)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
                ZR_TRUE, ZR_FALSE);
    if (ZrCore_String_GetByteLength(source->functionName) == 0U)
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                ZR_TRUE, 0U);
    if (source->moduleSignatureHash == 0U)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
                ZR_TRUE, 0U);
    return ZR_TRUE;
}

static TZrBool source_contract_record_binding(const SZrMetadataTokenRecord *row,
        SZrExecIrDiagnostic *diagnostic) {
#define RECORD_ZERO(field) \
    if (row->field != 0U) \
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH, 0U, row->field)
    RECORD_ZERO(targetMetadataToken);
    RECORD_ZERO(targetSignatureToken);
    RECORD_ZERO(targetSignatureHash);
    RECORD_ZERO(targetModuleSignatureHash);
    RECORD_ZERO(layoutHash);
#undef RECORD_ZERO
    if (row->requestedModuleVersion != ZR_NULL || row->minModuleVersionInclusive != ZR_NULL ||
        row->maxModuleVersionExclusive != ZR_NULL)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH, ZR_FALSE, ZR_TRUE);
    return ZR_TRUE;
}

static TZrUInt32 source_signature_u32(const TZrByte *bytes) {
    TZrUInt32 value = 0U;
    for (TZrUInt32 i = 0U; i < SOURCE_SIGNATURE_U32_BYTES; ++i)
        value |= (TZrUInt32)bytes[i] << (i * SOURCE_SIGNATURE_OCTET_BITS);
    return value;
}

static TZrBool source_contract_pair(const SZrFunction *source,
        const SZrMetadataTokenRecord *row, TZrUInt32 marker,
        TZrUInt64 *hash, SZrExecIrDiagnostic *diagnostic) {
    const SZrMetadataTokenRecord *pair = ZR_NULL;
    const TZrByte *blob;
    if (ZR_METADATA_TOKEN_TABLE(row->relatedToken) != ZR_METADATA_TABLE_SIGNATURE)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                ZR_METADATA_TABLE_SIGNATURE, ZR_METADATA_TOKEN_TABLE(row->relatedToken));
    if (ZR_METADATA_TOKEN_RID(row->relatedToken) == 0U)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                ZR_TRUE, 0U);
    for (TZrUInt32 i = 0U; i < source->metadataTokenRecordLength; ++i)
        if (source->metadataTokenRecords[i].token == row->relatedToken) {
            if (pair != ZR_NULL)
                return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH, 1U, 2U);
            pair = &source->metadataTokenRecords[i];
        }
    if (pair == ZR_NULL)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                row->relatedToken, 0U);
#define PAIR_EQUAL(field, expected) \
    if (pair->field != (expected)) \
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, \
                (expected), pair->field)
    PAIR_EQUAL(relatedToken, row->token);
    PAIR_EQUAL(ownerToken, row->token);
    PAIR_EQUAL(ownerIndex, row->ownerIndex);
    PAIR_EQUAL(signatureBlobOffset, row->signatureBlobOffset);
    PAIR_EQUAL(signatureBlobLength, row->signatureBlobLength);
    PAIR_EQUAL(signatureHash, row->signatureHash);
#undef PAIR_EQUAL
    if (!source_contract_version(diagnostic, marker, row->reserved0, ZR_FALSE) ||
        !source_contract_version(diagnostic, marker, pair->reserved0, ZR_FALSE) ||
        !source_contract_version(diagnostic, 0U, row->layoutVersion, ZR_FALSE) ||
        !source_contract_version(diagnostic, 0U, pair->layoutVersion, ZR_FALSE)) return ZR_FALSE;
    if (!source_contract_record_binding(row, diagnostic) ||
        !source_contract_record_binding(pair, diagnostic)) return ZR_FALSE;
    if (row->signatureBlobLength == 0U)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, ZR_TRUE, 0U);
    blob = source->signatureBlobHeap + row->signatureBlobOffset;
    if (!ZrCore_ZrpMetadata_ValidateSignatureBlob(blob, row->signatureBlobLength))
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                ZR_TRUE, ZR_FALSE);
    *hash = metadata_signature_hash_v1(blob, row->signatureBlobLength);
    if (*hash == 0U)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                row->signatureHash != 0U ? row->signatureHash : ZR_TRUE, 0U);
    if (row->signatureHash != *hash)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                *hash, row->signatureHash);
    return ZR_TRUE;
}

static TZrBool source_contract_rows(const SZrFunction *source,
        SSourceModuleRows *rows, SZrExecIrDiagnostic *diagnostic) {
    TZrUInt64 entryBlobHash;
    const TZrByte *blob;
    TZrUInt32 nameIndex;
    const SZrMetadataStringHeapEntry *name = ZR_NULL;
    memset(rows, 0, sizeof(*rows));
    for (TZrUInt32 i = 0U; i < source->metadataTokenRecordLength; ++i) {
        const SZrMetadataTokenRecord *row = &source->metadataTokenRecords[i];
        if (ZR_METADATA_TOKEN_RID(row->token) == 0U)
            return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
                    ZR_TRUE, ZR_METADATA_TOKEN_RID(row->token));
        for (TZrUInt32 j = 0U; j < i; ++j)
            if (source->metadataTokenRecords[j].token == row->token)
                return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH, 1U, 2U);
        if (ZR_METADATA_TOKEN_TABLE(row->token) == ZR_METADATA_TABLE_MODULE) {
            if (rows->module != ZR_NULL)
                return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH, 1U, 2U);
            rows->module = row;
        }
        if (ZR_METADATA_TOKEN_TABLE(row->token) == ZR_METADATA_TABLE_MEMBER_DEF) {
            if (rows->entry != ZR_NULL)
                return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH, 1U, 2U);
            rows->entry = row;
        }
    }
    if (rows->module == ZR_NULL || rows->entry == ZR_NULL)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH, ZR_TRUE, ZR_FALSE);
    if (rows->module->ownerToken != 0U || rows->module->ownerIndex != 0U)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
                0U, rows->module->ownerToken != 0U ? rows->module->ownerToken : rows->module->ownerIndex);
    if (rows->entry->ownerToken != rows->module->token)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
                rows->module->token, rows->entry->ownerToken);
    if (rows->entry->ownerIndex != ZR_CALL_BINDING_SLOT_NONE)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
                ZR_CALL_BINDING_SLOT_NONE, rows->entry->ownerIndex);
    if (!source_contract_pair(source, rows->module, 0U, &rows->moduleBlobHash, diagnostic) ||
        !source_contract_pair(source, rows->entry, ZR_METADATA_TOKEN_RECORD_SCRIPT_ENTRY,
                &entryBlobHash, diagnostic)) return ZR_FALSE;

    /* MODULE indexes refer to the actual source string heap, not token RIDs. */
    blob = source->signatureBlobHeap + rows->module->signatureBlobOffset;
    if (rows->module->signatureBlobLength != SOURCE_MODULE_SIGNATURE_BYTES)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                SOURCE_MODULE_SIGNATURE_BYTES, rows->module->signatureBlobLength);
    if (blob[0] != ZR_METADATA_SIGNATURE_NODE_MODULE)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                ZR_METADATA_SIGNATURE_NODE_MODULE, blob[0]);
    if (source_signature_u32(blob + SOURCE_MODULE_VERSION_OFFSET) != 0U)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                0U, source_signature_u32(blob + SOURCE_MODULE_VERSION_OFFSET));
    nameIndex = source_signature_u32(blob + SOURCE_MODULE_NAME_OFFSET);
    for (TZrUInt32 i = 0U; i < source->metadataStringHeapLength; ++i)
        if (source->metadataStringHeap[i].stringIndex == nameIndex && nameIndex != 0U) {
            if (name != ZR_NULL)
                return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH, 1U, 2U);
            name = &source->metadataStringHeap[i];
        }
    if (name == ZR_NULL || name->value == ZR_NULL ||
        !ZrCore_String_Equal(name->value, source->functionName))
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH, ZR_TRUE, ZR_FALSE);

    /* Noargs INT64 writer frame: schema=1, arity=0, reserved=0, primitive return.
     * ValidateSignatureBlob above supplies structural validation first. */
    blob = source->signatureBlobHeap + rows->entry->signatureBlobOffset;
    if (rows->entry->signatureBlobLength != SOURCE_METHOD_SIGNATURE_BYTES)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                SOURCE_METHOD_SIGNATURE_BYTES, rows->entry->signatureBlobLength);
    if (blob[0] != ZR_METADATA_SIGNATURE_NODE_METHOD_SIG ||
        blob[SOURCE_METHOD_SCHEMA_OFFSET] != SOURCE_METHOD_SCHEMA ||
        blob[SOURCE_METHOD_ARITY_OFFSET] != 0U ||
        source_signature_u32(blob + SOURCE_METHOD_RESERVED_OFFSET) != 0U ||
        blob[SOURCE_METHOD_RETURN_NODE_OFFSET] != ZR_METADATA_SIGNATURE_NODE_PRIMITIVE ||
        source_signature_u32(blob + SOURCE_METHOD_RETURN_TYPE_OFFSET) != ZR_VALUE_TYPE_INT64 ||
        source_signature_u32(blob + SOURCE_METHOD_PARAMETER_OFFSET) != 0U)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, ZR_TRUE, ZR_FALSE);
    return ZR_TRUE;
}

static TZrBool source_contract_canonical(const SZrCompilerState *compiler,
        const SZrFunction *source, TZrUInt64 *canonicalHash, SZrExecIrDiagnostic *diagnostic) {
    const SZrSemanticContext *context = compiler->semanticContext;
    const SZrFunctionSourceCallableIdentity *identity = &source->sourceCallableIdentity;
    const SZrCanonicalTypeNode *nodes;
    const SZrCanonicalTypeNode *callable, *returnNode;
    const SZrSemanticSymbolRecord *symbol = ZR_NULL;
    const SZrSemanticSymbolRecord *symbols;
    if (!source_contract_array(&context->canonicalTypes, sizeof(SZrCanonicalTypeNode), diagnostic) ||
        !source_contract_array(&context->symbols, sizeof(SZrSemanticSymbolRecord), diagnostic)) return ZR_FALSE;
    if (compiler->preSemanticIr.state != compiler->state || context->state != compiler->state)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, ZR_TRUE, ZR_FALSE);
    nodes = (const SZrCanonicalTypeNode *)context->canonicalTypes.head;
    for (TZrSize i = 0U; i < context->canonicalTypes.length; ++i)
        if (nodes[i].id == ZR_SEMANTIC_ID_INVALID || (i != 0U && nodes[i].id <= nodes[i - 1U].id))
            return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                    i != 0U ? nodes[i - 1U].id + (TZrUInt64)1U : ZR_TRUE, nodes[i].id);
    if (!source_contract_version(diagnostic, ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1,
            identity->schemaVersion, ZR_FALSE)) return ZR_FALSE;
    if (identity->typeId != compiler->preSemanticIr.callableTypeId ||
        identity->symbolId != compiler->preSemanticIr.symbolId || identity->typeId == ZR_SEMANTIC_ID_INVALID ||
        identity->symbolId == ZR_SEMANTIC_ID_INVALID)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, ZR_TRUE, ZR_FALSE);
    callable = ZrParser_CanonicalType_Find(context, identity->typeId);
    if (callable == ZR_NULL || callable->kind != ZR_CANONICAL_TYPE_FUNCTION || callable->structuralHash == 0U)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, ZR_TRUE, ZR_FALSE);
    if (!source_contract_array(&callable->data.function.parameterContracts,
            sizeof(SZrCanonicalParameterContract), diagnostic)) return ZR_FALSE;
    if (callable->data.function.parameterContracts.length != 0U ||
        callable->data.function.receiverEffect != ZR_CANONICAL_RECEIVER_NONE ||
        callable->data.function.effectFlags != ZR_CANONICAL_CALLABLE_EFFECT_NONE)
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, ZR_TRUE, ZR_FALSE);
    returnNode = ZrParser_CanonicalType_Find(context, callable->data.function.returnTypeId);
    if (returnNode == ZR_NULL || returnNode->kind != ZR_CANONICAL_TYPE_PRIMITIVE ||
        returnNode->data.primitive.valueType != ZR_VALUE_TYPE_INT64)
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, ZR_TRUE, ZR_FALSE);
    if (identity->canonicalSignatureHash != callable->structuralHash)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                callable->structuralHash, identity->canonicalSignatureHash);
    if (!identity->hasExplicitNoArgsI64 || identity->returnPrimitive != ZR_VALUE_TYPE_INT64 ||
        identity->parameterCount != 0U || identity->receiverFlags != ZR_CANONICAL_RECEIVER_NONE ||
        identity->effectFlags != ZR_CANONICAL_CALLABLE_EFFECT_NONE)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, ZR_TRUE, ZR_FALSE);
    symbols = (const SZrSemanticSymbolRecord *)context->symbols.head;
    for (TZrSize i = 0U; i < context->symbols.length; ++i)
        if (symbols[i].id == identity->symbolId) {
            if (symbol != ZR_NULL)
                return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 1U, 2U);
            symbol = &symbols[i];
        }
    if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        symbol->typeId != identity->typeId || symbol->name != compiler->currentModuleKey ||
        symbol->astNode != compiler->currentAst || symbol->overloadSetId != ZR_SEMANTIC_ID_INVALID)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, ZR_TRUE, ZR_FALSE);
#define SOURCE_RANGE(field, coordinate) \
    if ((TZrInt64)identity->declarationRange.field != (TZrInt64)compiler->currentAst->location.coordinate) \
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, \
                compiler->currentAst->location.coordinate, identity->declarationRange.field); \
    if (symbol->location.coordinate != compiler->currentAst->location.coordinate) \
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, \
                compiler->currentAst->location.coordinate, symbol->location.coordinate)
    SOURCE_RANGE(startLine, start.line);
    SOURCE_RANGE(startColumn, start.column);
    SOURCE_RANGE(endLine, end.line);
    SOURCE_RANGE(endColumn, end.column);
#undef SOURCE_RANGE
    *canonicalHash = callable->structuralHash;
    return ZR_TRUE;
}

static TZrBool source_contract_existing(const SZrExecutionContract *actual,
        const SZrExecutionContract *expected, TZrBool allowAbsent,
        SZrExecIrDiagnostic *diagnostic) {
    if (!source_contract_version(diagnostic, expected->schemaVersion, actual->schemaVersion, allowAbsent) ||
        !source_contract_version(diagnostic, expected->abiVersion, actual->abiVersion, allowAbsent) ||
        !source_contract_version(diagnostic, expected->logicalVersion, actual->logicalVersion, allowAbsent) ||
        !source_contract_version(diagnostic, 0U, actual->reserved0, ZR_FALSE) ||
        !source_contract_version(diagnostic, 0U, actual->reserved1, ZR_FALSE)) return ZR_FALSE;
#define CONTRACT_EQUAL(field, code) \
    if (actual->field != expected->field && !(allowAbsent && actual->field == 0U)) \
        return source_contract_fail(diagnostic, code, expected->field, actual->field)
    CONTRACT_EQUAL(generation, ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION);
    CONTRACT_EQUAL(targetToken, allowAbsent ? ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH : \
            ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH);
    CONTRACT_EQUAL(signatureHash, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    CONTRACT_EQUAL(requiredCapabilities, ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH);
    CONTRACT_EQUAL(declaredEffects, ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH);
    /* Function moduleHash alone may be absent; no other original identity is repaired. */
    if (actual->moduleHash != 0U && actual->moduleHash != expected->moduleHash)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
                expected->moduleHash, actual->moduleHash);
    if (allowAbsent) { CONTRACT_EQUAL(layoutHash, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH); }
#undef CONTRACT_EQUAL
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_BindSourceModuleContract(
        const SZrCompilerState *compiler, SZrExecIrModule *module,
        SZrExecIrDiagnostic *diagnostic) {
    const SZrFunction *source;
    SZrExecIrFunction *function;
    SSourceModuleRows rows;
    SZrExecutionContract candidate, functionExpected;
    TZrUInt64 canonicalHash, moduleHash;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (compiler == ZR_NULL || module == ZR_NULL)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, ZR_TRUE, ZR_FALSE);
    source = compiler->currentFunction;
    if (source == ZR_NULL)
        return source_contract_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, ZR_TRUE, ZR_FALSE);
    if (!source_contract_storage(source, module, diagnostic) ||
        !source_contract_eligible(compiler, source, diagnostic)) return ZR_FALSE;
    if (module->functionCount != 1U)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH, 1U, module->functionCount);
    if (!source_contract_rows(source, &rows, diagnostic)) return ZR_FALSE;
    if (diagnostic != ZR_NULL) diagnostic->functionToken = rows.entry->token;
    if (module->moduleToken != rows.module->token)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
                rows.module->token, module->moduleToken);
    if (!source_contract_canonical(compiler, source, &canonicalHash, diagnostic)) return ZR_FALSE;
    function = &module->functions[0];
    if (function->functionToken != rows.entry->token)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
                rows.entry->token, function->functionToken);
    if (function->id != SOURCE_MODULE_SINGLE_ENTRY_ID)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
                SOURCE_MODULE_SINGLE_ENTRY_ID, function->id);
    if (function->signatureHash != canonicalHash)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                canonicalHash, function->signatureHash);
    moduleHash = compiler_script_entry_metadata_hash(compiler, source);
    if (moduleHash == 0U)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                source->moduleSignatureHash, 0U);
    if (source->moduleSignatureHash != moduleHash)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
                moduleHash, source->moduleSignatureHash);
    if (module->moduleHash != moduleHash)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
                moduleHash, module->moduleHash);
    memset(&candidate, 0, sizeof(candidate));
    candidate.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    candidate.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    candidate.logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    if (function->contract.generation != SOURCE_CONTRACT_INITIAL_GENERATION)
        return source_contract_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION,
                SOURCE_CONTRACT_INITIAL_GENERATION, function->contract.generation);
    candidate.generation = function->contract.generation;
    candidate.targetToken = rows.module->token;
    candidate.signatureHash = rows.moduleBlobHash;
    candidate.moduleHash = moduleHash;
    functionExpected = candidate;
    functionExpected.targetToken = rows.entry->token;
    functionExpected.signatureHash = canonicalHash;
    if (!source_contract_existing(&function->contract, &functionExpected, ZR_FALSE, diagnostic) ||
        !source_contract_existing(&module->contract, &candidate, ZR_TRUE, diagnostic)) return ZR_FALSE;
    /* No fallible operation remains. Publish only these two authorized fields. */
    module->contract = candidate;
    function->contract.moduleHash = moduleHash;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    return ZR_TRUE;
}
