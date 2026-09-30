#include "zr_vm_parser/semantic_display.h"

#include "canonical_type_format_internal.h"

/* 由精确 owner symbol 的 declaration AST 取得其所属 generic 列表。 */
static const SZrGenericDeclaration *semantic_display_source_generic_declaration(
        const SZrSemanticSymbolRecord *owner) {
    const SZrAstNode *declaration;

    if (owner == ZR_NULL || owner->astNode == ZR_NULL) {
        return ZR_NULL;
    }
    declaration = owner->astNode;
    if (owner->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION) {
        switch (declaration->type) {
            case ZR_AST_FUNCTION_DECLARATION:
                return declaration->data.functionDeclaration.generic;
            case ZR_AST_CLASS_METHOD:
                return declaration->data.classMethod.generic;
            case ZR_AST_STRUCT_METHOD:
                return declaration->data.structMethod.generic;
            case ZR_AST_INTERFACE_METHOD_SIGNATURE:
                return declaration->data.interfaceMethodSignature.generic;
            default:
                return ZR_NULL;
        }
    }
    if (owner->kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE) {
        switch (declaration->type) {
            case ZR_AST_CLASS_DECLARATION:
                return declaration->data.classDeclaration.generic;
            case ZR_AST_STRUCT_DECLARATION:
                return declaration->data.structDeclaration.generic;
            case ZR_AST_INTERFACE_DECLARATION:
                return declaration->data.interfaceDeclaration.generic;
            case ZR_AST_UNION_DECLARATION:
                return declaration->data.unionDeclaration.generic;
            default:
                return ZR_NULL;
        }
    }
    return ZR_NULL;
}

/* 将 canonical owner/ordinal/kind 回查为声明中的 generic 源名称。 */
static const SZrString *semantic_display_source_generic_name(
        const SZrSemanticContext *context,
        TZrSymbolId ownerSymbolId,
        TZrUInt32 ordinal,
        EZrCanonicalGenericArgumentKind kind) {
    const SZrSemanticSymbolRecord *owner =
            ZrParser_Semantic_FindSymbolById(context, ownerSymbolId);
    const SZrGenericDeclaration *generic =
            semantic_display_source_generic_declaration(owner);
    const SZrAstNode *node;
    const SZrParameter *parameter;

    if (generic == ZR_NULL || generic->params == ZR_NULL ||
        generic->params->nodes == ZR_NULL || ordinal >= generic->params->count) {
        return ZR_NULL;
    }
    node = generic->params->nodes[ordinal];
    if (node == ZR_NULL || node->type != ZR_AST_PARAMETER) {
        return ZR_NULL;
    }
    parameter = &node->data.parameter;
    if ((kind == ZR_CANONICAL_GENERIC_ARGUMENT_TYPE &&
         parameter->genericKind != ZR_GENERIC_PARAMETER_TYPE) ||
        (kind == ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER &&
         parameter->genericKind != ZR_GENERIC_PARAMETER_CONST_INT) ||
        (kind != ZR_CANONICAL_GENERIC_ARGUMENT_TYPE &&
         kind != ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER) ||
        parameter->name == ZR_NULL || parameter->name->name == ZR_NULL ||
        ZrCore_String_GetByteLength(parameter->name->name) == 0U) {
        return ZR_NULL;
    }
    return parameter->name->name;
}

/**
 * @brief 使用声明中的 generic 名称格式化 canonical 类型。
 * @return 文本适配调用方 buffer 时返回 true；owner 或 generic AST 不匹配时返回 false。
 * @note callback 内借用 generic 名称仅限同步格式化；最终文本写入调用方 buffer。
 */
TZrBool ZrParser_SemanticDisplay_FormatSourceType(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrChar *buffer,
        TZrSize bufferSize) {
    return ZrParser_CanonicalType_FormatWithGenericNames(
            context, typeId, buffer, bufferSize, semantic_display_source_generic_name);
}
