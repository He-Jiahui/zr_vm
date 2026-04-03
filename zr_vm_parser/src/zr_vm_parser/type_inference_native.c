//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"
#include "type_inference_internal.h"
#include "zr_vm_parser/ast.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_string_conf.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define ZR_MEMBER_PARAMETER_COUNT_UNKNOWN ((TZrUInt32)-1)

static const SZrTypeValue *native_module_info_get_object_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static SZrObject *native_module_info_get_array_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    const SZrTypeValue *value = native_module_info_get_object_field(state, object, fieldName);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_ARRAY) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(state, value->value.object);
}

static SZrString *native_module_info_get_string_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    const SZrTypeValue *value = native_module_info_get_object_field(state, object, fieldName);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING) {
        return ZR_NULL;
    }
    return ZR_CAST_STRING(state, value->value.object);
}

static TZrInt64 native_module_info_get_int_field(SZrState *state,
                                                 SZrObject *object,
                                                 const TZrChar *fieldName,
                                                 TZrInt64 defaultValue) {
    const SZrTypeValue *value = native_module_info_get_object_field(state, object, fieldName);
    if (value == ZR_NULL) {
        return defaultValue;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return (TZrInt64)value->value.nativeObject.nativeUInt64;
    }

    return defaultValue;
}

static TZrBool native_module_info_get_bool_field(SZrState *state,
                                                 SZrObject *object,
                                                 const TZrChar *fieldName,
                                                 TZrBool defaultValue) {
    const SZrTypeValue *value = native_module_info_get_object_field(state, object, fieldName);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_BOOL) {
        return defaultValue;
    }
    return value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
}

static TZrSize native_module_info_array_length(SZrObject *array) {
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return 0;
    }
    return array->nodeMap.elementCount;
}

static SZrObject *native_module_info_array_get_object(SZrState *state, SZrObject *array, TZrSize index) {
    SZrTypeValue key;
    const SZrTypeValue *value;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    value = ZrCore_Object_GetValue(state, array, &key);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, value->value.object);
}

static SZrString *native_module_info_array_get_string(SZrState *state, SZrObject *array, TZrSize index) {
    SZrTypeValue key;
    const SZrTypeValue *value;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    value = ZrCore_Object_GetValue(state, array, &key);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_STRING(state, value->value.object);
}

