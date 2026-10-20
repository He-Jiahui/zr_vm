#include "type_inference_nominal_member.h"

#include "type_inference_internal.h"
#include "type_inference_nominal_prototype.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_string_conf.h"
#include "zr_vm_parser/semantic.h"

#include <string.h>

static SZrString *type_inference_create_hidden_property_accessor_name(SZrCompilerState *cs,
                                                                      SZrString *propertyName,
                                                                      TZrBool isSetter) {
    const TZrChar *prefix = isSetter ? "__set_" : "__get_";
    TZrNativeString propertyNameString;
    TZrSize prefixLength;
    TZrSize propertyNameLength;
    TZrSize bufferSize;
    TZrChar *buffer;
    SZrString *result;

    if (cs == ZR_NULL || propertyName == ZR_NULL) {
        return ZR_NULL;
    }

    propertyNameString = ZrCore_String_GetNativeString(propertyName);
    if (propertyNameString == ZR_NULL) {
        return ZR_NULL;
    }

    prefixLength = strlen(prefix);
    propertyNameLength = propertyName->shortStringLength < ZR_VM_LONG_STRING_FLAG
                                 ? propertyName->shortStringLength
                                 : propertyName->longStringLength;
    bufferSize = prefixLength + propertyNameLength + 1;
    buffer = (TZrChar *)ZrCore_Memory_RawMalloc(cs->state->global, bufferSize);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(buffer, prefix, prefixLength);
    memcpy(buffer + prefixLength, propertyNameString, propertyNameLength);
    buffer[bufferSize - 1] = '\0';

    result = ZrCore_String_CreateFromNative(cs->state, buffer);
    ZrCore_Memory_RawFree(cs->state->global, buffer, bufferSize);
    return result;
}

static TZrSymbolId type_inference_member_prototype_symbol_id(
        SZrCompilerState *cs, const SZrTypePrototypeInfo *prototype) {
    const SZrSemanticContext *context;
    TZrSymbolId identity = ZR_SEMANTIC_ID_INVALID;

    if (cs == ZR_NULL || prototype == ZR_NULL || prototype->declarationNode == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    context = cs->typeEnv != ZR_NULL && cs->typeEnv->semanticContext != ZR_NULL
            ? cs->typeEnv->semanticContext : cs->semanticContext;
    if (context == ZR_NULL || !context->symbols.isValid) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    for (TZrSize index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol = ZrCore_Array_Get(
                (SZrArray *)&context->symbols, index);

        if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
            symbol->astNode != prototype->declarationNode ||
            symbol->id == ZR_SEMANTIC_ID_INVALID) {
            continue;
        }
        if (identity != ZR_SEMANTIC_ID_INVALID && identity != symbol->id) {
            return ZR_SEMANTIC_ID_INVALID;
        }
        identity = symbol->id;
    }
    return identity;
}

TZrBool type_inference_member_prototypes_have_same_identity(
        SZrCompilerState *cs, const SZrTypePrototypeInfo *left,
        const SZrTypePrototypeInfo *right) {
    TZrSymbolId leftIdentity;
    TZrSymbolId rightIdentity;

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    if (left->genericInstanceTypeId != ZR_SEMANTIC_ID_INVALID ||
        right->genericInstanceTypeId != ZR_SEMANTIC_ID_INVALID) {
        return left->genericInstanceTypeId == right->genericInstanceTypeId;
    }
    if (left->declarationNode != ZR_NULL && right->declarationNode != ZR_NULL) {
        leftIdentity = type_inference_member_prototype_symbol_id(cs, left);
        rightIdentity = type_inference_member_prototype_symbol_id(cs, right);
        if (leftIdentity != ZR_SEMANTIC_ID_INVALID && rightIdentity != ZR_SEMANTIC_ID_INVALID) {
            return leftIdentity == rightIdentity;
        }
        /* Source-only environments can query declarations before publication. */
        return left->declarationNode == right->declarationNode;
    }

    /* Unkeyed metadata retains the legacy name guard, not artifact identity. */
    return left->name != ZR_NULL && right->name != ZR_NULL &&
           ZrCore_String_Equal(left->name, right->name);
}

