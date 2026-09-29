#include "zr_vm_parser/interface_contract.h"

#include <stdio.h>
#include <string.h>

/** @brief 按编译器保存的类型名称查找原型，并在列表未收录时检查当前原型。
 * @note 继承类型在此以字符串名称定位，不使用类型或成员 SymbolId。
 */
static SZrTypePrototypeInfo *interface_contract_find_prototype(
        SZrCompilerState *compilerState,
        SZrString *typeName) {
    TZrSize index;

    if (compilerState == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < compilerState->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *candidate =
                (SZrTypePrototypeInfo *)ZrCore_Array_Get(
                        &compilerState->typePrototypes, index);
        if (candidate != ZR_NULL && candidate->name != ZR_NULL &&
            ZrCore_String_Equal(candidate->name, typeName)) {
            return candidate;
        }
    }
    if (compilerState->currentTypePrototypeInfo != ZR_NULL &&
        compilerState->currentTypePrototypeInfo->name != ZR_NULL &&
        ZrCore_String_Equal(
                compilerState->currentTypePrototypeInfo->name, typeName)) {
        return compilerState->currentTypePrototypeInfo;
    }
    return ZR_NULL;
}

/** @brief 优先按类声明 AST 指针定位原型，缺少指针匹配时才退回类名查找。
 * @note 指针匹配保持当前声明身份；名称回退依赖编译器原型表中的名称唯一性。
 */
static SZrTypePrototypeInfo *interface_contract_find_class(
        SZrCompilerState *compilerState,
        const SZrAstNode *classNode) {
    SZrString *className;
    TZrSize index;

    if (compilerState == ZR_NULL || classNode == ZR_NULL ||
        classNode->type != ZR_AST_CLASS_DECLARATION) {
        return ZR_NULL;
    }
    if (compilerState->currentTypePrototypeInfo != ZR_NULL &&
        compilerState->currentTypePrototypeInfo->declarationNode == classNode) {
        return compilerState->currentTypePrototypeInfo;
    }
    for (index = 0U; index < compilerState->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *candidate =
                (SZrTypePrototypeInfo *)ZrCore_Array_Get(
                        &compilerState->typePrototypes, index);
        if (candidate != ZR_NULL && candidate->declarationNode == classNode) {
            return candidate;
        }
    }

    className = classNode->data.classDeclaration.name != ZR_NULL
            ? classNode->data.classDeclaration.name->name
            : ZR_NULL;
    return interface_contract_find_prototype(compilerState, className);
}

/** @brief 在实现类成员表中按名称取第一个匹配项供 const 字段检查使用。
 * @note 本辅助查找比较字符串名称，不以成员 SymbolId 选择候选。
 */
static SZrTypeMemberInfo *interface_contract_find_declared_member(
        SZrTypePrototypeInfo *classInfo,
        SZrString *memberName) {
    TZrSize index;

    if (classInfo == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < classInfo->members.length; index++) {
        SZrTypeMemberInfo *member =
                (SZrTypeMemberInfo *)ZrCore_Array_Get(
                        &classInfo->members, index);
        if (member != ZR_NULL && member->name != ZR_NULL &&
            ZrCore_String_Equal(member->name, memberName)) {
            return member;
        }
    }
    return ZR_NULL;
}

static SZrFileRange interface_contract_class_primary_range(
        const SZrAstNode *classNode,
        const SZrTypeMemberInfo *classMember) {
    if (classMember != ZR_NULL && classMember->declarationNode != ZR_NULL) {
        if (classMember->declarationNode->type == ZR_AST_CLASS_FIELD) {
            return classMember->declarationNode->data.classField.nameLocation;
        }
        return classMember->declarationNode->location;
    }
    if (classNode != ZR_NULL && classNode->type == ZR_AST_CLASS_DECLARATION) {
        return classNode->data.classDeclaration.nameLocation;
    }
    return classNode != ZR_NULL ? classNode->location : (SZrFileRange){0};
}