static void native_module_info_init_prototype(SZrState *state,
                                              SZrTypePrototypeInfo *info,
                                              SZrString *name,
                                              EZrObjectPrototypeType type) {
    if (state == ZR_NULL || info == ZR_NULL) {
        return;
    }

    info->name = name;
    info->type = type;
    info->accessModifier = ZR_ACCESS_PUBLIC;
    info->isImportedNative = ZR_TRUE;
    ZrCore_Array_Init(state, &info->inherits, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state, &info->implements, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state,
                      &info->genericParameters,
                      sizeof(SZrTypeGenericParameterInfo),
                      ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state, &info->members, sizeof(SZrTypeMemberInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    info->extendsTypeName = ZR_NULL;
    info->enumValueTypeName = ZR_NULL;
    info->allowValueConstruction =
            type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE && type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
    info->allowBoxedConstruction =
            type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE && type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
    info->constructorSignature = ZR_NULL;
}

static TZrBool native_module_info_has_member(SZrTypePrototypeInfo *info, SZrString *memberName) {
    if (info == ZR_NULL || memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < info->members.length; i++) {
        SZrTypeMemberInfo *existing = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, i);
        if (existing != ZR_NULL && existing->name != ZR_NULL && ZrCore_String_Equal(existing->name, memberName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool native_module_info_has_inherit(SZrTypePrototypeInfo *info, SZrString *typeName) {
    if (info == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < info->inherits.length; i++) {
        SZrString **existingName = (SZrString **)ZrCore_Array_Get(&info->inherits, i);
        if (existingName != ZR_NULL && *existingName != ZR_NULL && ZrCore_String_Equal(*existingName, typeName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool native_module_info_has_implementation(SZrTypePrototypeInfo *info, SZrString *typeName) {
    if (info == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < info->implements.length; i++) {
        SZrString **existingName = (SZrString **)ZrCore_Array_Get(&info->implements, i);
        if (existingName != ZR_NULL && *existingName != ZR_NULL && ZrCore_String_Equal(*existingName, typeName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void native_module_info_add_inherit(SZrState *state,
                                           SZrTypePrototypeInfo *info,
                                           SZrString *typeName) {
    if (state == ZR_NULL || info == ZR_NULL || typeName == ZR_NULL || native_module_info_has_inherit(info, typeName)) {
        return;
    }

    ZrCore_Array_Push(state, &info->inherits, &typeName);
}

static void native_module_info_add_implementation(SZrState *state,
                                                  SZrTypePrototypeInfo *info,
                                                  SZrString *typeName) {
    if (state == ZR_NULL || info == ZR_NULL || typeName == ZR_NULL ||
        native_module_info_has_implementation(info, typeName)) {
        return;
    }

    ZrCore_Array_Push(state, &info->implements, &typeName);
    native_module_info_add_inherit(state, info, typeName);
}

static void native_module_info_add_field_member(SZrState *state,
                                                SZrTypePrototypeInfo *info,
                                                EZrAstNodeType memberType,
                                                SZrString *memberName,
                                                SZrString *fieldTypeName,
                                                TZrBool isStatic) {
    SZrTypeMemberInfo memberInfo;

    if (state == ZR_NULL || info == ZR_NULL || memberName == ZR_NULL || native_module_info_has_member(info, memberName)) {
        return;
    }

    memset(&memberInfo, 0, sizeof(memberInfo));
    memberInfo.memberType = memberType;
    memberInfo.name = memberName;
    memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo.isStatic = isStatic;
    memberInfo.fieldTypeName = fieldTypeName;
    ZrCore_Array_Push(state, &info->members, &memberInfo);
}

static void native_module_info_copy_parameter_types(SZrCompilerState *cs,
                                                    SZrTypeMemberInfo *memberInfo,
                                                    SZrObject *parametersArray) {
    TZrSize parameterCount;

    if (cs == ZR_NULL || memberInfo == ZR_NULL || parametersArray == ZR_NULL) {
        return;
    }

    parameterCount = native_module_info_array_length(parametersArray);
    if (parameterCount > 0) {
        memberInfo->parameterCount = (TZrUInt32)parameterCount;
    }
    if (parameterCount == 0) {
        return;
    }

    ZrCore_Array_Init(cs->state, &memberInfo->parameterTypes, sizeof(SZrInferredType), parameterCount);
    for (TZrSize index = 0; index < parameterCount; index++) {
        SZrObject *parameterEntry = native_module_info_array_get_object(cs->state, parametersArray, index);
        SZrString *typeName = native_module_info_get_string_field(cs->state, parameterEntry, "typeName");
        SZrInferredType parameterType;

        ZrParser_InferredType_Init(cs->state, &parameterType, ZR_VALUE_TYPE_OBJECT);
        if (!inferred_type_from_type_name(cs, typeName, &parameterType)) {
            ZrParser_InferredType_Free(cs->state, &parameterType);
            continue;
        }

        ZrCore_Array_Push(cs->state, &memberInfo->parameterTypes, &parameterType);
    }
}

static TZrUInt32 native_module_info_exact_parameter_count(SZrState *state, SZrObject *entry) {
    TZrInt64 parameterCount;
    TZrInt64 minArgumentCount;
    TZrInt64 maxArgumentCount;

    if (state == ZR_NULL || entry == ZR_NULL) {
        return ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    }

    parameterCount = native_module_info_get_int_field(state, entry, "parameterCount", -1);
    if (parameterCount >= 0) {
        return (TZrUInt32)parameterCount;
    }

    minArgumentCount = native_module_info_get_int_field(state, entry, "minArgumentCount", -1);
    maxArgumentCount = native_module_info_get_int_field(state, entry, "maxArgumentCount", -1);
    if (minArgumentCount >= 0 && maxArgumentCount >= 0 && minArgumentCount == maxArgumentCount) {
        return (TZrUInt32)minArgumentCount;
    }

    return ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
}

static void native_module_info_add_method_member(SZrCompilerState *cs,
                                                 SZrTypePrototypeInfo *info,
                                                 EZrAstNodeType memberType,
                                                 SZrString *memberName,
                                                 SZrString *returnTypeName,
                                                 TZrBool isStatic,
                                                 TZrUInt32 parameterCount,
                                                 SZrObject *parametersArray) {
    SZrTypeMemberInfo memberInfo;

    if (cs == ZR_NULL || info == ZR_NULL || memberName == ZR_NULL || native_module_info_has_member(info, memberName)) {
        return;
    }

    memset(&memberInfo, 0, sizeof(memberInfo));
    memberInfo.memberType = memberType;
    memberInfo.name = memberName;
    memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo.isStatic = isStatic;
    memberInfo.parameterCount = parameterCount;
    memberInfo.returnTypeName = returnTypeName;
    native_module_info_copy_parameter_types(cs, &memberInfo, parametersArray);
    ZrCore_Array_Push(cs->state, &info->members, &memberInfo);
}

static void native_module_info_add_meta_method_member(SZrCompilerState *cs,
                                                      SZrTypePrototypeInfo *info,
                                                      EZrMetaType metaType,
                                                      SZrString *returnTypeName,
                                                      TZrUInt32 parameterCount,
                                                      SZrObject *parametersArray) {
    SZrTypeMemberInfo memberInfo;
    const TZrChar *memberNameText;
    SZrString *memberName;

    if (cs == ZR_NULL || info == ZR_NULL || metaType >= ZR_META_ENUM_MAX) {
        return;
    }

    memberNameText = metaType == ZR_META_CONSTRUCTOR ? "__constructor" : CZrMetaName[metaType];
    if (memberNameText == ZR_NULL) {
        return;
    }

    memberName = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)memberNameText);
    if (memberName == ZR_NULL || native_module_info_has_member(info, memberName)) {
        return;
    }

    memset(&memberInfo, 0, sizeof(memberInfo));
    memberInfo.memberType = info->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT
                                    ? ZR_AST_STRUCT_META_FUNCTION
                                    : ZR_AST_CLASS_META_FUNCTION;
    memberInfo.name = memberName;
    memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo.metaType = metaType;
    memberInfo.isMetaMethod = ZR_TRUE;
    memberInfo.parameterCount = parameterCount;
    memberInfo.returnTypeName = returnTypeName;
    native_module_info_copy_parameter_types(cs, &memberInfo, parametersArray);
    ZrCore_Array_Push(cs->state, &info->members, &memberInfo);
}

static void native_module_info_add_generic_parameter(SZrState *state,
                                                     SZrTypePrototypeInfo *info,
                                                     SZrObject *genericParameterEntry) {
    SZrString *name;
    SZrObject *constraintsArray;
    SZrTypeGenericParameterInfo genericInfo;

    if (state == ZR_NULL || info == ZR_NULL || genericParameterEntry == ZR_NULL) {
        return;
    }

    name = native_module_info_get_string_field(state, genericParameterEntry, "name");
    if (name == ZR_NULL) {
        return;
    }

    memset(&genericInfo, 0, sizeof(genericInfo));
    genericInfo.name = name;
    ZrCore_Array_Init(state,
                      &genericInfo.constraintTypeNames,
                      sizeof(SZrString *),
                      ZR_PARSER_INITIAL_CAPACITY_PAIR);

    constraintsArray = native_module_info_get_array_field(state, genericParameterEntry, "constraints");
    for (TZrSize index = 0; index < native_module_info_array_length(constraintsArray); index++) {
        SZrString *constraintTypeName = native_module_info_array_get_string(state, constraintsArray, index);
        if (constraintTypeName != ZR_NULL) {
            ZrCore_Array_Push(state, &genericInfo.constraintTypeNames, &constraintTypeName);
        }
    }

    ZrCore_Array_Push(state, &info->genericParameters, &genericInfo);
}

TZrBool inferred_type_from_type_name(SZrCompilerState *cs, SZrString *typeName, SZrInferredType *result) {
    TZrNativeString nativeTypeName;
    TZrSize nativeTypeNameLength;
    SZrString *genericBaseName = ZR_NULL;
    SZrArray genericArgumentTypeNames;

    if (cs == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeName == ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    }

    nativeTypeName = ZrCore_String_GetNativeString(typeName);
    nativeTypeNameLength = nativeTypeName != ZR_NULL ? strlen(nativeTypeName) : 0;
    if (nativeTypeName != ZR_NULL &&
        nativeTypeNameLength > 2 &&
        nativeTypeName[nativeTypeNameLength - 1] == ']') {
        const TZrChar *bracket = strrchr(nativeTypeName, '[');
        TZrSize elementTypeNameLength;
        TZrSize sizeTextLength;
        SZrString *elementTypeName;
        SZrInferredType elementType;
        TZrSize fixedSize = 0;
        TZrBool hasFixedSizeConstraint = ZR_FALSE;

        if (bracket != ZR_NULL && bracket > nativeTypeName) {
            elementTypeNameLength = (TZrSize)(bracket - nativeTypeName);
            sizeTextLength = nativeTypeNameLength - elementTypeNameLength - 2;

            if (sizeTextLength > 0) {
                for (TZrSize index = 0; index < sizeTextLength; index++) {
                    TZrChar ch = bracket[1 + index];
                    if (ch < '0' || ch > '9') {
                        hasFixedSizeConstraint = ZR_FALSE;
                        fixedSize = 0;
                        goto not_array_type_name;
                    }
                    fixedSize = fixedSize * 10 + (TZrSize)(ch - '0');
                }
                hasFixedSizeConstraint = ZR_TRUE;
            }

            elementTypeName = ZrCore_String_Create(cs->state, nativeTypeName, elementTypeNameLength);
            if (elementTypeName == ZR_NULL) {
                return ZR_FALSE;
            }

            ZrParser_InferredType_Init(cs->state, &elementType, ZR_VALUE_TYPE_OBJECT);
            if (!inferred_type_from_type_name(cs, elementTypeName, &elementType)) {
                ZrParser_InferredType_Free(cs->state, &elementType);
                return ZR_FALSE;
            }

            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_ARRAY);
            ZrCore_Array_Init(cs->state, &result->elementTypes, sizeof(SZrInferredType), 1);
            ZrCore_Array_Push(cs->state, &result->elementTypes, &elementType);
            result->typeName = typeName;
            result->hasArraySizeConstraint = hasFixedSizeConstraint;
            result->arrayFixedSize = hasFixedSizeConstraint ? fixedSize : 0;
            result->arrayMinSize = hasFixedSizeConstraint ? fixedSize : 0;
            result->arrayMaxSize = hasFixedSizeConstraint ? fixedSize : 0;
            return ZR_TRUE;
        }
    }
not_array_type_name:

    ZrCore_Array_Construct(&genericArgumentTypeNames);
    if (try_parse_generic_instance_type_name(cs->state, typeName, &genericBaseName, &genericArgumentTypeNames)) {
        ZrParser_InferredType_InitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
        ZrCore_Array_Init(cs->state,
                          &result->elementTypes,
                          sizeof(SZrInferredType),
                          genericArgumentTypeNames.length);
        for (TZrSize index = 0; index < genericArgumentTypeNames.length; index++) {
            SZrString **argumentTypeNamePtr = (SZrString **)ZrCore_Array_Get(&genericArgumentTypeNames, index);
            SZrInferredType argumentType;
            if (argumentTypeNamePtr == ZR_NULL || *argumentTypeNamePtr == ZR_NULL) {
                continue;
            }
            ZrParser_InferredType_Init(cs->state, &argumentType, ZR_VALUE_TYPE_OBJECT);
            if (!inferred_type_from_type_name(cs, *argumentTypeNamePtr, &argumentType)) {
                ZrParser_InferredType_Free(cs->state, &argumentType);
                continue;
            }
            ZrCore_Array_Push(cs->state, &result->elementTypes, &argumentType);
        }
        ensure_generic_instance_type_prototype(cs, typeName);
        ZrCore_Array_Free(cs->state, &genericArgumentTypeNames);
        if (cs->typeEnv != ZR_NULL) {
            ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, typeName);
        }
        return ZR_TRUE;
    }

    if (zr_string_equals_cstr(typeName, "null")) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_NULL);
        return ZR_TRUE;
    }
    if (zr_string_equals_cstr(typeName, "bool")) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_BOOL);
        return ZR_TRUE;
    }
    if (zr_string_equals_cstr(typeName, "int")) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_INT64);
        return ZR_TRUE;
    }
    if (zr_string_equals_cstr(typeName, "float")) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_DOUBLE);
        return ZR_TRUE;
    }
    if (zr_string_equals_cstr(typeName, "string")) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_STRING);
        return ZR_TRUE;
    }
    if (zr_string_equals_cstr(typeName, "array")) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_ARRAY);
        return ZR_TRUE;
    }
    if (zr_string_equals_cstr(typeName, "function")) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_CLOSURE);
        return ZR_TRUE;
    }
    if (zr_string_equals_cstr(typeName, "object") ||
        zr_string_equals_cstr(typeName, "value") ||
        zr_string_equals_cstr(typeName, "any")) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return ZR_TRUE;
    }

    ZrParser_InferredType_InitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, typeName);
    }
    return ZR_TRUE;
}