static SZrTypeMemberInfo *find_compiler_type_member_recursive_inference(
        SZrCompilerState *cs, const SZrTypePrototypeInfo *prototype,
        SZrString *typeName, SZrString *memberName, TZrUInt32 depth) {
    SZrTypePrototypeInfo prototypeSnapshot;
    SZrArray membersSnapshot;
    SZrArray implementsSnapshot;
    SZrArray inheritsSnapshot;

    if (cs == ZR_NULL || prototype == ZR_NULL || typeName == ZR_NULL ||
        memberName == ZR_NULL || depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_NULL;
    }

    prototypeSnapshot = *prototype;
    prototype = &prototypeSnapshot;
    membersSnapshot = prototype->members;
    implementsSnapshot = prototype->implements;
    inheritsSnapshot = prototype->inherits;

    for (TZrSize index = 0; index < membersSnapshot.length; index++) {
        SZrTypeMemberInfo *memberInfo = ZrCore_Array_Get(&membersSnapshot, index);
        if (memberInfo != ZR_NULL && memberInfo->name != ZR_NULL &&
            ZrCore_String_Equal(memberInfo->name, memberName)) {
            return memberInfo;
        }
    }

    for (TZrSize index = 0; index < implementsSnapshot.length; index++) {
        SZrString **implementedName = ZrCore_Array_Get(&implementsSnapshot, index);
        SZrTypeMemberInfo *memberInfo;
        SZrTypePrototypeInfo *relatedPrototype;

        if (implementedName == ZR_NULL || *implementedName == ZR_NULL) {
            continue;
        }
        relatedPrototype = type_inference_find_member_relation_prototype_root(cs, *implementedName);
        if (relatedPrototype == ZR_NULL ||
            type_inference_member_prototypes_have_same_identity(cs, prototype, relatedPrototype)) {
            continue;
        }
        memberInfo = find_compiler_type_member_recursive_inference(
                cs, relatedPrototype, *implementedName, memberName, depth + 1);
        if (memberInfo != ZR_NULL) {
            return memberInfo;
        }
    }

    for (TZrSize index = 0; index < inheritsSnapshot.length; index++) {
        SZrString **inheritedName = ZrCore_Array_Get(&inheritsSnapshot, index);
        SZrTypeMemberInfo *memberInfo;
        SZrTypePrototypeInfo *relatedPrototype;

        if (inheritedName == ZR_NULL || *inheritedName == ZR_NULL) {
            continue;
        }
        relatedPrototype = type_inference_find_member_relation_prototype_root(cs, *inheritedName);
        if (relatedPrototype == ZR_NULL ||
            type_inference_member_prototypes_have_same_identity(cs, prototype, relatedPrototype)) {
            continue;
        }
        memberInfo = find_compiler_type_member_recursive_inference(
                cs, relatedPrototype, *inheritedName, memberName, depth + 1);
        if (memberInfo != ZR_NULL) {
            return memberInfo;
        }
    }

    return ZR_NULL;
}

SZrTypeMemberInfo *type_inference_find_member_on_prototype(
        SZrCompilerState *cs, const SZrTypePrototypeInfo *prototype,
        SZrString *memberName) {
    SZrTypePrototypeInfo snapshot;
    SZrTypeMemberInfo *memberInfo;
    SZrString *accessorName;

    if (cs == ZR_NULL || prototype == ZR_NULL || prototype->name == ZR_NULL ||
        memberName == ZR_NULL) {
        return ZR_NULL;
    }

    /* Relation lookup can materialize another prototype and grow its table. */
    snapshot = *prototype;
    memberInfo = find_compiler_type_member_recursive_inference(
            cs, &snapshot, snapshot.name, memberName, 0);
    if (memberInfo != ZR_NULL && memberInfo->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_NONE) {
        return memberInfo;
    }

    accessorName = type_inference_create_hidden_property_accessor_name(cs, memberName, ZR_FALSE);
    if (accessorName != ZR_NULL) {
        memberInfo = find_compiler_type_member_recursive_inference(
                cs, &snapshot, snapshot.name, accessorName, 0);
        if (memberInfo != ZR_NULL && memberInfo->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_NONE) {
            return memberInfo;
        }
    }

    accessorName = type_inference_create_hidden_property_accessor_name(cs, memberName, ZR_TRUE);
    if (accessorName != ZR_NULL) {
        memberInfo = find_compiler_type_member_recursive_inference(
                cs, &snapshot, snapshot.name, accessorName, 0);
        if (memberInfo != ZR_NULL && memberInfo->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_NONE) {
            return memberInfo;
        }
    }

    return ZR_NULL;
}

