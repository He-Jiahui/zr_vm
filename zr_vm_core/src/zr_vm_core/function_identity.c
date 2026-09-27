#include "zr_vm_core/function_identity.h"

#include <string.h>

#include "zr_vm_core/string.h"

/* 身份比较允许同一字符串对象的快速路径，也接受内容相同的不同副本。 */
static TZrBool same_string(SZrString *left, SZrString *right) {
    return left == right || (left != ZR_NULL && right != ZR_NULL && ZrCore_String_Equal(left, right));
}

/* 函数常量是待重绑的图边；其余常量必须值相同，避免同形字节码误合并。 */
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

/* 子函数图和 AOT 绑定共用此规则；非共享缓冲路径缺少调试位置时拒绝判等。 */
TZrBool ZrCore_Function_HasSameDefinition(const SZrFunction *left, const SZrFunction *right) {
    if (left == ZR_NULL || right == ZR_NULL) return ZR_FALSE;
    if (left == right) return ZR_TRUE;
    if (left->instructionsList != ZR_NULL && right->instructionsList != ZR_NULL &&
        left->instructionsLength == right->instructionsLength &&
        memcmp(left->instructionsList, right->instructionsList,
               (TZrSize)left->instructionsLength * sizeof(*left->instructionsList)) != 0) return ZR_FALSE;
    /* TODO: 共享指令与常量缓冲会提前判等，跳过名称、源码与位置检查。
     * 当前调用方用于去重和绑定；需查加载/复制路径是否可能共享缓冲却保留不同定义身份。 */
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

/* 显式记录优先于按函数形状猜测；重复定义或目标记录失配时拒绝别名。 */
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
