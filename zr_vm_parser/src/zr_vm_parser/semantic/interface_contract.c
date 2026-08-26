#include "zr_vm_parser/interface_contract.h"

#include <stdio.h>
#include <string.h>

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