static TZrBool type_inference_resolve_member_segment_names(SZrCompilerState *cs,
                                                           SZrAstNode *propertyNode,
                                                           SZrString **outLookupName,
                                                           SZrString **outResolvedTypeName) {
    if (outLookupName != ZR_NULL) {
        *outLookupName = ZR_NULL;
    }
    if (outResolvedTypeName != ZR_NULL) {
        *outResolvedTypeName = ZR_NULL;
    }
    if (cs == ZR_NULL || propertyNode == ZR_NULL || outLookupName == ZR_NULL || outResolvedTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (propertyNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        *outLookupName = propertyNode->data.identifier.name;
        *outResolvedTypeName = propertyNode->data.identifier.name;
        return *outLookupName != ZR_NULL;
    }

    if (propertyNode->type == ZR_AST_TYPE) {
        SZrType *propertyType = &propertyNode->data.type;
        SZrInferredType inferredType;

        if (propertyType->name == ZR_NULL) {
            return ZR_FALSE;
        }

        if (propertyType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
            *outLookupName = propertyType->name->data.identifier.name;
        } else if (propertyType->name->type == ZR_AST_GENERIC_TYPE &&
                   propertyType->name->data.genericType.name != ZR_NULL) {
            *outLookupName = propertyType->name->data.genericType.name->name;
        }

        if (*outLookupName == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_AstTypeToInferredType_Convert(cs, propertyType, &inferredType)) {
            ZrParser_InferredType_Free(cs->state, &inferredType);
            return ZR_FALSE;
        }

        *outResolvedTypeName = inferredType.typeName;
        ZrParser_InferredType_Free(cs->state, &inferredType);
        return *outResolvedTypeName != ZR_NULL;
    }

    return ZR_FALSE;
}

static SZrTypeMemberInfo *find_compiler_type_meta_member_inference(SZrCompilerState *cs,
                                                                   SZrString *typeName,
                                                                   EZrMetaType metaType) {
    SZrTypePrototypeInfo *info;

    if (cs == ZR_NULL || typeName == ZR_NULL || metaType >= ZR_META_ENUM_MAX) {
        return ZR_NULL;
    }

    info = find_compiler_type_prototype_inference(cs, typeName);
    if (info == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < info->members.length; index++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, index);
        if (memberInfo != ZR_NULL && memberInfo->isMetaMethod && memberInfo->metaType == metaType) {
            return memberInfo;
        }
    }

    for (TZrSize index = 0; index < info->inherits.length; index++) {
        SZrString **inheritNamePtr = (SZrString **)ZrCore_Array_Get(&info->inherits, index);
        SZrTypeMemberInfo *inheritedMember;

        if (inheritNamePtr == ZR_NULL || *inheritNamePtr == ZR_NULL) {
            continue;
        }

        inheritedMember = find_compiler_type_meta_member_inference(cs, *inheritNamePtr, metaType);
        if (inheritedMember != ZR_NULL) {
            return inheritedMember;
        }
    }

    return ZR_NULL;
}

static TZrBool inferred_type_from_member_access(SZrCompilerState *cs,
                                                const SZrTypeMemberInfo *memberInfo,
                                                SZrInferredType *result) {
    TZrNativeString memberNameString = ZR_NULL;

    if (cs == ZR_NULL || memberInfo == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (memberInfo->name != ZR_NULL) {
        memberNameString = ZrCore_String_GetNativeStringShort(memberInfo->name);
        if (memberNameString == ZR_NULL) {
            memberNameString = ZrCore_String_GetNativeString(memberInfo->name);
        }
    }

    switch (memberInfo->memberType) {
        case ZR_AST_STRUCT_FIELD:
        case ZR_AST_CLASS_FIELD:
            if (!inferred_type_from_type_name(cs, memberInfo->fieldTypeName, result)) {
                return ZR_FALSE;
            }
            result->ownershipQualifier = memberInfo->ownershipQualifier;
            return ZR_TRUE;
        case ZR_AST_STRUCT_METHOD:
        case ZR_AST_CLASS_METHOD:
        case ZR_AST_STRUCT_META_FUNCTION:
        case ZR_AST_CLASS_META_FUNCTION:
            if (memberNameString != ZR_NULL && strncmp(memberNameString, "__get_", 6) == 0 &&
                memberInfo->returnTypeName != ZR_NULL) {
                return inferred_type_from_type_name(cs, memberInfo->returnTypeName, result);
            }
            if (memberNameString != ZR_NULL && strncmp(memberNameString, "__set_", 6) == 0 &&
                memberInfo->parameterTypes.length > 0) {
                SZrInferredType *parameterType =
                        (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterTypes, 0);
                if (parameterType != ZR_NULL) {
                    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                    ZrParser_InferredType_Copy(cs->state, result, parameterType);
                    return ZR_TRUE;
                }
            }
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_CLOSURE);
            return ZR_TRUE;
        default:
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            return ZR_TRUE;
    }
}

