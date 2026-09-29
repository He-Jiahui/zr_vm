#include "compiler_internal.h"
#include "zr_vm_parser/receiver_call.h"
#include "zr_vm_parser/syntax_contract.h"

SZrTypePrototypeInfo *find_compiler_type_prototype(
        SZrCompilerState *cs,
        SZrString *typeName);

/**
 * @brief 读取 struct/class 方法 AST 上声明的接收者所有权限定。
 * @note 只有这两类方法携带该限定；其余 AST 节点返回 NONE，供隐式 this 合成使用。
 */
EZrOwnershipQualifier get_member_receiver_qualifier(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_OWNERSHIP_QUALIFIER_NONE;
    }
    switch (node->type) {
        case ZR_AST_STRUCT_METHOD:
            return node->data.structMethod.receiverQualifier;
        case ZR_AST_CLASS_METHOD:
            return node->data.classMethod.receiverQualifier;
        default:
            return ZR_OWNERSHIP_QUALIFIER_NONE;
    }
}

/**
 * @brief 将成员声明统一投影为 canonical receiver effect。
 * @note 具体方法、属性访问器和静态成员规则由 syntax contract 集中维护，避免各编译入口自行解释修饰符。
 */
EZrCanonicalReceiverEffect get_member_receiver_effect(SZrAstNode *node) {
    return ZrParser_SyntaxCallable_ReceiverEffectFromDeclaration(node);
}

/**
 * @brief 为注入的 this 绑定推导不会转移所有权的接收者限定。
 * @note unique/borrowed/loaned 的当前接收者在方法体内都作为借用使用；shared、weak 与 NONE 保持原限定。
 */
EZrOwnershipQualifier get_implicit_this_ownership_qualifier(
        EZrOwnershipQualifier receiverQualifier) {
    if (receiverQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
        receiverQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED ||
        receiverQualifier == ZR_OWNERSHIP_QUALIFIER_LOANED) {
        return ZR_OWNERSHIP_QUALIFIER_BORROWED;
    }
    return receiverQualifier;
}

/**
 * @brief 检查实现的 receiver effect 是否满足继承或接口要求。
 * @note 只读要求必须由只读实现保持；可写要求可由只读或可写实现满足，因为实现可以减少副作用。
 */
TZrBool compiler_receiver_effect_can_implement(
        EZrCanonicalReceiverEffect requiredEffect,
        EZrCanonicalReceiverEffect implementationEffect) {
    if (requiredEffect == ZR_CANONICAL_RECEIVER_READONLY) {
        return implementationEffect == ZR_CANONICAL_RECEIVER_READONLY;
    }
    if (requiredEffect == ZR_CANONICAL_RECEIVER_MUTABLE) {
        return implementationEffect == ZR_CANONICAL_RECEIVER_MUTABLE ||
               implementationEffect == ZR_CANONICAL_RECEIVER_READONLY;
    }
    return requiredEffect == implementationEffect;
}

/**
 * @brief 按已解析的成员元数据确定 receiver-call 的分派类别。
 * @note 各类别共用 canonical capability 判定；struct 类别另用于要求可写值接收者具有可寻址 Place。
 */
