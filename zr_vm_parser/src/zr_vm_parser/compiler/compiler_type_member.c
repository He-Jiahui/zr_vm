#include "compiler_internal.h"

/**
 * @brief 把成员的返回类型同时发布为名称和结构化类型，供成员契约比较及属性类型绑定共用。
 * @pre memberInfo 是本次声明的新成员；旧的 structuredReturnType 若有所有权，调用前须先释放。
 * @return 无返回类型视为可选缺省并返回 true；显式类型转换失败返回 false，调用方应停止发布该成员。
 * @note 成功转换的结构化类型转交 memberInfo，后续随成员记录统一清理。
 * TODO: class/interface/struct 的方法构造路径忽略 false；需确认转换失败后整次编译能否拒绝并避免发布不完整成员。
 */
TZrBool compiler_type_member_capture_structured_return_type(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *memberInfo,
        SZrType *returnType) {
    SZrInferredType inferredType;

    if (cs == ZR_NULL || memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    memberInfo->returnTypeName =
            returnType != ZR_NULL ? extract_type_name_string(cs, returnType) : ZR_NULL;
    memberInfo->hasStructuredReturnType = ZR_FALSE;
    memset(&memberInfo->structuredReturnType, 0, sizeof(memberInfo->structuredReturnType));
    if (returnType == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!ZrParser_AstTypeToInferredType_Convert(cs, returnType, &inferredType)) {
        return ZR_FALSE;
    }
    /* 成员记录接管结构化类型；hasStructuredReturnType 也决定后续契约比较与销毁是否读取它。 */
    memberInfo->structuredReturnType = inferredType;
    memberInfo->hasStructuredReturnType = ZR_TRUE;
    return ZR_TRUE;
}

/**
 * @brief 将已解析的类型成员绑定到语义索引，使名称查询可回溯成员声明及其拥有类型。
 * @pre 调用方已准备成员名称、ownerTypeName、声明 AST 和语义上下文；ownerTypeName 可规范化为类型 ID。
 * @note 已有 symbolId 时直接复用，不再次分配语义符号；成功后 ID 保存在 memberInfo 中。
 * TODO: 此处把 ownerTypeId 作为符号 typeId，属性路径使用成员值类型 ID；需核对语义查询对字段/方法 typeId 的预期。
 */
static TZrBool compiler_type_member_register_symbol(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *memberInfo,
        EZrSemanticSymbolKind kind) {
    TZrTypeId ownerTypeId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || memberInfo == ZR_NULL ||
        memberInfo->name == ZR_NULL || memberInfo->ownerTypeName == ZR_NULL ||
        memberInfo->declarationNode == ZR_NULL) {
        return ZR_FALSE;
    }
    /* 保留已发布的语义身份，避免同一成员路径反复注册生成另一个 symbolId。 */
    if (memberInfo->symbolId != ZR_SEMANTIC_ID_INVALID) {
        return ZR_TRUE;
    }
    ownerTypeId = ZrParser_CanonicalType_FromName(
            cs->semanticContext, memberInfo->ownerTypeName);
    if (ownerTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    memberInfo->symbolId = ZrParser_Semantic_RegisterSymbol(
            cs->semanticContext,
            memberInfo->name,
            kind,
            ownerTypeId,
            ZR_SEMANTIC_ID_INVALID,
            memberInfo->declarationNode,
            memberInfo->declarationNode->location);
    return memberInfo->symbolId != ZR_SEMANTIC_ID_INVALID;
}

/** @brief 为 class/interface/struct 方法选择 function 语义种类，沿用共享成员注册约束。 */
TZrBool compiler_type_member_register_function_symbol(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *memberInfo) {
    return compiler_type_member_register_symbol(
            cs, memberInfo, ZR_SEMANTIC_SYMBOL_KIND_FUNCTION);
}

/** @brief 为 class/interface/struct 字段选择 field 语义种类，沿用共享成员注册约束。 */
TZrBool compiler_type_member_register_field_symbol(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *memberInfo) {
    return compiler_type_member_register_symbol(
            cs, memberInfo, ZR_SEMANTIC_SYMBOL_KIND_FIELD);
}