static TZrBool inferred_type_from_member_call(SZrCompilerState *cs,
                                              const SZrTypeMemberInfo *memberInfo,
                                              const SZrResolvedCallSignature *resolvedSignature,
                                              SZrInferredType *result) {
    if (cs == ZR_NULL || memberInfo == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (resolvedSignature != ZR_NULL) {
        ZrParser_InferredType_Copy(cs->state, result, &resolvedSignature->returnType);
        return ZR_TRUE;
    }

    switch (memberInfo->memberType) {
        case ZR_AST_STRUCT_METHOD:
        case ZR_AST_CLASS_METHOD:
        case ZR_AST_STRUCT_META_FUNCTION:
        case ZR_AST_CLASS_META_FUNCTION:
            return inferred_type_from_type_name(cs, memberInfo->returnTypeName, result);
        default:
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
            return ZR_TRUE;
    }
}

static TZrBool validate_member_call_arguments(SZrCompilerState *cs,
                                              const SZrTypeMemberInfo *memberInfo,
                                              const SZrResolvedCallSignature *resolvedSignature,
                                              SZrFunctionCall *call,
                                              SZrFileRange location) {
    TZrSize argCount;
    SZrArray argTypes;
    const SZrArray *parameterTypes;
    const SZrArray *parameterPassingModes;
    TZrUInt32 parameterCount;

    if (cs == ZR_NULL || memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (resolvedSignature != ZR_NULL) {
        parameterTypes = &resolvedSignature->parameterTypes;
        parameterPassingModes = &resolvedSignature->parameterPassingModes;
        parameterCount = resolvedSignature->parameterTypes.length > 0
                                 ? (TZrUInt32)resolvedSignature->parameterTypes.length
                                 : memberInfo->parameterCount;
    } else {
        parameterTypes = &memberInfo->parameterTypes;
        parameterPassingModes = &memberInfo->parameterPassingModes;
        parameterCount = memberInfo->parameterCount;
    }
    argCount = (call != ZR_NULL && call->args != ZR_NULL) ? call->args->count : 0;
    if (memberInfo->parameterCount == ZR_MEMBER_PARAMETER_COUNT_UNKNOWN &&
        memberInfo->parameterTypes.length == 0 &&
        parameterTypes->length == 0) {
        return ZR_TRUE;
    }

    if (parameterCount != (TZrUInt32)argCount) {
        ZrParser_Compiler_Error(cs, "Argument count mismatch", location);
        return ZR_FALSE;
    }

    if (parameterTypes->length == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(cs->state, &argTypes, sizeof(SZrInferredType), parameterTypes->length);
    for (TZrSize index = 0; index < parameterTypes->length; index++) {
        SZrAstNode *argNode;
        SZrInferredType *paramType;
        SZrInferredType argType;

        if (call == ZR_NULL || call->args == ZR_NULL || index >= call->args->count) {
            free_inferred_type_array(cs->state, &argTypes);
            ZrParser_Compiler_Error(cs, "Argument count mismatch", location);
            return ZR_FALSE;
        }

        argNode = call->args->nodes[index];
        paramType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)parameterTypes, index);
        if (argNode == ZR_NULL || paramType == ZR_NULL) {
            free_inferred_type_array(cs->state, &argTypes);
            return ZR_FALSE;
        }

        ZrParser_InferredType_Init(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_ExpressionType_Infer(cs, argNode, &argType)) {
            free_inferred_type_array(cs->state, &argTypes);
            ZrParser_InferredType_Free(cs->state, &argType);
            return ZR_FALSE;
        }
        ZrCore_Array_Push(cs->state, &argTypes, &argType);
    }

    if (!validate_call_argument_passing_modes(cs,
                                              parameterPassingModes,
                                              parameterTypes,
                                              call,
                                              &argTypes)) {
        free_inferred_type_array(cs->state, &argTypes);
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < parameterTypes->length; index++) {
        EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
        EZrParameterPassingMode *modePtr = ZR_NULL;
        SZrInferredType *paramType =
                (SZrInferredType *)ZrCore_Array_Get((SZrArray *)parameterTypes, index);
        SZrInferredType *argType = (SZrInferredType *)ZrCore_Array_Get(&argTypes, index);
        SZrAstNode *argNode = call != ZR_NULL && call->args != ZR_NULL && index < call->args->count
                                      ? call->args->nodes[index]
                                      : ZR_NULL;

        if (paramType == ZR_NULL || argType == ZR_NULL) {
            free_inferred_type_array(cs->state, &argTypes);
            return ZR_FALSE;
        }

        if (parameterPassingModes != ZR_NULL && index < parameterPassingModes->length) {
            modePtr = (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)parameterPassingModes, index);
            if (modePtr != ZR_NULL) {
                passingMode = *modePtr;
            }
        }

        if (passingMode == ZR_PARAMETER_PASSING_MODE_OUT || passingMode == ZR_PARAMETER_PASSING_MODE_REF) {
            continue;
        }

        if (!ZrParser_InferredType_IsCompatible(argType, paramType)) {
            ZrParser_TypeError_Report(cs,
                                      "Argument type mismatch",
                                      paramType,
                                      argType,
                                      argNode != ZR_NULL ? argNode->location : location);
            free_inferred_type_array(cs->state, &argTypes);
            return ZR_FALSE;
        }
    }

    free_inferred_type_array(cs->state, &argTypes);
    return ZR_TRUE;
}

TZrBool infer_import_expression_type(SZrCompilerState *cs,
                                     SZrAstNode *node,
                                     SZrInferredType *result) {
    SZrAstNode *modulePathNode;
    SZrString *moduleName = ZR_NULL;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_IMPORT_EXPRESSION) {
        return ZR_FALSE;
    }

    modulePathNode = node->data.importExpression.modulePath;
    if (modulePathNode != ZR_NULL && modulePathNode->type == ZR_AST_STRING_LITERAL) {
        moduleName = modulePathNode->data.stringLiteral.value;
    }

    if (moduleName != ZR_NULL) {
        ensure_import_module_compile_info(cs, moduleName);
        ZrParser_InferredType_InitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, moduleName);
    } else {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
    }

    return ZR_TRUE;
}

TZrBool infer_type_query_expression_type(SZrCompilerState *cs,
                                         SZrAstNode *node,
                                         SZrInferredType *result) {
    SZrString *reflectionTypeName;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_TYPE_QUERY_EXPRESSION) {
        return ZR_FALSE;
    }

    if (node->data.typeQueryExpression.operand == ZR_NULL) {
        return ZR_FALSE;
    }

    reflectionTypeName = ZrCore_String_Create(cs->state, "zr.system.reflect.Type", 22);
    if (reflectionTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_InitFull(cs->state, result, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, reflectionTypeName);
    return ZR_TRUE;
}