static EZrReceiverDispatchKind compiler_receiver_dispatch_kind(
        SZrCompilerState *cs,
        const SZrInferredType *receiverType,
        const SZrTypeMemberInfo *memberInfo) {
    /* 导入/native token 优先，避免后续源码声明形态改变其分派身份。 */
    if (memberInfo != ZR_NULL &&
        (memberInfo->metadataToken != 0U ||
         memberInfo->signatureToken != 0U)) {
        return ZR_RECEIVER_DISPATCH_NATIVE;
    }
    /* override 与 generic contract 比普通 owner-kind 分类更具体。 */
    if (memberInfo != ZR_NULL &&
        (memberInfo->modifierFlags & ZR_DECLARATION_MODIFIER_OVERRIDE) != 0U) {
        return ZR_RECEIVER_DISPATCH_OVERRIDE;
    }
    if ((receiverType != ZR_NULL && receiverType->elementTypes.length > 0U) ||
        (memberInfo != ZR_NULL && memberInfo->genericParameters.length > 0U)) {
        return ZR_RECEIVER_DISPATCH_GENERIC;
    }
    /* 合成的属性 accessor 可能没有 interface signature AST，需由所属 prototype 补足分类。 */
    if (cs != ZR_NULL && memberInfo != ZR_NULL &&
        memberInfo->accessorRole != ZR_PROPERTY_ACCESSOR_ROLE_NONE &&
        memberInfo->ownerTypeName != ZR_NULL) {
        SZrTypePrototypeInfo *ownerPrototype = find_compiler_type_prototype(
                cs, memberInfo->ownerTypeName);
        if (ownerPrototype != ZR_NULL &&
            ownerPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
            return ZR_RECEIVER_DISPATCH_INTERFACE;
        }
    }
    if (memberInfo != ZR_NULL && memberInfo->declarationNode != ZR_NULL &&
        (memberInfo->declarationNode->type ==
                 ZR_AST_INTERFACE_METHOD_SIGNATURE ||
         memberInfo->declarationNode->type ==
                 ZR_AST_INTERFACE_META_SIGNATURE ||
         memberInfo->declarationNode->type ==
                 ZR_AST_INTERFACE_PROPERTY_SIGNATURE)) {
        return ZR_RECEIVER_DISPATCH_INTERFACE;
    }
    if (memberInfo != ZR_NULL &&
        memberInfo->memberType == ZR_AST_STRUCT_METHOD) {
        return ZR_RECEIVER_DISPATCH_STRUCT;
    }
    return ZR_RECEIVER_DISPATCH_CLASS;
}

/**
 * @brief 在表达式降级前校验成员调用或字段写入的 receiver capability。
 * @pre 调用方提供已解析的成员契约；receiverNode 可为空，此时用当前成员链的类型名和限定补全。
 * @note 此处只负责统一语义 gate；调用借用的 reserve/activate/end 生命周期由后续 Semantic IR 降级管理。
 */
TZrBool compiler_validate_receiver_call(
        SZrCompilerState *cs,
        SZrAstNode *receiverNode,
        SZrString *receiverTypeName,
        EZrOwnershipQualifier receiverQualifier,
        const SZrTypeMemberInfo *memberInfo,
        TZrBool receiverIsAddressable,
        SZrFileRange location) {
    SZrInferredType receiverType;
    SZrReceiverCallDecision decision;

    /* 静态成员或无 receiver effect 的契约不依赖表达式接收者 capability。 */
    if (cs == ZR_NULL || memberInfo == ZR_NULL || memberInfo->isStatic ||
        memberInfo->receiverEffect == ZR_CANONICAL_RECEIVER_NONE) {
        return ZR_TRUE;
    }

    /* 根表达式能精确推导时保留其完整 capability；中间成员段则从解析出的类型名开始。 */
    ZrParser_InferredType_InitFull(
            cs->state,
            &receiverType,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            receiverTypeName);
    if (receiverNode != ZR_NULL) {
        SZrInferredType inferredType;
        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (ZrParser_ExpressionType_Infer(cs, receiverNode, &inferredType)) {
            ZrParser_InferredType_Free(cs->state, &receiverType);
            ZrParser_InferredType_Init(cs->state, &receiverType, ZR_VALUE_TYPE_OBJECT);
            ZrParser_InferredType_Copy(cs->state, &receiverType, &inferredType);
        }
        ZrParser_InferredType_Free(cs->state, &inferredType);
    }
    if (receiverType.typeName == ZR_NULL) {
        receiverType.typeName = receiverTypeName;
    }
    if (receiverType.ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_NONE) {
        receiverType.ownershipQualifier = receiverQualifier;
    }
    memset(&decision, 0, sizeof(decision));
    if (!ZrParser_ReceiverCall_AnalyzeInferred(
                cs->semanticContext,
                &receiverType,
                memberInfo->receiverEffect,
                compiler_receiver_dispatch_kind(cs, &receiverType, memberInfo),
                receiverIsAddressable,
                ZR_TRUE,
                &decision) ||
        !decision.allowed) {
        /* 不可调用和无效 canonical contract 共用语义诊断出口，避免继续生成该调用的指令。 */
        ZrParser_InferredType_Free(cs->state, &receiverType);
        ZrParser_Compiler_Error(
                cs,
                ZrParser_ReceiverCall_DiagnosticMessage(decision.diagnostic),
                location);
        return ZR_FALSE;
    }
    ZrParser_InferredType_Free(cs->state, &receiverType);
    return ZR_TRUE;
}