static SZrFileRange interface_contract_required_member_range(
        const SZrTypeMemberInfo *requiredMember,
        const SZrTypePrototypeInfo *interfaceInfo,
        const SZrAstNode *classNode) {
    if (requiredMember != ZR_NULL && requiredMember->declarationNode != ZR_NULL) {
        if (requiredMember->declarationNode->type == ZR_AST_CLASS_FIELD) {
            return requiredMember->declarationNode->data.classField.nameLocation;
        }
        if (requiredMember->declarationNode->type ==
            ZR_AST_INTERFACE_FIELD_DECLARATION) {
            return requiredMember->declarationNode->data
                    .interfaceFieldDeclaration.nameLocation;
        }
        return requiredMember->declarationNode->location;
    }
    if (interfaceInfo != ZR_NULL && interfaceInfo->declarationNode != ZR_NULL) {
        return interfaceInfo->declarationNode->location;
    }
    return classNode != ZR_NULL ? classNode->location : (SZrFileRange){0};
}

/**
 * @brief 按原型继承顺序枚举实现类缺少或丢失 const 的接口字段。
 * @note 候选通过类型名和成员名查找；常规类编译入口会先做通用签名检查。本专项只比较字段存在性与 isConst，也不比较 accessModifier。
 * BUG: 常规类签名校验会递归父接口但忽略 isConst；本专项只枚举类的直接接口和子接口自身成员，
 *      而 compiler_interface 只登记接口自身成员、不展平父接口。
 *      因此 Base{pub const id}、Child:Base、Impl:Child{pub var id} 可绕过祖先 const 检查并无诊断。
 * TODO: 独立核对接口访问级别是否要求实现保留 pub：对照语言规范和访问校验路径，
 *       再用 pub const 要求与 pri const 实现用例确定预期。
 * @note outViolation 中的节点及字段名借用 AST/原型；位置范围按值复制。
 * @return true 找到 violationIndex 指定的违规；false 也可能表示参数无效或原型不可用。
 */