static TZrBool receiver_ownership_can_call_member(EZrOwnershipQualifier receiverQualifier,
                                                  EZrOwnershipQualifier memberQualifier) {
    switch (receiverQualifier) {
        case ZR_OWNERSHIP_QUALIFIER_NONE:
            return ZR_TRUE;
        case ZR_OWNERSHIP_QUALIFIER_WEAK:
            return memberQualifier == ZR_OWNERSHIP_QUALIFIER_WEAK ||
                   memberQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED ||
                   memberQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED;
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
            return memberQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED ||
                   memberQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED;
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE:
            return memberQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED;
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
            return memberQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED;
        default:
            return ZR_FALSE;
    }
}

const TZrChar *receiver_ownership_call_error(EZrOwnershipQualifier receiverQualifier) {
    switch (receiverQualifier) {
        case ZR_OWNERSHIP_QUALIFIER_WEAK:
            return "Weak-owned receivers can only call %weak, %shared, or %borrowed methods";
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
            return "Shared-owned receivers can only call %shared or %borrowed methods";
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE:
            return "Unique-owned receivers can only call %borrowed methods";
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
            return "Borrowed receivers can only call %borrowed methods";
        case ZR_OWNERSHIP_QUALIFIER_NONE:
        default:
            return "Receiver ownership qualifier is not compatible with this method";
    }
}

SZrString *extract_imported_module_name(SZrFunctionCall *call) {
    if (call == ZR_NULL || call->args == ZR_NULL || call->args->count == 0) {
        return ZR_NULL;
    }

    if (call->args->nodes[0] != ZR_NULL && call->args->nodes[0]->type == ZR_AST_STRING_LITERAL) {
        return call->args->nodes[0]->data.stringLiteral.value;
    }

    return ZR_NULL;
}