SZrTypeMemberInfo *find_compiler_type_member_inference(
        SZrCompilerState *cs, SZrString *typeName, SZrString *memberName) {
    SZrTypePrototypeInfo *prototype = find_compiler_type_prototype_inference(cs, typeName);
    SZrString *lookupTypeName = typeName;

    if (prototype == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }
    if (type_inference_member_prototype_is_registered(cs, prototype) && prototype->name != ZR_NULL) {
        lookupTypeName = prototype->name;
    }

    prototype = type_inference_find_member_relation_prototype_root(cs, lookupTypeName);
    return type_inference_find_member_on_prototype(cs, prototype, memberName);
}

static TZrBool receiver_has_structured_identity(const SZrInferredType *receiverType) {
    TZrNativeString typeName;

    if (receiverType->nominalDefinitionToken != 0U) {
        return ZR_TRUE;
    }
    if (receiverType->baseType != ZR_VALUE_TYPE_OBJECT || receiverType->elementTypes.length == 0U) {
        return ZR_FALSE;
    }
    typeName = ZrCore_String_GetNativeString(receiverType->typeName);
    return typeName != ZR_NULL && strchr(typeName, '<') != ZR_NULL;
}

SZrTypeMemberInfo *type_inference_find_receiver_member(
        SZrCompilerState *cs, const SZrInferredType *receiverType,
        SZrString *memberName) {
    SZrInferredType snapshot;
    SZrTypePrototypeInfo *prototype;
    SZrTypeMemberInfo *memberInfo;

    if (cs == ZR_NULL || receiverType == ZR_NULL ||
        receiverType->typeName == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }
    if (!receiver_has_structured_identity(receiverType)) {
        return find_compiler_type_member_inference(cs, receiverType->typeName, memberName);
    }

    ZrParser_InferredType_Init(cs->state, &snapshot, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Copy(cs->state, &snapshot, receiverType);
    ensure_generic_instance_type_prototype_from_inferred(cs, &snapshot);
    prototype = type_inference_find_nominal_prototype(cs, &snapshot);
    memberInfo = type_inference_find_member_on_prototype(cs, prototype, memberName);
    ZrParser_InferredType_Free(cs->state, &snapshot);
    return memberInfo;
}

TZrBool type_inference_find_receiver_member_call(
        SZrCompilerState *cs, const SZrInferredType *receiverType,
        SZrString *memberName, SZrFunctionCall *call, SZrFileRange location,
        SZrTypeMemberInfo **outMember) {
    SZrInferredType snapshot;
    SZrTypePrototypeInfo *prototype;
    TZrBool success;

    if (outMember != ZR_NULL) {
        *outMember = ZR_NULL;
    }
    if (cs == ZR_NULL || receiverType == ZR_NULL || receiverType->typeName == ZR_NULL ||
        memberName == ZR_NULL || outMember == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &snapshot, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Copy(cs->state, &snapshot, receiverType);
    if (!receiver_has_structured_identity(&snapshot)) {
        success = find_compiler_type_member_call_inference(
                cs, snapshot.typeName, memberName, call, location, outMember);
    } else {
        ensure_generic_instance_type_prototype_from_inferred(cs, &snapshot);
        prototype = type_inference_find_nominal_prototype(cs, &snapshot);
        success = prototype == ZR_NULL ? ZR_TRUE : type_inference_find_member_call_on_prototype(
                cs, prototype, memberName, call, location, outMember);
    }
    ZrParser_InferredType_Free(cs->state, &snapshot);
    return success;
}
