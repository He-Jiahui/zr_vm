#include "zr_vm_parser/semantic_query.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"

#define ZR_PUBLIC_CONTRACT_HASH_OFFSET ((TZrUInt64)14695981039346656037ULL)
#define ZR_PUBLIC_CONTRACT_HASH_PRIME ((TZrUInt64)1099511628211ULL)
#define ZR_PUBLIC_CONTRACT_HASH_SCHEMA_VERSION ((TZrUInt64)1ULL)

typedef enum EZrPublicContractExportKind {
    ZR_PUBLIC_CONTRACT_EXPORT_FUNCTION = 1,
    ZR_PUBLIC_CONTRACT_EXPORT_VARIABLE = 2
} EZrPublicContractExportKind;

typedef struct SZrPublicContractExport {
    EZrPublicContractExportKind kind;
    const SZrString *name;
    TZrUInt64 contractHash;
} SZrPublicContractExport;

static void public_contract_hash_byte(TZrUInt64 *hash, TZrUInt8 value) {
    *hash ^= (TZrUInt64)value;
    *hash *= ZR_PUBLIC_CONTRACT_HASH_PRIME;
}

static void public_contract_hash_word(TZrUInt64 *hash, TZrUInt64 value) {
    TZrUInt32 shift;

    for (shift = 0U; shift < 64U; shift += 8U) {
        public_contract_hash_byte(hash, (TZrUInt8)((value >> shift) & 0xffU));
    }
}

static TZrBool public_contract_hash_string(TZrUInt64 *hash, const SZrString *value) {
    const TZrChar *text;
    TZrSize length;
    TZrSize index;

    if (value == ZR_NULL) {
        return ZR_FALSE;
    }
    text = ZrCore_String_GetNativeString(value);
    length = ZrCore_String_GetByteLength(value);
    if (text == ZR_NULL) {
        return ZR_FALSE;
    }
    public_contract_hash_word(hash, (TZrUInt64)length);
    for (index = 0U; index < length; index++) {
        public_contract_hash_byte(hash, (TZrUInt8)text[index]);
    }
    return ZR_TRUE;
}

static TZrBool public_contract_array_is_readable(
        const SZrArray *array,
        TZrSize elementSize) {
    return (TZrBool)(array != ZR_NULL && array->isValid &&
                     array->elementSize == elementSize &&
                     (array->length == 0U || array->head != ZR_NULL));
}

static TZrBool public_contract_hash_type(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId genericOwnerSymbolId,
        TZrSize genericParameterCount,
        TZrSize remainingDepth,
        TZrUInt64 *hash);