TZrBool ZrParser_InterfaceContract_ConstFieldViolationAt(
        SZrCompilerState *compilerState,
        const SZrAstNode *classNode,
        TZrSize violationIndex,
        SZrInterfaceConstFieldViolation *outViolation) {
    SZrTypePrototypeInfo *classInfo;
    TZrSize inheritIndex;
    TZrSize foundCount = 0U;

    if (compilerState == ZR_NULL || classNode == ZR_NULL ||
        classNode->type != ZR_AST_CLASS_DECLARATION ||
        outViolation == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outViolation, 0, sizeof(*outViolation));
    classInfo = interface_contract_find_class(compilerState, classNode);
    if (classInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    for (inheritIndex = 0U; inheritIndex < classInfo->inherits.length; inheritIndex++) {
        SZrString **inheritName = (SZrString **)ZrCore_Array_Get(
                &classInfo->inherits, inheritIndex);
        SZrTypePrototypeInfo *interfaceInfo = inheritName != ZR_NULL
                ? interface_contract_find_prototype(compilerState, *inheritName)
                : ZR_NULL;
        TZrSize memberIndex;

        if (interfaceInfo == ZR_NULL ||
            interfaceInfo->type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
            continue;
        }
        for (memberIndex = 0U;
             memberIndex < interfaceInfo->members.length;
             memberIndex++) {
            SZrTypeMemberInfo *requiredMember =
                    (SZrTypeMemberInfo *)ZrCore_Array_Get(
                            &interfaceInfo->members, memberIndex);
            SZrTypeMemberInfo *classMember;

            if (requiredMember == ZR_NULL || requiredMember->name == ZR_NULL ||
                requiredMember->memberType != ZR_AST_CLASS_FIELD ||
                !requiredMember->isConst) {
                continue;
            }
            classMember = interface_contract_find_declared_member(
                    classInfo, requiredMember->name);
            if (classMember != ZR_NULL && classMember->isConst) {
                continue;
            }
            if (foundCount++ != violationIndex) {
                continue;
            }

            outViolation->node = classMember != ZR_NULL
                    ? classMember->declarationNode
                    : (SZrAstNode *)classNode;
            outViolation->fieldName = requiredMember->name;
            outViolation->kind = classMember != ZR_NULL
                    ? ZR_INTERFACE_CONST_FIELD_DROPS_CONST
                    : ZR_INTERFACE_CONST_FIELD_MISSING;
            outViolation->location = interface_contract_class_primary_range(
                    classNode, classMember);
            outViolation->requiredDeclarationRange =
                    interface_contract_required_member_range(
                            requiredMember, interfaceInfo, classNode);
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/**
 * @brief 为缺失或丢失 const 的实现字段生成带接口声明关联位置的结构化诊断。
 * @note 诊断代码固定为 const_interface_mismatch，自动修复交由用户决定；此处只构造，不追加语义事实。
 */
TZrBool ZrParser_InterfaceContract_BuildConstFieldDiagnostic(
        SZrState *state,
        const SZrInterfaceConstFieldViolation *violation,
        SZrStructuredDiagnostic *outDiagnostic) {
    const TZrChar *fieldName;
    const TZrChar *suggestion;
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (state == ZR_NULL || violation == ZR_NULL ||
        violation->fieldName == ZR_NULL || outDiagnostic == ZR_NULL) {
        return ZR_FALSE;
    }
    fieldName = ZrCore_String_GetNativeStringShort(violation->fieldName);
    if (fieldName != ZR_NULL) {
        snprintf(message,
                 sizeof(message),
                 "Interface const field '%s' must remain const in implementing class",
                 fieldName);
    } else {
        snprintf(message,
                 sizeof(message),
                 "Interface const field must remain const in implementing class");
    }
    suggestion = violation->kind == ZR_INTERFACE_CONST_FIELD_MISSING
            ? "Declare a const field with the required name and type."
            : "Mark the implementing field const.";

    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                outDiagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                violation->location,
                "const_interface_mismatch",
                message,
                "The interface requires this field to preserve const access in every implementation.",
                suggestion)) {
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_AddRelatedInformation(
                state,
                outDiagnostic,
                violation->requiredDeclarationRange,
                "Const field is required by this interface declaration") ||
        !ZrParser_StructuredDiagnostic_SetNoFixReason(
                outDiagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(state, outDiagnostic);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/**
 * @brief 顺序发布类所实现接口的全部 const 字段违规，供语义查询物化。
 * @note 诊断事实由语义上下文持有；成功追加后释放本地诊断。若后续构造或追加失败会返回 false，先前已追加事实不在此回滚，也不改写编译错误状态。
 * @return true 表示枚举结束并完成所有追加；false 表示上下文无效或构造、追加失败。
 */
TZrBool ZrParser_InterfaceContract_PublishConstFieldDiagnostics(
        SZrCompilerState *compilerState,
        const SZrAstNode *classNode) {
    TZrSize violationIndex = 0U;
    SZrInterfaceConstFieldViolation violation;

    if (compilerState == ZR_NULL || compilerState->state == ZR_NULL ||
        compilerState->semanticContext == ZR_NULL || classNode == ZR_NULL) {
        return ZR_FALSE;
    }

    while (ZrParser_InterfaceContract_ConstFieldViolationAt(
            compilerState,
            classNode,
            violationIndex,
            &violation)) {
        SZrStructuredDiagnostic diagnostic;
        SZrSemanticDiagnosticFact fact;

        ZrParser_StructuredDiagnostic_Init(&diagnostic);
        if (!ZrParser_InterfaceContract_BuildConstFieldDiagnostic(
                    compilerState->state, &violation, &diagnostic)) {
            return ZR_FALSE;
        }
        memset(&fact, 0, sizeof(fact));
        fact.node = violation.node;
        fact.diagnostic = diagnostic;
        if (!ZrParser_SemanticFacts_AppendDiagnostic(
                    compilerState->semanticContext, &fact)) {
            ZrParser_StructuredDiagnostic_Free(
                    compilerState->state, &diagnostic);
            return ZR_FALSE;
        }
        ZrParser_StructuredDiagnostic_Free(compilerState->state, &diagnostic);
        violationIndex++;
    }

    return ZR_TRUE;
}