TZrBool ensure_native_module_compile_info(SZrCompilerState *cs, SZrString *moduleName) {
    SZrObjectModule *module;
    SZrObject *moduleInfo;
    SZrObject *functionsArray;
    SZrObject *constantsArray;
    SZrObject *typesArray;
    SZrObject *modulesArray;
    SZrTypePrototypeInfo modulePrototype;
    TZrUInt64 pathHash;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (find_compiler_type_prototype_inference(cs, moduleName) != ZR_NULL) {
        return ZR_TRUE;
    }

    module = ZrCore_Module_GetFromCache(cs->state, moduleName);
    if (module == ZR_NULL && cs->state->global->nativeModuleLoader != ZR_NULL) {
        module = cs->state->global->nativeModuleLoader(cs->state,
                                                       moduleName,
                                                       cs->state->global->nativeModuleLoaderUserData);
        if (module != ZR_NULL) {
            if (module->fullPath == ZR_NULL || module->moduleName == ZR_NULL) {
                pathHash = ZrCore_Module_CalculatePathHash(cs->state, moduleName);
                ZrCore_Module_SetInfo(cs->state, module, moduleName, pathHash, moduleName);
            }
            ZrCore_Module_AddToCache(cs->state, moduleName, module);
        }
    }

    if (module == ZR_NULL) {
        return ZR_FALSE;
    }

    {
        SZrString *infoName = ZrCore_String_Create(cs->state,
                                                   ZR_NATIVE_MODULE_INFO_EXPORT_NAME,
                                                   strlen(ZR_NATIVE_MODULE_INFO_EXPORT_NAME));
        const SZrTypeValue *moduleInfoValue;
        if (infoName == ZR_NULL) {
            return ZR_FALSE;
        }

        moduleInfoValue = ZrCore_Module_GetPubExport(cs->state, module, infoName);
        if (moduleInfoValue == ZR_NULL || moduleInfoValue->type != ZR_VALUE_TYPE_OBJECT) {
            return ZR_FALSE;
        }

        moduleInfo = ZR_CAST_OBJECT(cs->state, moduleInfoValue->value.object);
    }

    native_module_info_init_prototype(cs->state, &modulePrototype, moduleName, ZR_OBJECT_PROTOTYPE_TYPE_MODULE);

    functionsArray = native_module_info_get_array_field(cs->state, moduleInfo, "functions");
    for (TZrSize i = 0; i < native_module_info_array_length(functionsArray); i++) {
        SZrObject *entry = native_module_info_array_get_object(cs->state, functionsArray, i);
        SZrString *name = native_module_info_get_string_field(cs->state, entry, "name");
        SZrString *returnTypeName = native_module_info_get_string_field(cs->state, entry, "returnTypeName");
        TZrUInt32 parameterCount = native_module_info_exact_parameter_count(cs->state, entry);
        SZrObject *parametersArray = native_module_info_get_array_field(cs->state, entry, "parameters");
        if (name != ZR_NULL) {
            native_module_info_add_method_member(cs,
                                                 &modulePrototype,
                                                 ZR_AST_CLASS_METHOD,
                                                 name,
                                                 returnTypeName,
                                                 ZR_TRUE,
                                                 parameterCount,
                                                 parametersArray);
        }
    }

    constantsArray = native_module_info_get_array_field(cs->state, moduleInfo, "constants");
    for (TZrSize i = 0; i < native_module_info_array_length(constantsArray); i++) {
        SZrObject *entry = native_module_info_array_get_object(cs->state, constantsArray, i);
        SZrString *name = native_module_info_get_string_field(cs->state, entry, "name");
        SZrString *typeName = native_module_info_get_string_field(cs->state, entry, "typeName");
        if (name != ZR_NULL) {
            native_module_info_add_field_member(cs->state,
                                                &modulePrototype,
                                                ZR_AST_CLASS_FIELD,
                                                name,
                                                typeName,
                                                ZR_TRUE);
        }
    }

    modulesArray = native_module_info_get_array_field(cs->state, moduleInfo, "modules");
    for (TZrSize i = 0; i < native_module_info_array_length(modulesArray); i++) {
        SZrObject *entry = native_module_info_array_get_object(cs->state, modulesArray, i);
        SZrString *name = native_module_info_get_string_field(cs->state, entry, "name");
        SZrString *linkedModuleName = native_module_info_get_string_field(cs->state, entry, "moduleName");

        if (name == ZR_NULL || linkedModuleName == ZR_NULL) {
            continue;
        }

        ensure_native_module_compile_info(cs, linkedModuleName);
        native_module_info_add_field_member(cs->state,
                                            &modulePrototype,
                                            ZR_AST_CLASS_FIELD,
                                            name,
                                            linkedModuleName,
                                            ZR_TRUE);
    }

    typesArray = native_module_info_get_array_field(cs->state, moduleInfo, "types");
    for (TZrSize i = 0; i < native_module_info_array_length(typesArray); i++) {
        SZrObject *entry = native_module_info_array_get_object(cs->state, typesArray, i);
        SZrString *name = native_module_info_get_string_field(cs->state, entry, "name");
        TZrInt64 prototypeTypeValue = native_module_info_get_int_field(cs->state,
                                                                       entry,
                                                                       "prototypeType",
                                                                       ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrString *extendsTypeName = native_module_info_get_string_field(cs->state, entry, "extendsTypeName");
        SZrObject *implementsArray = native_module_info_get_array_field(cs->state, entry, "implements");
        SZrObject *enumMembersArray = native_module_info_get_array_field(cs->state, entry, "enumMembers");
        SZrString *enumValueTypeName = native_module_info_get_string_field(cs->state, entry, "enumValueTypeName");
        TZrBool allowValueConstruction = native_module_info_get_bool_field(
                cs->state,
                entry,
                "allowValueConstruction",
                prototypeTypeValue != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE &&
                        prototypeTypeValue != ZR_OBJECT_PROTOTYPE_TYPE_MODULE);
        TZrBool allowBoxedConstruction = native_module_info_get_bool_field(
                cs->state,
                entry,
                "allowBoxedConstruction",
                prototypeTypeValue != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE &&
                        prototypeTypeValue != ZR_OBJECT_PROTOTYPE_TYPE_MODULE);
        SZrString *constructorSignature = native_module_info_get_string_field(cs->state, entry, "constructorSignature");
        SZrObject *fieldsArray = native_module_info_get_array_field(cs->state, entry, "fields");
        SZrObject *methodsArray = native_module_info_get_array_field(cs->state, entry, "methods");
        SZrObject *metaMethodsArray = native_module_info_get_array_field(cs->state, entry, "metaMethods");
        SZrObject *genericParametersArray = native_module_info_get_array_field(cs->state, entry, "genericParameters");
        SZrTypePrototypeInfo typePrototype;
        EZrAstNodeType fieldMemberType;
        EZrAstNodeType methodMemberType;

        if (name == ZR_NULL) {
            continue;
        }

        if (cs->typeEnv != ZR_NULL) {
            ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, name);
        }

        native_module_info_add_field_member(cs->state,
                                            &modulePrototype,
                                            ZR_AST_CLASS_FIELD,
                                            name,
                                            name,
                                            ZR_TRUE);

        if (find_compiler_type_prototype_inference(cs, name) != ZR_NULL) {
            continue;
        }

        native_module_info_init_prototype(cs->state,
                                          &typePrototype,
                                          name,
                                          (EZrObjectPrototypeType)prototypeTypeValue);
        typePrototype.extendsTypeName = extendsTypeName;
        typePrototype.enumValueTypeName = enumValueTypeName;
        typePrototype.allowValueConstruction = allowValueConstruction;
        typePrototype.allowBoxedConstruction = allowBoxedConstruction;
        typePrototype.constructorSignature = constructorSignature;
        if (extendsTypeName != ZR_NULL) {
            native_module_info_add_inherit(cs->state, &typePrototype, extendsTypeName);
        }
        for (TZrSize genericIndex = 0; genericIndex < native_module_info_array_length(genericParametersArray);
             genericIndex++) {
            SZrObject *genericParameterEntry =
                    native_module_info_array_get_object(cs->state, genericParametersArray, genericIndex);
            native_module_info_add_generic_parameter(cs->state, &typePrototype, genericParameterEntry);
        }
        for (TZrSize implementsIndex = 0;
             implementsIndex < native_module_info_array_length(implementsArray);
             implementsIndex++) {
            SZrString *implementedTypeName =
                    native_module_info_array_get_string(cs->state, implementsArray, implementsIndex);
            if (implementedTypeName != ZR_NULL) {
                native_module_info_add_implementation(cs->state, &typePrototype, implementedTypeName);
            }
        }
        fieldMemberType = prototypeTypeValue == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT ? ZR_AST_STRUCT_FIELD : ZR_AST_CLASS_FIELD;
        methodMemberType = prototypeTypeValue == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT ? ZR_AST_STRUCT_METHOD : ZR_AST_CLASS_METHOD;

        for (TZrSize fieldIndex = 0; fieldIndex < native_module_info_array_length(fieldsArray); fieldIndex++) {
            SZrObject *fieldEntry = native_module_info_array_get_object(cs->state, fieldsArray, fieldIndex);
            SZrString *fieldName = native_module_info_get_string_field(cs->state, fieldEntry, "name");
            SZrString *fieldTypeName = native_module_info_get_string_field(cs->state, fieldEntry, "typeName");
            if (fieldName != ZR_NULL) {
                native_module_info_add_field_member(cs->state,
                                                    &typePrototype,
                                                    fieldMemberType,
                                                    fieldName,
                                                    fieldTypeName,
                                                    ZR_FALSE);
            }
        }

        for (TZrSize methodIndex = 0; methodIndex < native_module_info_array_length(methodsArray); methodIndex++) {
            SZrObject *methodEntry = native_module_info_array_get_object(cs->state, methodsArray, methodIndex);
            SZrString *methodName = native_module_info_get_string_field(cs->state, methodEntry, "name");
            SZrString *returnTypeName = native_module_info_get_string_field(cs->state, methodEntry, "returnTypeName");
            TZrBool isStatic = native_module_info_get_bool_field(cs->state, methodEntry, "isStatic", ZR_FALSE);
            TZrUInt32 parameterCount = native_module_info_exact_parameter_count(cs->state, methodEntry);
            SZrObject *parametersArray = native_module_info_get_array_field(cs->state, methodEntry, "parameters");
            if (methodName != ZR_NULL) {
                native_module_info_add_method_member(cs,
                                                     &typePrototype,
                                                     methodMemberType,
                                                     methodName,
                                                     returnTypeName,
                                                     isStatic,
                                                     parameterCount,
                                                     parametersArray);
            }
        }

        for (TZrSize metaIndex = 0; metaIndex < native_module_info_array_length(metaMethodsArray); metaIndex++) {
            SZrObject *metaEntry = native_module_info_array_get_object(cs->state, metaMethodsArray, metaIndex);
            TZrInt64 metaTypeValue =
                    native_module_info_get_int_field(cs->state, metaEntry, "metaType", ZR_META_ENUM_MAX);
            SZrString *returnTypeName = native_module_info_get_string_field(cs->state, metaEntry, "returnTypeName");
            TZrUInt32 parameterCount = native_module_info_exact_parameter_count(cs->state, metaEntry);
            SZrObject *parametersArray = native_module_info_get_array_field(cs->state, metaEntry, "parameters");

            if (metaTypeValue < 0 || metaTypeValue >= ZR_META_ENUM_MAX) {
                continue;
            }

            native_module_info_add_meta_method_member(cs,
                                                      &typePrototype,
                                                      (EZrMetaType)metaTypeValue,
                                                      returnTypeName,
                                                      parameterCount,
                                                      parametersArray);
        }

        for (TZrSize enumMemberIndex = 0; enumMemberIndex < native_module_info_array_length(enumMembersArray);
             enumMemberIndex++) {
            SZrObject *enumMemberEntry = native_module_info_array_get_object(cs->state, enumMembersArray, enumMemberIndex);
            SZrString *enumMemberName = native_module_info_get_string_field(cs->state, enumMemberEntry, "name");
            if (enumMemberName != ZR_NULL) {
                native_module_info_add_field_member(cs->state,
                                                    &typePrototype,
                                                    ZR_AST_CLASS_FIELD,
                                                    enumMemberName,
                                                    name,
                                                    ZR_TRUE);
            }
        }

        ZrCore_Array_Push(cs->state, &cs->typePrototypes, &typePrototype);
    }

    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, moduleName);
    }
    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &modulePrototype);
    return ZR_TRUE;
}