static TZrBool public_contract_hash_type_id_array(
        const SZrSemanticContext *context,
        const SZrArray *typeIds,
        TZrSymbolId genericOwnerSymbolId,
        TZrSize genericParameterCount,
        TZrSize remainingDepth,
        TZrUInt64 *hash) {
    TZrSize index;

    if (!public_contract_array_is_readable(typeIds, sizeof(TZrTypeId))) {
        return ZR_FALSE;
    }
    public_contract_hash_word(hash, (TZrUInt64)typeIds->length);
    for (index = 0U; index < typeIds->length; index++) {
        const TZrTypeId *elementTypeId = (const TZrTypeId *)ZrCore_Array_Get(
                (SZrArray *)typeIds, index);
        if (elementTypeId == ZR_NULL ||
            !public_contract_hash_type(
                    context,
                    *elementTypeId,
                    genericOwnerSymbolId,
                    genericParameterCount,
                    remainingDepth,
                    hash)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool public_contract_hash_function_type(
        const SZrSemanticContext *context,
        const SZrCanonicalFunctionType *functionType,
        TZrSymbolId genericOwnerSymbolId,
        TZrSize genericParameterCount,
        TZrSize remainingDepth,
        TZrUInt64 *hash) {
    TZrSize index;

    if (!public_contract_array_is_readable(
                &functionType->parameterContracts,
                sizeof(SZrCanonicalParameterContract))) {
        return ZR_FALSE;
    }
    public_contract_hash_word(hash, (TZrUInt64)functionType->parameterContracts.length);
    for (index = 0U; index < functionType->parameterContracts.length; index++) {
        const SZrCanonicalParameterContract *parameter =
                (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                        (SZrArray *)&functionType->parameterContracts, index);
        if (parameter == ZR_NULL ||
            !public_contract_hash_type(
                    context,
                    parameter->typeId,
                    genericOwnerSymbolId,
                    genericParameterCount,
                    remainingDepth,
                    hash)) {
            return ZR_FALSE;
        }
        public_contract_hash_word(hash, (TZrUInt64)parameter->passingForm);
        public_contract_hash_word(hash, (TZrUInt64)parameter->escapeUpperBound);
        public_contract_hash_word(hash, (TZrUInt64)parameter->entryInitialization);
        public_contract_hash_word(hash, (TZrUInt64)parameter->exitInitialization);
        public_contract_hash_word(hash, (TZrUInt64)parameter->acceptsTemporary);
        public_contract_hash_word(hash, (TZrUInt64)parameter->callSiteMarker);
    }
    if (!public_contract_hash_type(
                context,
                functionType->returnTypeId,
                genericOwnerSymbolId,
                genericParameterCount,
                remainingDepth,
                hash)) {
        return ZR_FALSE;
    }
    public_contract_hash_word(hash, (TZrUInt64)functionType->receiverEffect);
    public_contract_hash_word(hash, (TZrUInt64)functionType->effectFlags);
    return ZR_TRUE;
}

static TZrBool public_contract_hash_generic_instance(
        const SZrSemanticContext *context,
        const SZrCanonicalGenericInstanceType *genericInstance,
        TZrSymbolId genericOwnerSymbolId,
        TZrSize genericParameterCount,
        TZrSize remainingDepth,
        TZrUInt64 *hash) {
    TZrSize index;

    if (!public_contract_hash_type(
                context,
                genericInstance->definitionTypeId,
                genericOwnerSymbolId,
                genericParameterCount,
                remainingDepth,
                hash) ||
        !public_contract_array_is_readable(
                &genericInstance->arguments,
                sizeof(SZrCanonicalGenericArgument))) {
        return ZR_FALSE;
    }
    public_contract_hash_word(hash, (TZrUInt64)genericInstance->arguments.length);
    for (index = 0U; index < genericInstance->arguments.length; index++) {
        const SZrCanonicalGenericArgument *argument =
                (const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                        (SZrArray *)&genericInstance->arguments, index);
        if (argument == ZR_NULL) {
            return ZR_FALSE;
        }
        public_contract_hash_word(hash, (TZrUInt64)argument->kind);
        switch (argument->kind) {
            case ZR_CANONICAL_GENERIC_ARGUMENT_TYPE:
                if (!public_contract_hash_type(
                            context,
                            argument->data.typeId,
                            genericOwnerSymbolId,
                            genericParameterCount,
                            remainingDepth,
                            hash)) {
                    return ZR_FALSE;
                }
                break;
            case ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT:
                public_contract_hash_word(
                        hash, (TZrUInt64)argument->data.constIntValue);
                break;
            case ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER:
                if (genericOwnerSymbolId == ZR_SEMANTIC_ID_INVALID ||
                    argument->data.constParameter.ownerSymbolId != genericOwnerSymbolId ||
                    (TZrSize)argument->data.constParameter.ordinal >= genericParameterCount) {
                    return ZR_FALSE;
                }
                public_contract_hash_word(
                        hash, (TZrUInt64)argument->data.constParameter.ordinal);
                break;
            default:
                return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool public_contract_hash_type(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId genericOwnerSymbolId,
        TZrSize genericParameterCount,
        TZrSize remainingDepth,
        TZrUInt64 *hash) {
    const SZrCanonicalTypeNode *node;

    if (context == ZR_NULL || hash == ZR_NULL ||
        typeId == ZR_SEMANTIC_ID_INVALID || remainingDepth == 0U) {
        return ZR_FALSE;
    }
    node = ZrParser_CanonicalType_Find(context, typeId);
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }
    public_contract_hash_word(hash, (TZrUInt64)node->kind);
    remainingDepth--;
    switch (node->kind) {
        case ZR_CANONICAL_TYPE_PRIMITIVE:
            public_contract_hash_word(hash, (TZrUInt64)node->data.primitive.valueType);
            return ZR_TRUE;
        case ZR_CANONICAL_TYPE_NOMINAL:
            return (TZrBool)(public_contract_hash_string(
                                     hash, node->data.nominal.moduleIdentity) &&
                             public_contract_hash_string(hash, node->data.nominal.name));
        case ZR_CANONICAL_TYPE_GENERIC_PARAMETER:
            if (genericOwnerSymbolId == ZR_SEMANTIC_ID_INVALID ||
                node->data.genericParameter.ownerSymbolId != genericOwnerSymbolId ||
                (TZrSize)node->data.genericParameter.ordinal >= genericParameterCount) {
                return ZR_FALSE;
            }
            public_contract_hash_word(
                    hash, (TZrUInt64)node->data.genericParameter.ordinal);
            return ZR_TRUE;
        case ZR_CANONICAL_TYPE_GENERIC_INSTANCE:
            return public_contract_hash_generic_instance(
                    context,
                    &node->data.genericInstance,
                    genericOwnerSymbolId,
                    genericParameterCount,
                    remainingDepth,
                    hash);
        case ZR_CANONICAL_TYPE_ARRAY:
            public_contract_hash_word(hash, (TZrUInt64)node->data.array.rank);
            public_contract_hash_word(hash, (TZrUInt64)node->data.array.storageKind);
            return public_contract_hash_type(
                    context,
                    node->data.array.elementTypeId,
                    genericOwnerSymbolId,
                    genericParameterCount,
                    remainingDepth,
                    hash);
        case ZR_CANONICAL_TYPE_TUPLE:
            return public_contract_hash_type_id_array(
                    context,
                    &node->data.typeList.elementTypeIds,
                    genericOwnerSymbolId,
                    genericParameterCount,
                    remainingDepth,
                    hash);
        case ZR_CANONICAL_TYPE_UNION:
            if (!public_contract_hash_type(
                        context,
                        node->data.unionType.definitionTypeId,
                        genericOwnerSymbolId,
                        genericParameterCount,
                        remainingDepth,
                        hash)) {
                return ZR_FALSE;
            }
            return public_contract_hash_type_id_array(
                    context,
                    &node->data.unionType.variantTypeIds,
                    genericOwnerSymbolId,
                    genericParameterCount,
                    remainingDepth,
                    hash);
        case ZR_CANONICAL_TYPE_ERROR:
            return ZR_FALSE;
        case ZR_CANONICAL_TYPE_NEVER:
            return ZR_TRUE;
        case ZR_CANONICAL_TYPE_REF:
            public_contract_hash_word(hash, (TZrUInt64)node->data.refType.access);
            return public_contract_hash_type(
                    context,
                    node->data.refType.pointeeTypeId,
                    genericOwnerSymbolId,
                    genericParameterCount,
                    remainingDepth,
                    hash);
        case ZR_CANONICAL_TYPE_OWNER:
            public_contract_hash_word(hash, (TZrUInt64)node->data.owner.ownerKind);
            return public_contract_hash_type(
                    context,
                    node->data.owner.targetTypeId,
                    genericOwnerSymbolId,
                    genericParameterCount,
                    remainingDepth,
                    hash);
        case ZR_CANONICAL_TYPE_READONLY_VIEW:
        case ZR_CANONICAL_TYPE_NULLABLE:
            return public_contract_hash_type(
                    context,
                    node->data.target.targetTypeId,
                    genericOwnerSymbolId,
                    genericParameterCount,
                    remainingDepth,
                    hash);
        case ZR_CANONICAL_TYPE_FUNCTION:
            return public_contract_hash_function_type(
                    context,
                    &node->data.function,
                    genericOwnerSymbolId,
                    genericParameterCount,
                    remainingDepth,
                    hash);
        default:
            return ZR_FALSE;
    }
}

static TZrBool public_contract_hash_generic_parameters(
        const SZrFunctionTypeInfo *functionInfo,
        TZrUInt64 *hash) {
    TZrSize parameterIndex;

    if (functionInfo->genericParameters.length > 0U &&
        !public_contract_array_is_readable(
                &functionInfo->genericParameters,
                sizeof(SZrTypeGenericParameterInfo))) {
        return ZR_FALSE;
    }
    public_contract_hash_word(hash, (TZrUInt64)functionInfo->genericParameters.length);
    for (parameterIndex = 0U;
         parameterIndex < functionInfo->genericParameters.length;
         parameterIndex++) {
        const SZrTypeGenericParameterInfo *parameter =
                (const SZrTypeGenericParameterInfo *)ZrCore_Array_Get(
                        (SZrArray *)&functionInfo->genericParameters, parameterIndex);

        if (parameter == ZR_NULL || parameter->constraintTypeNames.length > 0U) {
            return ZR_FALSE;
        }
        public_contract_hash_word(hash, (TZrUInt64)parameterIndex);
        public_contract_hash_word(hash, (TZrUInt64)parameter->genericKind);
        public_contract_hash_word(hash, (TZrUInt64)parameter->variance);
        public_contract_hash_word(hash, (TZrUInt64)parameter->requiresClass);
        public_contract_hash_word(hash, (TZrUInt64)parameter->requiresStruct);
        public_contract_hash_word(hash, (TZrUInt64)parameter->requiresNew);
        public_contract_hash_word(hash, (TZrUInt64)parameter->requiresOwner);
        public_contract_hash_word(
                hash, (TZrUInt64)parameter->requiredOwnershipQualifier);
        public_contract_hash_word(hash, 0U);
    }
    return ZR_TRUE;
}

static const SZrFunctionTypeInfo *public_contract_find_function(
        const SZrTypeEnvironment *typeEnvironment,
        const SZrAstNode *declaration) {
    const SZrFunctionTypeInfo *match = ZR_NULL;
    TZrSize index;

    if (!public_contract_array_is_readable(
                &typeEnvironment->functionReturnTypes,
                sizeof(SZrFunctionTypeInfo *))) {
        return ZR_NULL;
    }
    for (index = 0U; index < typeEnvironment->functionReturnTypes.length; index++) {
        SZrFunctionTypeInfo *const *functionInfo =
                (SZrFunctionTypeInfo *const *)ZrCore_Array_Get(
                        (SZrArray *)&typeEnvironment->functionReturnTypes, index);
        if (functionInfo != ZR_NULL && *functionInfo != ZR_NULL &&
            (*functionInfo)->declarationNode == declaration) {
            if (match != ZR_NULL) {
                return ZR_NULL;
            }
            match = *functionInfo;
        }
    }
    return match;
}

static const SZrSemanticSymbolRecord *public_contract_find_variable(
        const SZrSemanticContext *context,
        const SZrAstNode *declaration,
        const SZrAstNode *pattern,
        const SZrString *name,
        TZrTypeId *outTypeId) {
    const SZrSemanticSymbolRecord *match = ZR_NULL;
    TZrSize index;

    if (!public_contract_array_is_readable(
                &context->symbols, sizeof(SZrSemanticSymbolRecord))) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);
        if (symbol != ZR_NULL && symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_VARIABLE &&
            symbol->name != ZR_NULL && name != ZR_NULL &&
            ZrCore_String_Equal(symbol->name, (SZrString *)name) &&
            (symbol->astNode == declaration || symbol->astNode == pattern)) {
            if (match != ZR_NULL) {
                return ZR_NULL;
            }
            match = symbol;
        }
    }
    if (match != ZR_NULL && outTypeId != ZR_NULL) {
        *outTypeId = match->typeId;
    }
    return match;
}

static TZrBool public_contract_export_compare_names(
        const SZrString *left,
        const SZrString *right,
        TZrInt32 *outComparison) {
    const TZrChar *leftText;
    const TZrChar *rightText;
    TZrSize leftLength;
    TZrSize rightLength;
    TZrSize commonLength;
    int comparison;

    if (left == ZR_NULL || right == ZR_NULL || outComparison == ZR_NULL) {
        return ZR_FALSE;
    }
    leftText = ZrCore_String_GetNativeString(left);
    rightText = ZrCore_String_GetNativeString(right);
    leftLength = ZrCore_String_GetByteLength(left);
    rightLength = ZrCore_String_GetByteLength(right);
    commonLength = leftLength < rightLength ? leftLength : rightLength;
    if (leftText == ZR_NULL || rightText == ZR_NULL) {
        return ZR_FALSE;
    }
    comparison = memcmp(leftText, rightText, commonLength);
    if (comparison == 0) {
        comparison = leftLength < rightLength ? -1 : (leftLength > rightLength ? 1 : 0);
    }
    *outComparison = (TZrInt32)comparison;
    return ZR_TRUE;
}

static int public_contract_export_compare(const void *left, const void *right) {
    const SZrPublicContractExport *leftExport = (const SZrPublicContractExport *)left;
    const SZrPublicContractExport *rightExport = (const SZrPublicContractExport *)right;
    TZrInt32 nameComparison = 0;

    if (leftExport->kind != rightExport->kind) {
        return leftExport->kind < rightExport->kind ? -1 : 1;
    }
    if (!public_contract_export_compare_names(
                leftExport->name, rightExport->name, &nameComparison)) {
        return 0;
    }
    if (nameComparison != 0) {
        return nameComparison;
    }
    return leftExport->contractHash < rightExport->contractHash
                   ? -1
                   : (leftExport->contractHash > rightExport->contractHash ? 1 : 0);
}

static TZrBool public_contract_public_type_is_unsupported(const SZrAstNode *node) {
    switch (node->type) {
        case ZR_AST_STRUCT_DECLARATION:
            return (TZrBool)(node->data.structDeclaration.accessModifier == ZR_ACCESS_PUBLIC);
        case ZR_AST_CLASS_DECLARATION:
            return (TZrBool)(node->data.classDeclaration.accessModifier == ZR_ACCESS_PUBLIC);
        case ZR_AST_INTERFACE_DECLARATION:
            return (TZrBool)(node->data.interfaceDeclaration.accessModifier == ZR_ACCESS_PUBLIC);
        case ZR_AST_ENUM_DECLARATION:
            return (TZrBool)(node->data.enumDeclaration.accessModifier == ZR_ACCESS_PUBLIC);
        case ZR_AST_UNION_DECLARATION:
            return (TZrBool)(node->data.unionDeclaration.accessModifier == ZR_ACCESS_PUBLIC);
        default:
            return ZR_FALSE;
    }
}

static TZrBool public_contract_hash_source_callable_shape(
        const SZrSemanticContext *context,
        const SZrFunctionTypeInfo *functionInfo,
        const SZrAstNode *node,
        TZrUInt64 *hash) {
    const SZrCanonicalTypeNode *canonicalType;
    const SZrAstNodeArray *parameters;
    TZrSize parameterCount;
    TZrSize index;

    if (context == ZR_NULL || functionInfo == ZR_NULL || node == ZR_NULL ||
        hash == ZR_NULL || node->type != ZR_AST_FUNCTION_DECLARATION ||
        node->data.functionDeclaration.args != ZR_NULL ||
        (node->data.functionDeclaration.decorators != ZR_NULL &&
         node->data.functionDeclaration.decorators->count > 0U)) {
        return ZR_FALSE;
    }
    canonicalType = ZrParser_CanonicalType_Find(context, functionInfo->typeId);
    if (canonicalType == ZR_NULL ||
        canonicalType->kind != ZR_CANONICAL_TYPE_FUNCTION ||
        !public_contract_array_is_readable(
                &canonicalType->data.function.parameterContracts,
                sizeof(SZrCanonicalParameterContract))) {
        return ZR_FALSE;
    }

    parameters = node->data.functionDeclaration.params;
    parameterCount = parameters != ZR_NULL ? parameters->count : 0U;
    if (parameterCount != canonicalType->data.function.parameterContracts.length ||
        (parameterCount > 0U &&
         (parameters == ZR_NULL || parameters->nodes == ZR_NULL))) {
        return ZR_FALSE;
    }
    public_contract_hash_word(hash, (TZrUInt64)parameterCount);
    for (index = 0U; index < parameterCount; index++) {
        const SZrAstNode *parameterNode = parameters->nodes[index];
        const SZrParameter *parameter;

        if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
            return ZR_FALSE;
        }
        parameter = &parameterNode->data.parameter;
        if (parameter->name == ZR_NULL || parameter->name->name == ZR_NULL ||
            parameter->defaultValue != ZR_NULL ||
            (parameter->decorators != ZR_NULL &&
             parameter->decorators->count > 0U) ||
            !public_contract_hash_string(hash, parameter->name->name)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool public_contract_collect_function(
        const SZrSemanticContext *context,
        const SZrTypeEnvironment *typeEnvironment,
        const SZrAstNode *node,
        SZrPublicContractExport *outExport) {
    const SZrFunctionTypeInfo *functionInfo =
            public_contract_find_function(typeEnvironment, node);
    const SZrIdentifier *astName = node->data.functionDeclaration.name;
    TZrUInt64 hash = ZR_PUBLIC_CONTRACT_HASH_OFFSET;

    if (functionInfo == ZR_NULL || functionInfo->name == ZR_NULL ||
        functionInfo->typeId == ZR_SEMANTIC_ID_INVALID ||
        functionInfo->symbolId == ZR_SEMANTIC_ID_INVALID ||
        astName == ZR_NULL || astName->name == ZR_NULL ||
        !ZrCore_String_Equal(functionInfo->name, astName->name) ||
        !public_contract_hash_generic_parameters(functionInfo, &hash) ||
        !public_contract_hash_source_callable_shape(
                context, functionInfo, node, &hash) ||
        !public_contract_hash_type(
                context,
                functionInfo->typeId,
                functionInfo->symbolId,
                functionInfo->genericParameters.length,
                context->canonicalTypes.length + 1U,
                &hash)) {
        return ZR_FALSE;
    }
    outExport->kind = ZR_PUBLIC_CONTRACT_EXPORT_FUNCTION;
    outExport->name = functionInfo->name;
    outExport->contractHash = hash;
    return ZR_TRUE;
}

static TZrBool public_contract_collect_variable(
        const SZrSemanticContext *context,
        const SZrAstNode *node,
        SZrPublicContractExport *outExport) {
    const SZrAstNode *pattern = node->data.variableDeclaration.pattern;
    const SZrSemanticSymbolRecord *symbol;
    TZrTypeId typeId = ZR_SEMANTIC_ID_INVALID;
    TZrUInt64 hash = ZR_PUBLIC_CONTRACT_HASH_OFFSET;

    if (node->data.variableDeclaration.isConst ||
        pattern == ZR_NULL || pattern->type != ZR_AST_IDENTIFIER_LITERAL ||
        pattern->data.identifier.name == ZR_NULL) {
        return ZR_FALSE;
    }
    symbol = public_contract_find_variable(
            context, node, pattern, pattern->data.identifier.name, &typeId);
    if (symbol == ZR_NULL || typeId == ZR_SEMANTIC_ID_INVALID ||
        !public_contract_hash_type(
                context,
                typeId,
                ZR_SEMANTIC_ID_INVALID,
                0U,
                context->canonicalTypes.length + 1U,
                &hash)) {
        return ZR_FALSE;
    }
    outExport->kind = ZR_PUBLIC_CONTRACT_EXPORT_VARIABLE;
    outExport->name = symbol->name;
    outExport->contractHash = hash;
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticQuery_PublicContract(
        const SZrSemanticContext *context,
        const SZrTypeEnvironment *typeEnvironment,
        const SZrAstNode *moduleRoot,
        SZrParserSemanticPublicContractQuery *outQuery) {
    SZrArray exports;
    TZrUInt64 hash = ZR_PUBLIC_CONTRACT_HASH_OFFSET;
    TZrSize index;
    TZrBool succeeded = ZR_FALSE;

    if (outQuery != ZR_NULL) {
        memset(outQuery, 0, sizeof(*outQuery));
    }
    if (context == ZR_NULL || context->state == ZR_NULL ||
        typeEnvironment == ZR_NULL || moduleRoot == ZR_NULL ||
        outQuery == ZR_NULL || moduleRoot->type != ZR_AST_SCRIPT ||
        typeEnvironment->semanticContext != context ||
        moduleRoot->data.script.statements == ZR_NULL ||
        (moduleRoot->data.script.statements->count > 0U &&
         moduleRoot->data.script.statements->nodes == ZR_NULL) ||
        !context->queryDiagnostics.isValid ||
        context->queryDiagnostics.length > 0U ||
        !public_contract_array_is_readable(
                &context->canonicalTypes, sizeof(SZrCanonicalTypeNode))) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(
            context->state,
            &exports,
            sizeof(SZrPublicContractExport),
            moduleRoot->data.script.statements->count);
    for (index = 0U; index < moduleRoot->data.script.statements->count; index++) {
        const SZrAstNode *node = moduleRoot->data.script.statements->nodes[index];
        SZrPublicContractExport exportItem;
        TZrBool contributes = ZR_FALSE;

        if (node == ZR_NULL) {
            goto cleanup;
        }
        memset(&exportItem, 0, sizeof(exportItem));
        if (node->type == ZR_AST_FUNCTION_DECLARATION) {
            if (!public_contract_collect_function(
                        context, typeEnvironment, node, &exportItem)) {
                goto cleanup;
            }
            contributes = ZR_TRUE;
        } else if (node->type == ZR_AST_VARIABLE_DECLARATION &&
                   node->data.variableDeclaration.accessModifier == ZR_ACCESS_PUBLIC) {
            if (!public_contract_collect_variable(
                        context, node, &exportItem)) {
                goto cleanup;
            }
            contributes = ZR_TRUE;
        } else if (public_contract_public_type_is_unsupported(node) ||
                   node->type == ZR_AST_EXTERN_BLOCK ||
                   node->type == ZR_AST_EXTERN_FUNCTION_DECLARATION ||
                   node->type == ZR_AST_EXTERN_DELEGATE_DECLARATION ||
                   node->type == ZR_AST_COMPILE_TIME_DECLARATION ||
                   node->type == ZR_AST_INTERMEDIATE_DECLARATION ||
                   node->type == ZR_AST_INTERMEDIATE_STATEMENT) {
            goto cleanup;
        }
        if (contributes) {
            ZrCore_Array_Push(context->state, &exports, &exportItem);
        }
    }

    qsort(
            exports.head,
            exports.length,
            sizeof(SZrPublicContractExport),
            public_contract_export_compare);
    public_contract_hash_word(&hash, ZR_PUBLIC_CONTRACT_HASH_SCHEMA_VERSION);
    public_contract_hash_word(&hash, (TZrUInt64)exports.length);
    for (index = 0U; index < exports.length; index++) {
        const SZrPublicContractExport *exportItem =
                (const SZrPublicContractExport *)ZrCore_Array_Get(&exports, index);
        public_contract_hash_word(&hash, (TZrUInt64)exportItem->kind);
        if (!public_contract_hash_string(&hash, exportItem->name)) {
            goto cleanup;
        }
        public_contract_hash_word(&hash, exportItem->contractHash);
    }
    outQuery->hash = hash;
    outQuery->exportCount = exports.length;
    succeeded = ZR_TRUE;

cleanup:
    ZrCore_Array_Free(context->state, &exports);
    if (!succeeded) {
        memset(outQuery, 0, sizeof(*outQuery));
    }
    return succeeded;
}
