#include "zr_vm_core/function_identity.h"

#include <string.h>

#include "zr_vm_core/string.h"

static TZrBool same_string(SZrString *left, SZrString *right) {
    return left == right || (left != ZR_NULL && right != ZR_NULL && ZrCore_String_Equal(left, right));
}

static TZrBool same_constant_pool(const SZrFunction *left, const SZrFunction *right) {
    if (left->constantValueLength != right->constantValueLength) return ZR_FALSE;
    if (left->constantValueLength == 0u) return ZR_TRUE;
    if (left->constantValueList == ZR_NULL || right->constantValueList == ZR_NULL) return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < left->constantValueLength; ++index) {
        const SZrTypeValue *a = &left->constantValueList[index];
        const SZrTypeValue *b = &right->constantValueList[index];
        /* Function constants are graph edges. Their addresses differ before
         * child aliases are published, so compare their containing bodies
         * through the instruction stream and leave the edge for the explicit
         * alias/rebind pass. Literal constants still participate in identity;
         * this prevents two same-shaped functions returning different values
         * from being treated as the same child. */
        if ((a->type == ZR_VALUE_TYPE_FUNCTION || a->type == ZR_VALUE_TYPE_CLOSURE) &&
            (b->type == ZR_VALUE_TYPE_FUNCTION || b->type == ZR_VALUE_TYPE_CLOSURE)) {
            continue;
        }
        if (!ZrCore_Value_CompareDirectly(ZR_NULL, a, b)) return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Function_HasSameDefinition(const SZrFunction *left, const SZrFunction *right) {
    if (left == ZR_NULL || right == ZR_NULL) return ZR_FALSE;
    if (left == right) return ZR_TRUE;
    if (left->instructionsList != ZR_NULL && right->instructionsList != ZR_NULL &&
        left->instructionsLength == right->instructionsLength &&
        memcmp(left->instructionsList, right->instructionsList,
               (TZrSize)left->instructionsLength * sizeof(*left->instructionsList)) != 0) return ZR_FALSE;
    if (left->instructionsList != ZR_NULL && left->instructionsList == right->instructionsList &&
        left->constantValueList == right->constantValueList) return ZR_TRUE;
    if (!same_constant_pool(left, right)) return ZR_FALSE;
    if (!same_string(left->functionName, right->functionName) ||
        !same_string(left->sourceCodeList, right->sourceCodeList) ||
        !same_string(left->sourceHash, right->sourceHash) ||
        left->parameterCount != right->parameterCount || left->instructionsLength != right->instructionsLength ||
        left->lineInSourceStart != right->lineInSourceStart || left->lineInSourceEnd != right->lineInSourceEnd ||
        left->executionLocationInfoLength == 0u ||
        left->executionLocationInfoLength != right->executionLocationInfoLength ||
        left->executionLocationInfoList == ZR_NULL || right->executionLocationInfoList == ZR_NULL) return ZR_FALSE;
    /* Before metadata publication, source spans distinguish same-line methods.
     * Loaded constants use explicit child aliases and do not need debug data. */
    for (TZrUInt32 index = 0u; index < left->executionLocationInfoLength; ++index) {
        const SZrFunctionExecutionLocationInfo *a = &left->executionLocationInfoList[index];
        const SZrFunctionExecutionLocationInfo *b = &right->executionLocationInfoList[index];
        if (a->currentInstructionOffset != b->currentInstructionOffset || a->lineInSource != b->lineInSource ||
            a->columnInSourceStart != b->columnInSourceStart || a->lineInSourceEnd != b->lineInSourceEnd ||
            a->columnInSourceEnd != b->columnInSourceEnd) return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Function_FindConstantChildAlias(const SZrFunction *function,
        TZrUInt32 constantIndex, TZrUInt32 *childIndex, TZrBool *hasDefinition) {
    const SZrMetadataTokenRecord *constant = ZR_NULL;
    *hasDefinition = ZR_FALSE;
    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; ++index) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_DEF ||
            record->reserved0 != ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT || record->ownerIndex != constantIndex)
            continue;
        *hasDefinition = ZR_TRUE;
        if (constant != ZR_NULL) return ZR_FALSE;
        constant = record;
    }
    if (constant == ZR_NULL || constant->targetMetadataToken == 0u) return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; ++index) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        if (record->token == constant->targetMetadataToken &&
            record->reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_CHILD &&
            record->signatureHash == constant->signatureHash && record->ownerIndex < function->childFunctionLength) {
            *childIndex = record->ownerIndex;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}