TZrBool infer_primary_member_chain_type(SZrCompilerState *cs,
                                        const SZrInferredType *baseType,
                                        SZrAstNodeArray *members,
                                        TZrSize startIndex,
                                        TZrBool baseIsPrototypeReference,
                                        SZrInferredType *result) {
    SZrInferredType currentType;
    TZrBool currentIsPrototypeReference = baseIsPrototypeReference;

    if (cs == ZR_NULL || baseType == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &currentType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Copy(cs->state, &currentType, baseType);

    if (members != ZR_NULL) {
        for (TZrSize i = startIndex; i < members->count; i++) {
            SZrAstNode *memberNode = members->nodes[i];

            if (memberNode == ZR_NULL) {
                continue;
            }

            if (memberNode->type == ZR_AST_MEMBER_EXPRESSION) {
                SZrMemberExpression *memberExpr = &memberNode->data.memberExpression;
                SZrString *memberLookupName = ZR_NULL;
                SZrString *memberResolvedTypeName = ZR_NULL;
                SZrTypeMemberInfo *memberInfo;
                SZrInferredType nextType;
                TZrBool nextIsPrototypeReference = ZR_FALSE;
                TZrBool nextIsFunctionCall =
                        i + 1 < members->count &&
                        members->nodes[i + 1] != ZR_NULL &&
                        members->nodes[i + 1]->type == ZR_AST_FUNCTION_CALL;

                if (memberExpr->computed && currentType.baseType == ZR_VALUE_TYPE_ARRAY) {
                    ZrParser_InferredType_Init(cs->state, &nextType, ZR_VALUE_TYPE_OBJECT);
                    if (currentType.elementTypes.length > 0) {
                        SZrInferredType *elementType =
                                (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&currentType.elementTypes, 0);
                        if (elementType != ZR_NULL) {
                            ZrParser_InferredType_Copy(cs->state, &nextType, elementType);
                        }
                    }

                    ZrParser_InferredType_Free(cs->state, &currentType);
                    ZrParser_InferredType_Init(cs->state, &currentType, ZR_VALUE_TYPE_OBJECT);
                    ZrParser_InferredType_Copy(cs->state, &currentType, &nextType);
                    ZrParser_InferredType_Free(cs->state, &nextType);
                    currentIsPrototypeReference = ZR_FALSE;
                    continue;
                }

                if (memberExpr->computed) {
                    SZrTypeMemberInfo *metaMember;
                    SZrTypePrototypeInfo *knownPrototype;
                    TZrChar errorMessage[ZR_PARSER_ERROR_BUFFER_LENGTH];

                    if (currentType.typeName == ZR_NULL) {
                        ZrParser_InferredType_Free(cs->state, &currentType);
                        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                        return ZR_TRUE;
                    }

                    metaMember = find_compiler_type_meta_member_inference(cs, currentType.typeName, ZR_META_GET_ITEM);
                    if (metaMember == ZR_NULL) {
                        knownPrototype = find_compiler_type_prototype_inference(cs, currentType.typeName);
                        if (knownPrototype != ZR_NULL) {
                            snprintf(errorMessage,
                                     sizeof(errorMessage),
                                     "Type '%s' does not support computed member access",
                                     ZrCore_String_GetNativeString(currentType.typeName));
                            ZrParser_Compiler_Error(cs, errorMessage, memberNode->location);
                            ZrParser_InferredType_Free(cs->state, &currentType);
                            return ZR_FALSE;
                        }

                        ZrParser_InferredType_Free(cs->state, &currentType);
                        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                        return ZR_TRUE;
                    }

                    ZrParser_InferredType_Init(cs->state, &nextType, ZR_VALUE_TYPE_OBJECT);
                    if (!inferred_type_from_type_name(cs, metaMember->returnTypeName, &nextType)) {
                        ZrParser_InferredType_Free(cs->state, &currentType);
                        ZrParser_InferredType_Free(cs->state, &nextType);
                        return ZR_FALSE;
                    }

                    ZrParser_InferredType_Free(cs->state, &currentType);
                    ZrParser_InferredType_Init(cs->state, &currentType, ZR_VALUE_TYPE_OBJECT);
                    ZrParser_InferredType_Copy(cs->state, &currentType, &nextType);
                    ZrParser_InferredType_Free(cs->state, &nextType);
                    currentIsPrototypeReference = ZR_FALSE;
                    continue;
                }

                if (memberExpr->property == ZR_NULL || currentType.typeName == ZR_NULL ||
                    !type_inference_resolve_member_segment_names(cs,
                                                                 memberExpr->property,
                                                                 &memberLookupName,
                                                                 &memberResolvedTypeName)) {
                    ZrParser_InferredType_Free(cs->state, &currentType);
                    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                    return ZR_TRUE;
                }

                memberInfo = find_compiler_type_member_inference(cs, currentType.typeName, memberLookupName);
                if (memberInfo == ZR_NULL) {
                    ZrParser_InferredType_Free(cs->state, &currentType);
                    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                    return ZR_TRUE;
                }

                if ((memberInfo->memberType == ZR_AST_STRUCT_FIELD || memberInfo->memberType == ZR_AST_CLASS_FIELD) &&
                    memberInfo->fieldTypeName != ZR_NULL &&
                    find_compiler_type_prototype_inference(cs, memberInfo->fieldTypeName) != ZR_NULL &&
                    !type_name_is_module_prototype_inference(cs, memberInfo->fieldTypeName)) {
                    nextIsPrototypeReference = ZR_TRUE;
                }

                if (nextIsFunctionCall && nextIsPrototypeReference) {
                    ZrParser_Compiler_Error(cs,
                                            "Prototype references are not callable; use $target(...) or new target(...)",
                                            members->nodes[i + 1]->location);
                    ZrParser_InferredType_Free(cs->state, &currentType);
                    return ZR_FALSE;
                }

                ZrParser_InferredType_Init(cs->state, &nextType, ZR_VALUE_TYPE_OBJECT);
                if (nextIsFunctionCall) {
                    SZrResolvedCallSignature resolvedMemberSignature;
                    TZrChar genericDiagnostic[ZR_PARSER_ERROR_BUFFER_LENGTH];

                    memset(&resolvedMemberSignature, 0, sizeof(resolvedMemberSignature));
                    ZrParser_InferredType_Init(cs->state, &resolvedMemberSignature.returnType, ZR_VALUE_TYPE_OBJECT);
                    ZrCore_Array_Construct(&resolvedMemberSignature.parameterTypes);
                    ZrCore_Array_Construct(&resolvedMemberSignature.parameterPassingModes);
                    genericDiagnostic[0] = '\0';

                    if (!memberInfo->isStatic &&
                        !receiver_ownership_can_call_member(currentType.ownershipQualifier,
                                                            memberInfo->receiverQualifier)) {
                        ZrParser_Compiler_Error(cs,
                                                receiver_ownership_call_error(currentType.ownershipQualifier),
                                                members->nodes[i + 1]->location);
                        ZrParser_InferredType_Free(cs->state, &currentType);
                        ZrParser_InferredType_Free(cs->state, &nextType);
                        free_resolved_call_signature(cs->state, &resolvedMemberSignature);
                        return ZR_FALSE;
                    }

                    if (resolve_generic_member_call_signature_detailed(cs,
                                                                       memberInfo,
                                                                       &members->nodes[i + 1]->data.functionCall,
                                                                       &resolvedMemberSignature,
                                                                       genericDiagnostic,
                                                                       sizeof(genericDiagnostic)) !=
                        ZR_GENERIC_CALL_RESOLVE_OK) {
                        ZrParser_Compiler_Error(cs,
                                                genericDiagnostic[0] != '\0' ? genericDiagnostic
                                                                             : "Unable to resolve generic method call",
                                                members->nodes[i + 1]->location);
                        ZrParser_InferredType_Free(cs->state, &currentType);
                        ZrParser_InferredType_Free(cs->state, &nextType);
                        free_resolved_call_signature(cs->state, &resolvedMemberSignature);
                        return ZR_FALSE;
                    }

                    if (!validate_member_call_arguments(cs,
                                                        memberInfo,
                                                        &resolvedMemberSignature,
                                                        &members->nodes[i + 1]->data.functionCall,
                                                        members->nodes[i + 1]->location)) {
                        ZrParser_InferredType_Free(cs->state, &currentType);
                        ZrParser_InferredType_Free(cs->state, &nextType);
                        free_resolved_call_signature(cs->state, &resolvedMemberSignature);
                        return ZR_FALSE;
                    }
                    inferred_type_from_member_call(cs, memberInfo, &resolvedMemberSignature, &nextType);
                    free_resolved_call_signature(cs->state, &resolvedMemberSignature);
                    i++;
                    nextIsPrototypeReference = ZR_FALSE;
                } else {
                    if (memberExpr->property->type == ZR_AST_TYPE &&
                        find_compiler_type_prototype_inference(cs, currentType.typeName) != ZR_NULL &&
                        type_name_is_module_prototype_inference(cs, currentType.typeName) &&
                        memberResolvedTypeName != ZR_NULL) {
                        inferred_type_from_type_name(cs, memberResolvedTypeName, &nextType);
                    } else {
                        inferred_type_from_member_access(cs, memberInfo, &nextType);
                    }
                }

                ZrParser_InferredType_Free(cs->state, &currentType);
                ZrParser_InferredType_Init(cs->state, &currentType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Copy(cs->state, &currentType, &nextType);
                ZrParser_InferredType_Free(cs->state, &nextType);
                currentIsPrototypeReference = nextIsPrototypeReference;
                continue;
            }

            if (memberNode->type == ZR_AST_FUNCTION_CALL) {
                if (currentIsPrototypeReference) {
                    ZrParser_Compiler_Error(cs,
                                            "Prototype references are not callable; use $target(...) or new target(...)",
                                            memberNode->location);
                    ZrParser_InferredType_Free(cs->state, &currentType);
                    return ZR_FALSE;
                }
                ZrParser_InferredType_Free(cs->state, &currentType);
                ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                return ZR_TRUE;
            }
        }
    }
    ZrParser_InferredType_Copy(cs->state, result, &currentType);
    ZrParser_InferredType_Free(cs->state, &currentType);
    return ZR_TRUE;
}

TZrBool resolve_compile_time_array_size(SZrCompilerState *cs,
                                        const SZrType *astType,
                                        TZrSize *resolvedSize) {
    SZrTypeValue evaluatedValue;
    TZrInt64 signedSize;

    if (cs == ZR_NULL || astType == ZR_NULL || resolvedSize == ZR_NULL ||
        astType->arraySizeExpression == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrParser_Compiler_EvaluateCompileTimeExpression(cs, astType->arraySizeExpression, &evaluatedValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(evaluatedValue.type)) {
        signedSize = evaluatedValue.value.nativeObject.nativeInt64;
        if (signedSize < 0) {
            ZrParser_Compiler_Error(cs,
                            "Array size expression must evaluate to a non-negative integer",
                            astType->arraySizeExpression->location);
            return ZR_FALSE;
        }
        *resolvedSize = (TZrSize)signedSize;
        return ZR_TRUE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(evaluatedValue.type)) {
        *resolvedSize = (TZrSize)evaluatedValue.value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }

    ZrParser_Compiler_Error(cs,
                    "Array size expression must evaluate to an integer",
                    astType->arraySizeExpression->location);
    return ZR_FALSE;
}

static const TZrChar *type_name_string_get_ownership_prefix(EZrOwnershipQualifier ownershipQualifier) {
    switch (ownershipQualifier) {
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE: return "%unique ";
        case ZR_OWNERSHIP_QUALIFIER_SHARED: return "%shared ";
        case ZR_OWNERSHIP_QUALIFIER_WEAK: return "%weak ";
        case ZR_OWNERSHIP_QUALIFIER_BORROWED: return "%borrowed ";
        default: return "";
    }
}

static const TZrChar *type_name_string_get_base_or_named_type(const SZrInferredType *type) {
    if (type == ZR_NULL) {
        return "unknown";
    }

    if (type->typeName != ZR_NULL) {
        if (type->typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            return ZrCore_String_GetNativeStringShort(type->typeName);
        }
        return ZrCore_String_GetNativeString(type->typeName);
    }

    return get_base_type_name(type->baseType);
}

static TZrBool type_name_string_append(TZrChar *buffer,
                                       TZrSize bufferSize,
                                       TZrSize *writeIndex,
                                       const TZrChar *text) {
    TZrSize length;

    if (buffer == ZR_NULL || writeIndex == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    length = strlen(text);
    if (*writeIndex + length >= bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer + *writeIndex, text, length);
    *writeIndex += length;
    buffer[*writeIndex] = '\0';
    return ZR_TRUE;
}

static TZrBool type_name_string_append_type(SZrState *state,
                                            const SZrInferredType *type,
                                            TZrChar *buffer,
                                            TZrSize bufferSize,
                                            TZrSize *writeIndex) {
    TZrChar nestedBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    const TZrChar *ownershipPrefix;
    const TZrChar *baseName;

    if (state == ZR_NULL || type == ZR_NULL || buffer == ZR_NULL || writeIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    ownershipPrefix = type_name_string_get_ownership_prefix(type->ownershipQualifier);
    baseName = type_name_string_get_base_or_named_type(type);
    if (!type_name_string_append(buffer, bufferSize, writeIndex, ownershipPrefix) ||
        !type_name_string_append(buffer, bufferSize, writeIndex, baseName != ZR_NULL ? baseName : "unknown")) {
        return ZR_FALSE;
    }

    if (type->elementTypes.length > 0 && type->baseType == ZR_VALUE_TYPE_ARRAY) {
        SZrInferredType *elementType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&type->elementTypes, 0);
        const TZrChar *elementText = ZrParser_TypeNameString_Get(state, elementType, nestedBuffer, sizeof(nestedBuffer));
        if (!type_name_string_append(buffer, bufferSize, writeIndex, "<") ||
            !type_name_string_append(buffer, bufferSize, writeIndex, elementText != ZR_NULL ? elementText : "unknown") ||
            !type_name_string_append(buffer, bufferSize, writeIndex, ">")) {
            return ZR_FALSE;
        }
    }

    if (type->isNullable) {
        if (!type_name_string_append(buffer, bufferSize, writeIndex, "?")) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

// 获取类型名称字符串（用于错误报告）
const TZrChar *ZrParser_TypeNameString_Get(SZrState *state,
                                           const SZrInferredType *type,
                                           TZrChar *buffer,
                                           TZrSize bufferSize) {
    TZrSize writeIndex = 0;

    if (state == ZR_NULL || type == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return "unknown";
    }

    buffer[0] = '\0';
    if (type_name_string_append_type(state, type, buffer, bufferSize, &writeIndex)) {
        return buffer;
    }

    return type_name_string_get_base_or_named_type(type);
}

// 报告类型错误
