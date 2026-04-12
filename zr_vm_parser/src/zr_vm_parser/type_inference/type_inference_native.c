//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"
#include "compiler_internal.h"
#include "type_inference_internal.h"
#include "zr_vm_parser/ast.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_common/zr_string_conf.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define ZR_MEMBER_PARAMETER_COUNT_UNKNOWN ((TZrUInt32)-1)

typedef struct ZrLibRegisteredModuleRecord ZrLibRegisteredModuleRecord;

extern SZrObject *native_metadata_make_module_info(SZrState *state,
                                                   const ZrLibModuleDescriptor *descriptor,
                                                   const ZrLibRegisteredModuleRecord *record);

static TZrBool inferred_type_is_task_handle(SZrCompilerState *cs, const SZrInferredType *type) {
    if (cs == ZR_NULL || type == ZR_NULL) {
        return ZR_FALSE;
    }

    return inferred_type_implements_protocol_mask(cs,
                                                  type,
                                                  ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_HANDLE));
}

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

static void native_module_info_add_field_member(SZrState *state,
                                                SZrTypePrototypeInfo *info,
                                                EZrAstNodeType memberType,
                                                SZrString *memberName,
                                                SZrString *fieldTypeName,
                                                TZrBool isStatic,
                                                TZrUInt32 contractRole);
static void native_module_info_add_inherit(SZrState *state,
                                           SZrTypePrototypeInfo *info,
                                           SZrString *typeName);

static SZrTypePrototypeInfo *find_registered_type_prototype_inference_exact_only(SZrCompilerState *cs,
                                                                                  SZrString *typeName) {
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, index);
        if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, typeName)) {
            return info;
        }
    }

    if (cs->currentTypePrototypeInfo != ZR_NULL &&
        cs->currentTypePrototypeInfo->name != ZR_NULL &&
        ZrCore_String_Equal(cs->currentTypePrototypeInfo->name, typeName)) {
        return cs->currentTypePrototypeInfo;
    }

    return ZR_NULL;
}

static TZrBool native_module_compile_info_stack_contains(SZrGlobalState *global, SZrString *moduleName) {
    if (global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < global->importCompileInfoStack.length; index++) {
        SZrString **entryPtr = (SZrString **)ZrCore_Array_Get(&global->importCompileInfoStack, index);
        if (entryPtr != ZR_NULL && *entryPtr != ZR_NULL && ZrCore_String_Equal(*entryPtr, moduleName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool native_module_info_is_ascii_space(TZrChar ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static SZrString *native_module_info_parse_property_hint_type_name(SZrState *state, SZrString *signature) {
    const TZrChar *signatureText;
    const TZrChar *colon;
    const TZrChar *typeStart;
    const TZrChar *typeEnd;

    if (state == ZR_NULL || signature == ZR_NULL) {
        return ZR_NULL;
    }

    signatureText = ZrCore_String_GetNativeString(signature);
    if (signatureText == ZR_NULL) {
        return ZR_NULL;
    }

    colon = strchr(signatureText, ':');
    if (colon == ZR_NULL) {
        return ZR_NULL;
    }

    typeStart = colon + 1;
    while (*typeStart != '\0' && native_module_info_is_ascii_space(*typeStart)) {
        typeStart++;
    }

    typeEnd = signatureText + strlen(signatureText);
    while (typeEnd > typeStart && native_module_info_is_ascii_space(typeEnd[-1])) {
        typeEnd--;
    }

    if (typeEnd <= typeStart) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, (TZrNativeString)typeStart, (TZrSize)(typeEnd - typeStart));
}

static void native_module_info_add_property_hint_member(SZrCompilerState *cs,
                                                        SZrTypePrototypeInfo *modulePrototype,
                                                        SZrObject *hintEntry) {
    SZrString *symbolName;
    SZrString *symbolKind;
    SZrString *signature;
    TZrNativeString symbolKindText;
    SZrString *typeName;

    if (cs == ZR_NULL || modulePrototype == ZR_NULL || hintEntry == ZR_NULL) {
        return;
    }

    symbolName = native_module_info_get_string_field(cs->state, hintEntry, "symbolName");
    symbolKind = native_module_info_get_string_field(cs->state, hintEntry, "symbolKind");
    signature = native_module_info_get_string_field(cs->state, hintEntry, "signature");
    symbolKindText = symbolKind != ZR_NULL ? ZrCore_String_GetNativeString(symbolKind) : ZR_NULL;
    if (symbolName == ZR_NULL || symbolKindText == ZR_NULL || strcmp(symbolKindText, "property") != 0) {
        return;
    }

    typeName = native_module_info_parse_property_hint_type_name(cs->state, signature);
    if (typeName == ZR_NULL) {
        return;
    }

    native_module_info_add_field_member(cs->state,
                                        modulePrototype,
                                        ZR_AST_CLASS_FIELD,
                                        symbolName,
                                        typeName,
                                        ZR_TRUE,
                                        ZR_MEMBER_CONTRACT_ROLE_NONE);
}

static void native_module_info_init_prototype(SZrState *state,
                                              SZrTypePrototypeInfo *info,
                                              SZrString *name,
                                              EZrObjectPrototypeType type) {
    if (state == ZR_NULL || info == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(info, 0, sizeof(*info));
    info->name = name;
    info->type = type;
    info->accessModifier = ZR_ACCESS_PUBLIC;
    info->isImportedNative = ZR_TRUE;
    info->protocolMask = 0;
    ZrCore_Array_Init(state, &info->inherits, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state, &info->implements, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state,
                      &info->genericParameters,
                      sizeof(SZrTypeGenericParameterInfo),
                      ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state, &info->decorators, sizeof(SZrTypeDecoratorInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(state, &info->members, sizeof(SZrTypeMemberInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    info->extendsTypeName = ZR_NULL;
    info->enumValueTypeName = ZR_NULL;
    info->allowValueConstruction =
            type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE && type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
    info->allowBoxedConstruction =
            type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE && type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
    info->constructorSignature = ZR_NULL;
}

static SZrString *type_inference_builtin_reflection_string(SZrCompilerState *cs, const TZrChar *text);

static SZrString *type_inference_builtin_root_type_name(SZrCompilerState *cs, EZrObjectPrototypeType type) {
    if (cs == ZR_NULL) {
        return ZR_NULL;
    }

    if (type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
        return type_inference_builtin_reflection_string(cs, "zr.builtin.Module");
    }
    if (type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        return type_inference_builtin_reflection_string(cs, "zr.builtin.Object");
    }

    return ZR_NULL;
}

static void type_inference_apply_default_builtin_root(SZrCompilerState *cs,
                                                      SZrTypePrototypeInfo *info,
                                                      EZrObjectPrototypeType type,
                                                      const TZrChar *moduleNameText) {
    SZrString *defaultRootName;

    if (cs == ZR_NULL || info == ZR_NULL || info->extendsTypeName != ZR_NULL) {
        return;
    }

    defaultRootName = type_inference_builtin_root_type_name(cs, type);
    if (defaultRootName == ZR_NULL) {
        return;
    }

    if (moduleNameText != ZR_NULL &&
        strcmp(moduleNameText, "zr.builtin") == 0 &&
        info->name != ZR_NULL &&
        ((type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS && zr_string_equals_cstr(info->name, "Object")) ||
         (type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE && zr_string_equals_cstr(info->name, "Module")))) {
        return;
    }

    info->extendsTypeName = defaultRootName;
    native_module_info_add_inherit(cs->state, info, defaultRootName);
}

static TZrBool type_inference_is_builtin_reflection_compile_type_name(SZrString *typeName) {
    const TZrChar *typeNameText;

    if (typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    typeNameText = ZrCore_String_GetNativeString(typeName);
    if (typeNameText == ZR_NULL) {
        return ZR_FALSE;
    }

    return strcmp(typeNameText, "Class") == 0 ||
           strcmp(typeNameText, "Struct") == 0 ||
           strcmp(typeNameText, "Function") == 0 ||
           strcmp(typeNameText, "Field") == 0 ||
           strcmp(typeNameText, "Method") == 0 ||
           strcmp(typeNameText, "Property") == 0 ||
           strcmp(typeNameText, "Parameter") == 0 ||
           strcmp(typeNameText, "DecoratorPatch") == 0 ||
           strcmp(typeNameText, "Object") == 0 ||
           strcmp(typeNameText, "zr.builtin.TypeInfo") == 0;
}

static SZrString *type_inference_builtin_reflection_string(SZrCompilerState *cs, const TZrChar *text) {
    if (cs == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(cs->state, (TZrNativeString)text, strlen(text));
}

static void type_inference_builtin_reflection_add_inherit(SZrCompilerState *cs,
                                                          SZrTypePrototypeInfo *info,
                                                          const TZrChar *typeName) {
    SZrString *inheritName;

    if (cs == ZR_NULL || info == ZR_NULL || typeName == ZR_NULL) {
        return;
    }

    inheritName = type_inference_builtin_reflection_string(cs, typeName);
    if (inheritName == ZR_NULL) {
        return;
    }

    native_module_info_add_inherit(cs->state, info, inheritName);
}

static void type_inference_builtin_reflection_add_field(SZrCompilerState *cs,
                                                        SZrTypePrototypeInfo *info,
                                                        const TZrChar *memberName,
                                                        const TZrChar *fieldTypeName) {
    SZrString *memberNameString;
    SZrString *fieldTypeNameString;

    if (cs == ZR_NULL || info == ZR_NULL || memberName == ZR_NULL || fieldTypeName == ZR_NULL) {
        return;
    }

    memberNameString = type_inference_builtin_reflection_string(cs, memberName);
    fieldTypeNameString = type_inference_builtin_reflection_string(cs, fieldTypeName);
    if (memberNameString == ZR_NULL || fieldTypeNameString == ZR_NULL) {
        return;
    }

    native_module_info_add_field_member(cs->state,
                                        info,
                                        ZR_AST_CLASS_FIELD,
                                        memberNameString,
                                        fieldTypeNameString,
                                        ZR_FALSE,
                                        ZR_MEMBER_CONTRACT_ROLE_NONE);
}

static void type_inference_builtin_reflection_add_common_members(SZrCompilerState *cs, SZrTypePrototypeInfo *info) {
    static const struct {
        const TZrChar *memberName;
        const TZrChar *fieldTypeName;
    } kCommonFields[] = {
            { "name", "string" },
            { "qualifiedName", "string" },
            { "hash", "uint" },
            { "kind", "string" },
            { "owner", "object" },
            { "module", "object" },
            { "members", "object" },
            { "metadata", "object" },
            { "decorators", "array" },
            { "genericParameters", "array" },
            { "source", "object" },
            { "tests", "array" },
            { "compileTime", "object" },
            { "layout", "object" },
            { "ownership", "object" },
            { "ir", "object" },
            { "codeBlocks", "array" },
            { "nativeOrigin", "object" },
            { "mutable", "bool" },
            { "phase", "string" },
            { "modifierFlags", "int" },
            { "isAbstract", "bool" },
            { "isVirtual", "bool" },
            { "isOverride", "bool" },
            { "isFinal", "bool" },
            { "isShadow", "bool" },
            { "ownerTypeName", "string" },
            { "baseDefinitionOwnerTypeName", "string" },
            { "baseDefinitionName", "string" },
            { "virtualSlotIndex", "int" },
            { "interfaceContractSlot", "int" },
            { "propertyIdentity", "int" },
            { "accessorRole", "int" },
            { "nextVirtualSlotIndex", "int" },
            { "nextPropertyIdentity", "int" },
    };

    if (cs == ZR_NULL || info == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(kCommonFields); index++) {
        type_inference_builtin_reflection_add_field(cs,
                                                    info,
                                                    kCommonFields[index].memberName,
                                                    kCommonFields[index].fieldTypeName);
    }
}

static void type_inference_builtin_reflection_add_callable_members(SZrCompilerState *cs,
                                                                   SZrTypePrototypeInfo *info) {
    static const struct {
        const TZrChar *memberName;
        const TZrChar *fieldTypeName;
    } kCallableFields[] = {
            { "returnTypeName", "string" },
            { "parameterCount", "int" },
            { "isStatic", "bool" },
            { "isVariadic", "bool" },
            { "returnType", "object" },
            { "parameters", "array" },
            { "parameterModes", "array" },
    };

    if (cs == ZR_NULL || info == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(kCallableFields); index++) {
        type_inference_builtin_reflection_add_field(cs,
                                                    info,
                                                    kCallableFields[index].memberName,
                                                    kCallableFields[index].fieldTypeName);
    }
}

ZR_PARSER_API void ensure_builtin_reflection_compile_type(SZrCompilerState *cs, SZrString *typeName) {
    SZrTypePrototypeInfo info;
    SZrTypePrototypeInfo *targetInfo;
    const TZrChar *typeNameText;
    static const TZrChar *kBuiltinTypeInfoName = "zr.builtin.TypeInfo";

    if (cs == ZR_NULL || typeName == ZR_NULL || !type_inference_is_builtin_reflection_compile_type_name(typeName)) {
        return;
    }

    typeNameText = ZrCore_String_GetNativeString(typeName);
    if (typeNameText == ZR_NULL) {
        return;
    }

    if (strcmp(typeNameText, kBuiltinTypeInfoName) != 0) {
        ensure_builtin_reflection_compile_type(cs,
                                               type_inference_builtin_reflection_string(cs, kBuiltinTypeInfoName));
    }

    targetInfo = find_registered_type_prototype_inference_exact_only(cs, typeName);
    if (targetInfo == ZR_NULL) {
        native_module_info_init_prototype(cs->state, &info, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        info.allowValueConstruction = ZR_FALSE;
        info.allowBoxedConstruction = ZR_FALSE;
        ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);
        targetInfo = find_registered_type_prototype_inference_exact_only(cs, typeName);
    }
    if (targetInfo == ZR_NULL) {
        return;
    }

    targetInfo->type = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    targetInfo->allowValueConstruction = ZR_FALSE;
    targetInfo->allowBoxedConstruction = ZR_FALSE;

    if (strcmp(typeNameText, "Function") == 0 || strcmp(typeNameText, "Method") == 0) {
        type_inference_builtin_reflection_add_inherit(cs, targetInfo, kBuiltinTypeInfoName);
    } else if (strcmp(typeNameText, "Class") == 0 ||
               strcmp(typeNameText, "Struct") == 0 ||
               strcmp(typeNameText, "Object") == 0 ||
               strcmp(typeNameText, "Field") == 0 ||
               strcmp(typeNameText, "Property") == 0 ||
               strcmp(typeNameText, "Parameter") == 0) {
        type_inference_builtin_reflection_add_inherit(cs, targetInfo, kBuiltinTypeInfoName);
    }

    if (strcmp(typeNameText, kBuiltinTypeInfoName) == 0) {
        type_inference_builtin_reflection_add_common_members(cs, targetInfo);
    }
    if (strcmp(typeNameText, "Function") == 0 || strcmp(typeNameText, "Method") == 0) {
        type_inference_builtin_reflection_add_callable_members(cs, targetInfo);
    } else if (strcmp(typeNameText, "DecoratorPatch") == 0) {
        type_inference_builtin_reflection_add_field(cs, targetInfo, "metadata", "object");
    }
}

static SZrObject *native_module_info_ensure_type_metadata_object(SZrCompilerState *cs,
                                                                 SZrTypePrototypeInfo *info) {
    SZrObject *metadataObject;

    if (cs == ZR_NULL || info == ZR_NULL) {
        return ZR_NULL;
    }

    if (info->hasDecoratorMetadata && info->decoratorMetadataValue.type == ZR_VALUE_TYPE_OBJECT &&
        info->decoratorMetadataValue.value.object != ZR_NULL) {
        return ZR_CAST_OBJECT(cs->state, info->decoratorMetadataValue.value.object);
    }

    metadataObject = extern_compiler_new_object_constant(cs);
    if (metadataObject == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(cs->state, &info->decoratorMetadataValue, ZR_CAST_RAW_OBJECT_AS_SUPER(metadataObject));
    info->decoratorMetadataValue.type = ZR_VALUE_TYPE_OBJECT;
    info->hasDecoratorMetadata = ZR_TRUE;
    return metadataObject;
}

static TZrBool native_module_info_set_type_metadata_string_field(SZrCompilerState *cs,
                                                                 SZrTypePrototypeInfo *info,
                                                                 const TZrChar *fieldName,
                                                                 SZrString *fieldValue) {
    SZrTypeValue value;
    SZrObject *metadataObject;

    if (cs == ZR_NULL || info == ZR_NULL || fieldName == ZR_NULL || fieldValue == ZR_NULL) {
        return ZR_FALSE;
    }

    metadataObject = native_module_info_ensure_type_metadata_object(cs, info);
    if (metadataObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldValue));
    value.type = ZR_VALUE_TYPE_STRING;
    return extern_compiler_set_object_field(cs, metadataObject, fieldName, &value);
}

static void native_module_info_copy_type_metadata(SZrCompilerState *cs,
                                                  SZrTypePrototypeInfo *info,
                                                  SZrObject *entry) {
    static const TZrChar *const kMetadataFields[] = {
            "ffiLoweringKind",
            "ffiViewTypeName",
            "ffiUnderlyingTypeName",
            "ffiOwnerMode",
            "ffiReleaseHook",
    };

    if (cs == ZR_NULL || info == ZR_NULL || entry == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(kMetadataFields); index++) {
        SZrString *fieldValue = native_module_info_get_string_field(cs->state, entry, kMetadataFields[index]);
        if (fieldValue != ZR_NULL) {
            native_module_info_set_type_metadata_string_field(cs, info, kMetadataFields[index], fieldValue);
        }
    }
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
                                                TZrBool isStatic,
                                                TZrUInt32 contractRole) {
    SZrTypeMemberInfo memberInfo;

    if (state == ZR_NULL || info == ZR_NULL || memberName == ZR_NULL || native_module_info_has_member(info, memberName)) {
        return;
    }

    memset(&memberInfo, 0, sizeof(memberInfo));
    memberInfo.minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    memberInfo.memberType = memberType;
    memberInfo.name = memberName;
    memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo.isStatic = isStatic;
    memberInfo.fieldTypeName = fieldTypeName;
    memberInfo.ownerTypeName = info->name;
    memberInfo.baseDefinitionOwnerTypeName = info->name;
    memberInfo.baseDefinitionName = memberName;
    memberInfo.contractRole = contractRole;
    memberInfo.virtualSlotIndex = (TZrUInt32)-1;
    memberInfo.interfaceContractSlot = (TZrUInt32)-1;
    memberInfo.propertyIdentity = (TZrUInt32)-1;
    if (info->type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE && !isStatic) {
        memberInfo.modifierFlags = ZR_DECLARATION_MODIFIER_ABSTRACT | ZR_DECLARATION_MODIFIER_VIRTUAL;
    }
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

static TZrUInt32 native_module_info_min_argument_count(SZrState *state,
                                                       SZrObject *entry,
                                                       TZrUInt32 parameterCount) {
    TZrInt64 minArgumentCount;

    if (state == ZR_NULL || entry == ZR_NULL) {
        return parameterCount;
    }

    minArgumentCount = native_module_info_get_int_field(state, entry, "minArgumentCount", -1);
    if (minArgumentCount >= 0) {
        return (TZrUInt32)minArgumentCount;
    }

    return parameterCount;
}

static TZrBool native_module_info_member_has_generic_parameter(const SZrTypeMemberInfo *memberInfo, SZrString *name) {
    if (memberInfo == ZR_NULL || name == ZR_NULL || !memberInfo->genericParameters.isValid) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < memberInfo->genericParameters.length; index++) {
        SZrTypeGenericParameterInfo *existing =
                (SZrTypeGenericParameterInfo *)ZrCore_Array_Get((SZrArray *)&memberInfo->genericParameters, index);
        if (existing != ZR_NULL && existing->name != ZR_NULL && ZrCore_String_Equal(existing->name, name)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void native_module_info_copy_method_generic_parameters(SZrState *state,
                                                              SZrTypeMemberInfo *memberInfo,
                                                              SZrObject *genericParametersArray) {
    if (state == ZR_NULL || memberInfo == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < native_module_info_array_length(genericParametersArray); index++) {
        SZrObject *genericParameterEntry =
                native_module_info_array_get_object(state, genericParametersArray, index);
        SZrString *name;
        SZrObject *constraintsArray;
        SZrTypeGenericParameterInfo copied;

        if (genericParameterEntry == ZR_NULL) {
            continue;
        }

        name = native_module_info_get_string_field(state, genericParameterEntry, "name");
        if (name == ZR_NULL || native_module_info_member_has_generic_parameter(memberInfo, name)) {
            continue;
        }

        if (!memberInfo->genericParameters.isValid) {
            ZrCore_Array_Init(state,
                              &memberInfo->genericParameters,
                              sizeof(SZrTypeGenericParameterInfo),
                              ZR_PARSER_INITIAL_CAPACITY_PAIR);
        }

        memset(&copied, 0, sizeof(copied));
        copied.name = name;
        ZrCore_Array_Init(state,
                          &copied.constraintTypeNames,
                          sizeof(SZrString *),
                          ZR_PARSER_INITIAL_CAPACITY_PAIR);
        constraintsArray = native_module_info_get_array_field(state, genericParameterEntry, "constraints");
        for (TZrSize constraintIndex = 0; constraintIndex < native_module_info_array_length(constraintsArray);
             constraintIndex++) {
            SZrString *constraintTypeName =
                    native_module_info_array_get_string(state, constraintsArray, constraintIndex);
            if (constraintTypeName != ZR_NULL) {
                ZrCore_Array_Push(state, &copied.constraintTypeNames, &constraintTypeName);
            }
        }

        ZrCore_Array_Push(state, &memberInfo->genericParameters, &copied);
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
                                                 TZrUInt32 minArgumentCount,
                                                 SZrObject *parametersArray,
                                                 SZrObject *genericParametersArray,
                                                 TZrUInt32 contractRole) {
    SZrTypeMemberInfo memberInfo;

    if (cs == ZR_NULL || info == ZR_NULL || memberName == ZR_NULL || native_module_info_has_member(info, memberName)) {
        return;
    }

    memset(&memberInfo, 0, sizeof(memberInfo));
    memberInfo.minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    memberInfo.memberType = memberType;
    memberInfo.name = memberName;
    memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo.isStatic = isStatic;
    memberInfo.parameterCount = parameterCount;
    memberInfo.minArgumentCount = minArgumentCount;
    memberInfo.returnTypeName = returnTypeName;
    memberInfo.ownerTypeName = info->name;
    memberInfo.baseDefinitionOwnerTypeName = info->name;
    memberInfo.baseDefinitionName = memberName;
    memberInfo.contractRole = contractRole;
    memberInfo.virtualSlotIndex = (TZrUInt32)-1;
    memberInfo.interfaceContractSlot = (TZrUInt32)-1;
    memberInfo.propertyIdentity = (TZrUInt32)-1;
    if (info->type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE && !isStatic) {
        memberInfo.modifierFlags = ZR_DECLARATION_MODIFIER_ABSTRACT | ZR_DECLARATION_MODIFIER_VIRTUAL;
        memberInfo.virtualSlotIndex = info->nextVirtualSlotIndex++;
        memberInfo.interfaceContractSlot = memberInfo.virtualSlotIndex;
    }
    native_module_info_copy_method_generic_parameters(cs->state, &memberInfo, genericParametersArray);
    native_module_info_copy_parameter_types(cs, &memberInfo, parametersArray);
    ZrCore_Array_Push(cs->state, &info->members, &memberInfo);
}

static void native_module_info_add_meta_method_member(SZrCompilerState *cs,
                                                      SZrTypePrototypeInfo *info,
                                                      EZrMetaType metaType,
                                                      SZrString *returnTypeName,
                                                      TZrUInt32 parameterCount,
                                                      TZrUInt32 minArgumentCount,
                                                      SZrObject *parametersArray,
                                                      SZrObject *genericParametersArray) {
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
    memberInfo.minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    memberInfo.memberType = info->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT
                                    ? ZR_AST_STRUCT_META_FUNCTION
                                    : ZR_AST_CLASS_META_FUNCTION;
    memberInfo.name = memberName;
    memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo.metaType = metaType;
    memberInfo.isMetaMethod = ZR_TRUE;
    memberInfo.parameterCount = parameterCount;
    memberInfo.minArgumentCount = minArgumentCount;
    memberInfo.returnTypeName = returnTypeName;
    native_module_info_copy_method_generic_parameters(cs->state, &memberInfo, genericParametersArray);
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

    if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        nativeTypeName = ZrCore_String_GetNativeStringShort(typeName);
        nativeTypeNameLength = typeName->shortStringLength;
    } else {
        nativeTypeName = ZrCore_String_GetNativeString(typeName);
        nativeTypeNameLength = nativeTypeName != ZR_NULL ? typeName->longStringLength : 0;
    }
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
        return ZR_TRUE;
    }

    {
        EZrValueType primitiveBaseType = ZR_VALUE_TYPE_UNKNOWN;
        if (nativeTypeName != ZR_NULL &&
            inferred_type_try_map_primitive_name(nativeTypeName, nativeTypeNameLength, &primitiveBaseType)) {
            /* Preserve the imported primitive spelling so closed generic names
             * like Ptr<u8> stay stable; compatibility is normalized later in
             * the shared type-system compare helpers. */
            ZrParser_InferredType_InitFull(cs->state, result, primitiveBaseType, ZR_FALSE, typeName);
            return ZR_TRUE;
        }
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
    SZrArray membersSnapshot;
    SZrArray implementsSnapshot;
    SZrArray inheritsSnapshot;

    if (cs == ZR_NULL || typeName == ZR_NULL || metaType >= ZR_META_ENUM_MAX) {
        return ZR_NULL;
    }

    info = find_compiler_type_prototype_inference(cs, typeName);
    if (info == ZR_NULL) {
        return ZR_NULL;
    }

    membersSnapshot = info->members;
    implementsSnapshot = info->implements;
    inheritsSnapshot = info->inherits;

    for (TZrSize index = 0; index < membersSnapshot.length; index++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&membersSnapshot, index);
        if (memberInfo != ZR_NULL && memberInfo->isMetaMethod && memberInfo->metaType == metaType) {
            return memberInfo;
        }
    }

    for (TZrSize index = 0; index < implementsSnapshot.length; index++) {
        SZrString **implementedNamePtr = (SZrString **)ZrCore_Array_Get(&implementsSnapshot, index);
        SZrTypeMemberInfo *implementedMember;

        if (implementedNamePtr == ZR_NULL || *implementedNamePtr == ZR_NULL) {
            continue;
        }

        implementedMember = find_compiler_type_meta_member_inference(cs, *implementedNamePtr, metaType);
        if (implementedMember != ZR_NULL) {
            return implementedMember;
        }
    }

    for (TZrSize index = 0; index < inheritsSnapshot.length; index++) {
        SZrString **inheritNamePtr = (SZrString **)ZrCore_Array_Get(&inheritsSnapshot, index);
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
                                              SZrFunctionCall *call,
                                              const SZrResolvedCallSignature *resolvedSignature,
                                              SZrInferredType *result) {
    if (cs == ZR_NULL || memberInfo == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (infer_member_call_contract_return_type(cs, memberInfo, call, result)) {
        return ZR_TRUE;
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

static SZrAstNodeArray *member_info_parameter_list(const SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL || memberInfo->declarationNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (memberInfo->declarationNode->type) {
        case ZR_AST_STRUCT_METHOD:
            return memberInfo->declarationNode->data.structMethod.params;
        case ZR_AST_STRUCT_META_FUNCTION:
            return memberInfo->declarationNode->data.structMetaFunction.params;
        case ZR_AST_CLASS_METHOD:
            return memberInfo->declarationNode->data.classMethod.params;
        case ZR_AST_CLASS_META_FUNCTION:
            return memberInfo->declarationNode->data.classMetaFunction.params;
        default:
            return ZR_NULL;
    }
}

static SZrString *member_info_parameter_name_at(const SZrTypeMemberInfo *memberInfo, TZrSize index) {
    SZrAstNodeArray *paramList = member_info_parameter_list(memberInfo);

    if (paramList != ZR_NULL && index < paramList->count) {
        SZrAstNode *paramNode = paramList->nodes[index];
        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER &&
            paramNode->data.parameter.name != ZR_NULL) {
            return paramNode->data.parameter.name->name;
        }
    }

    if (memberInfo != ZR_NULL && index < memberInfo->parameterNames.length) {
        SZrString **namePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterNames, index);
        return namePtr != ZR_NULL ? *namePtr : ZR_NULL;
    }

    return ZR_NULL;
}

static TZrBool member_info_parameter_has_default_at(const SZrTypeMemberInfo *memberInfo, TZrSize index) {
    SZrAstNodeArray *paramList = member_info_parameter_list(memberInfo);

    if (paramList != ZR_NULL && index < paramList->count) {
        SZrAstNode *paramNode = paramList->nodes[index];
        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
            return paramNode->data.parameter.defaultValue != ZR_NULL;
        }
    }

    if (memberInfo != ZR_NULL && index < memberInfo->parameterHasDefaultValues.length) {
        TZrBool *hasDefaultPtr =
                (TZrBool *)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterHasDefaultValues, index);
        return hasDefaultPtr != ZR_NULL ? *hasDefaultPtr : ZR_FALSE;
    }

    return ZR_FALSE;
}

static SZrAstNode *member_info_parameter_default_node_at(const SZrTypeMemberInfo *memberInfo, TZrSize index) {
    SZrAstNodeArray *paramList = member_info_parameter_list(memberInfo);

    if (paramList == ZR_NULL || index >= paramList->count) {
        return ZR_NULL;
    }

    if (paramList->nodes[index] == ZR_NULL || paramList->nodes[index]->type != ZR_AST_PARAMETER) {
        return ZR_NULL;
    }

    return paramList->nodes[index]->data.parameter.defaultValue;
}

static TZrUInt32 member_info_min_argument_count(const SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL) {
        return 0;
    }

    if (memberInfo->minArgumentCount != ZR_MEMBER_PARAMETER_COUNT_UNKNOWN) {
        return memberInfo->minArgumentCount;
    }

    if (memberInfo->parameterCount == ZR_MEMBER_PARAMETER_COUNT_UNKNOWN) {
        return 0;
    }

    if (memberInfo->parameterHasDefaultValues.length > 0) {
        TZrUInt32 minArgumentCount = memberInfo->parameterCount;
        while (minArgumentCount > 0 &&
               member_info_parameter_has_default_at(memberInfo, (TZrSize)minArgumentCount - 1U)) {
            minArgumentCount--;
        }
        return minArgumentCount;
    }

    return memberInfo->parameterCount;
}

static EZrParameterPassingMode member_call_parameter_passing_mode_at(const SZrArray *parameterPassingModes,
                                                                    TZrSize index) {
    EZrParameterPassingMode *modePtr;

    if (parameterPassingModes == ZR_NULL || index >= parameterPassingModes->length) {
        return ZR_PARAMETER_PASSING_MODE_VALUE;
    }

    modePtr = (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)parameterPassingModes, index);
    return modePtr != ZR_NULL ? *modePtr : ZR_PARAMETER_PASSING_MODE_VALUE;
}

static const TZrChar *member_call_parameter_passing_mode_label(EZrParameterPassingMode passingMode) {
    switch (passingMode) {
        case ZR_PARAMETER_PASSING_MODE_IN:
            return "%in";
        case ZR_PARAMETER_PASSING_MODE_OUT:
            return "%out";
        case ZR_PARAMETER_PASSING_MODE_REF:
            return "%ref";
        case ZR_PARAMETER_PASSING_MODE_VALUE:
        default:
            return "value";
    }
}

static TZrBool member_call_expression_is_assignable_storage(SZrAstNode *node) {
    SZrPrimaryExpression *primaryExpr;

    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_TRUE;
    }

    if (node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primaryExpr = &node->data.primaryExpression;
    if (primaryExpr->property == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!member_call_expression_is_assignable_storage(primaryExpr->property) &&
        primaryExpr->property->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }

    if (primaryExpr->members == ZR_NULL || primaryExpr->members->count == 0) {
        return primaryExpr->property->type == ZR_AST_IDENTIFIER_LITERAL;
    }

    for (TZrSize index = 0; index < primaryExpr->members->count; index++) {
        SZrAstNode *memberNode = primaryExpr->members->nodes[index];
        if (memberNode == ZR_NULL || memberNode->type != ZR_AST_MEMBER_EXPRESSION) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool validate_member_call_arguments(SZrCompilerState *cs,
                                              const SZrTypeMemberInfo *memberInfo,
                                              const SZrResolvedCallSignature *resolvedSignature,
                                              SZrFunctionCall *call,
                                              SZrFileRange location) {
    TZrSize argCount;
    SZrArray argTypes;
    SZrAstNode **argumentNodes = ZR_NULL;
    const SZrArray *parameterTypes;
    const SZrArray *parameterPassingModes;
    TZrBool *provided = ZR_NULL;
    TZrSize parameterSlotCount;
    TZrUInt32 parameterCount;
    TZrUInt32 minArgumentCount;
    TZrBool requireTaskHandleArgument = ZR_FALSE;

    if (cs == ZR_NULL || memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    requireTaskHandleArgument = memberInfo->contractRole == ZR_MEMBER_CONTRACT_ROLE_TASK_AWAIT;
    minArgumentCount = member_info_min_argument_count(memberInfo);

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

    parameterSlotCount = parameterCount > parameterTypes->length ? parameterCount : parameterTypes->length;
    if (parameterSlotCount == 0) {
        if (argCount > 0) {
            ZrParser_Compiler_Error(cs, "Argument count mismatch", location);
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    provided = (TZrBool *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                          sizeof(TZrBool) * parameterSlotCount,
                                                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
    argumentNodes = (SZrAstNode **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                   sizeof(SZrAstNode *) * parameterSlotCount,
                                                                   ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (provided == ZR_NULL || argumentNodes == ZR_NULL) {
        if (provided != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          provided,
                                          sizeof(TZrBool) * parameterSlotCount,
                                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (argumentNodes != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          argumentNodes,
                                          sizeof(SZrAstNode *) * parameterSlotCount,
                                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < parameterSlotCount; index++) {
        provided[index] = ZR_FALSE;
        argumentNodes[index] = ZR_NULL;
    }

    if (call != ZR_NULL && call->hasNamedArgs && call->argNames != ZR_NULL) {
        TZrSize positionalCount = 0;

        for (TZrSize index = 0; index < argCount && index < call->argNames->length; index++) {
            SZrString **namePtr = (SZrString **)ZrCore_Array_Get(call->argNames, index);
            if (namePtr != ZR_NULL && *namePtr == ZR_NULL) {
                positionalCount++;
                continue;
            }
            break;
        }

        if (positionalCount > parameterSlotCount) {
            ZrParser_Compiler_Error(cs, "Argument count mismatch", location);
            goto cleanup_error;
        }

        for (TZrSize index = 0; index < positionalCount; index++) {
            provided[index] = ZR_TRUE;
            argumentNodes[index] = call->args->nodes[index];
        }

        for (TZrSize index = positionalCount; index < argCount && index < call->argNames->length; index++) {
            SZrString **namePtr = (SZrString **)ZrCore_Array_Get(call->argNames, index);
            TZrBool matched = ZR_FALSE;

            if (namePtr == ZR_NULL || *namePtr == ZR_NULL) {
                ZrParser_Compiler_Error(cs, "Argument count mismatch", location);
                goto cleanup_error;
            }

            for (TZrSize paramIndex = 0; paramIndex < parameterSlotCount; paramIndex++) {
                SZrString *paramName = member_info_parameter_name_at(memberInfo, paramIndex);
                if (paramName == ZR_NULL || !ZrCore_String_Equal(paramName, *namePtr)) {
                    continue;
                }

                if (provided[paramIndex]) {
                    ZrParser_Compiler_Error(cs, "Argument count mismatch", location);
                    goto cleanup_error;
                }

                provided[paramIndex] = ZR_TRUE;
                argumentNodes[paramIndex] = call->args->nodes[index];
                matched = ZR_TRUE;
                break;
            }

            if (!matched) {
                ZrParser_Compiler_Error(cs, "Argument count mismatch", location);
                goto cleanup_error;
            }
        }
    } else {
        if (argCount > parameterSlotCount) {
            ZrParser_Compiler_Error(cs, "Argument count mismatch", location);
            goto cleanup_error;
        }

        for (TZrSize index = 0; index < argCount; index++) {
            provided[index] = ZR_TRUE;
            argumentNodes[index] = call->args->nodes[index];
        }
    }

    while (parameterSlotCount > 0) {
        TZrSize trailingIndex = parameterSlotCount - 1U;
        EZrParameterPassingMode passingMode =
                member_call_parameter_passing_mode_at(parameterPassingModes, trailingIndex);
        if (argumentNodes[trailingIndex] != ZR_NULL ||
            member_info_parameter_default_node_at(memberInfo, trailingIndex) != ZR_NULL ||
            member_info_parameter_has_default_at(memberInfo, trailingIndex) ||
            passingMode == ZR_PARAMETER_PASSING_MODE_OUT ||
            passingMode == ZR_PARAMETER_PASSING_MODE_REF ||
            trailingIndex < (TZrSize)minArgumentCount) {
            break;
        }
        parameterSlotCount--;
    }

    ZrCore_Array_Init(cs->state, &argTypes, sizeof(SZrInferredType), parameterSlotCount);
    for (TZrSize index = 0; index < parameterSlotCount; index++) {
        SZrAstNode *argNode = argumentNodes[index];
        SZrAstNode *defaultNode = member_info_parameter_default_node_at(memberInfo, index);
        SZrInferredType *paramType =
                index < parameterTypes->length
                        ? (SZrInferredType *)ZrCore_Array_Get((SZrArray *)parameterTypes, index)
                        : ZR_NULL;
        SZrInferredType argType;

        ZrParser_InferredType_Init(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
        if (provided[index]) {
            if (argNode == ZR_NULL || !ZrParser_ExpressionType_Infer(cs, argNode, &argType)) {
                ZrParser_InferredType_Free(cs->state, &argType);
                goto cleanup_error_with_types;
            }
        } else if (defaultNode != ZR_NULL) {
            if (!ZrParser_ExpressionType_Infer(cs, defaultNode, &argType)) {
                ZrParser_InferredType_Free(cs->state, &argType);
                goto cleanup_error_with_types;
            }
        } else if (member_info_parameter_has_default_at(memberInfo, index)) {
            if (paramType != ZR_NULL) {
                ZrParser_InferredType_Copy(cs->state, &argType, paramType);
            }
        } else {
            ZrParser_InferredType_Free(cs->state, &argType);
            ZrParser_Compiler_Error(cs, "Argument count mismatch", location);
            goto cleanup_error_with_types;
        }

        ZrCore_Array_Push(cs->state, &argTypes, &argType);
    }

    for (TZrSize index = 0; index < parameterTypes->length && index < parameterSlotCount; index++) {
        EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
        SZrInferredType *paramType =
                (SZrInferredType *)ZrCore_Array_Get((SZrArray *)parameterTypes, index);
        SZrInferredType *argType = (SZrInferredType *)ZrCore_Array_Get(&argTypes, index);
        SZrAstNode *argNode = index < parameterSlotCount ? argumentNodes[index] : ZR_NULL;
        TZrChar errorBuffer[ZR_PARSER_DETAIL_BUFFER_LENGTH];

        if (paramType == ZR_NULL || argType == ZR_NULL) {
            goto cleanup_error_with_types;
        }

        passingMode = member_call_parameter_passing_mode_at(parameterPassingModes, index);

        if (passingMode == ZR_PARAMETER_PASSING_MODE_OUT || passingMode == ZR_PARAMETER_PASSING_MODE_REF) {
            if (argNode == ZR_NULL || !member_call_expression_is_assignable_storage(argNode)) {
                snprintf(errorBuffer,
                         sizeof(errorBuffer),
                         "%s argument must be an assignable storage location",
                         member_call_parameter_passing_mode_label(passingMode));
                ZrParser_Compiler_Error(cs, errorBuffer, argNode != ZR_NULL ? argNode->location : location);
                goto cleanup_error_with_types;
            }

            if (!ZrParser_InferredType_Equal(argType, paramType)) {
                snprintf(errorBuffer,
                         sizeof(errorBuffer),
                         "%s argument type mismatch",
                         member_call_parameter_passing_mode_label(passingMode));
                ZrParser_TypeError_Report(cs,
                                          errorBuffer,
                                          paramType,
                                          argType,
                                          argNode != ZR_NULL ? argNode->location : location);
                goto cleanup_error_with_types;
            }
            continue;
        }

        if (requireTaskHandleArgument && index == 0 && !inferred_type_is_task_handle(cs, argType)) {
            ZrParser_Compiler_Error(cs,
                                    "%await expects a zr.task.Task<T>; call .start() on the TaskRunner first",
                                    argNode != ZR_NULL ? argNode->location : location);
            goto cleanup_error_with_types;
        }

        if (!ZrParser_InferredType_IsCompatible(argType, paramType) &&
            !inferred_type_can_use_named_constraint_fallback(cs, argType, paramType)) {
            ZrParser_TypeError_Report(cs,
                                      "Argument type mismatch",
                                      paramType,
                                      argType,
                                      argNode != ZR_NULL ? argNode->location : location);
            goto cleanup_error_with_types;
        }
    }

    if (provided != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      provided,
                                      sizeof(TZrBool) * parameterSlotCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (argumentNodes != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      argumentNodes,
                                      sizeof(SZrAstNode *) * parameterSlotCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    free_inferred_type_array(cs->state, &argTypes);
    return ZR_TRUE;

cleanup_error_with_types:
    free_inferred_type_array(cs->state, &argTypes);
cleanup_error:
    if (provided != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      provided,
                                      sizeof(TZrBool) * parameterSlotCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (argumentNodes != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      argumentNodes,
                                      sizeof(SZrAstNode *) * parameterSlotCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return ZR_FALSE;
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
    static const TZrChar *kBuiltinTypeInfoName = "zr.builtin.TypeInfo";
    SZrInferredType operandType;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_TYPE_QUERY_EXPRESSION) {
        return ZR_FALSE;
    }

    if (node->data.typeQueryExpression.operand == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &operandType, ZR_VALUE_TYPE_OBJECT);
    if (node->data.typeQueryExpression.operand->type != ZR_AST_TYPE_LITERAL_EXPRESSION ||
        node->data.typeQueryExpression.operand->data.typeLiteralExpression.typeInfo == ZR_NULL ||
        node->data.typeQueryExpression.operand->data.typeLiteralExpression.typeInfo->name == ZR_NULL ||
        node->data.typeQueryExpression.operand->data.typeLiteralExpression.typeInfo->name->type != ZR_AST_FUNCTION_TYPE) {
        ZrParser_ExpressionType_Infer(cs, node->data.typeQueryExpression.operand, &operandType);
    }
    ZrParser_InferredType_Free(cs->state, &operandType);

    reflectionTypeName =
            ZrCore_String_Create(cs->state, (TZrNativeString)kBuiltinTypeInfoName, strlen(kBuiltinTypeInfoName));
    if (reflectionTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    ensure_builtin_reflection_compile_type(cs, reflectionTypeName);
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
    SZrGlobalState *global;
    SZrObjectModule *module;
    SZrObject *moduleInfo;
    const ZrLibModuleDescriptor *nativeDescriptor;
    SZrObject *functionsArray;
    SZrObject *constantsArray;
    SZrObject *typesArray;
    SZrObject *modulesArray;
    SZrObject *typeHintsArray;
    SZrTypePrototypeInfo modulePrototype;
    TZrUInt64 pathHash;
    TZrBool pushedImportStack = ZR_FALSE;
    TZrBool result = ZR_FALSE;
    const TZrChar *moduleNameText;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    global = cs->state->global;
    moduleNameText = ZrCore_String_GetNativeString(moduleName);
    nativeDescriptor = ZR_NULL;

    /*
     * Parser tests and embedders sometimes construct a bare GlobalState
     * without going through the library common-state helpers. Attach the
     * builtin native registry lazily so native import metadata remains
     * available when the parser first needs it.
     */
    if (global->nativeModuleLoader == ZR_NULL) {
        (void)ZrLibrary_NativeRegistry_Attach(global);
    }

    if (find_registered_type_prototype_inference_exact_only(cs, moduleName) != ZR_NULL) {
        return ZR_TRUE;
    }

    if (native_module_compile_info_stack_contains(global, moduleName)) {
        return ZR_TRUE;
    }

    ZrCore_Array_Push(cs->state, &global->importCompileInfoStack, &moduleName);
    pushedImportStack = ZR_TRUE;

    if (moduleNameText != ZR_NULL && strcmp(moduleNameText, "zr") == 0) {
        SZrString *builtinFieldName = ZrCore_String_CreateFromNative(cs->state, "builtin");
        SZrString *builtinModuleName = type_inference_builtin_reflection_string(cs, "zr.builtin");
        SZrString *decoratorPatchFieldName = ZrCore_String_CreateFromNative(cs->state, "DecoratorPatch");
        SZrString *decoratorPatchTypeName = ZrCore_String_CreateFromNative(cs->state, "DecoratorPatch");

        native_module_info_init_prototype(cs->state, &modulePrototype, moduleName, ZR_OBJECT_PROTOTYPE_TYPE_MODULE);
        type_inference_apply_default_builtin_root(cs,
                                                  &modulePrototype,
                                                  ZR_OBJECT_PROTOTYPE_TYPE_MODULE,
                                                  moduleNameText);
        if (builtinFieldName != ZR_NULL && builtinModuleName != ZR_NULL) {
            native_module_info_add_field_member(cs->state,
                                                &modulePrototype,
                                                ZR_AST_CLASS_FIELD,
                                                builtinFieldName,
                                                builtinModuleName,
                                                ZR_TRUE,
                                                ZR_MEMBER_CONTRACT_ROLE_NONE);
        }
        if (decoratorPatchTypeName != ZR_NULL) {
            ensure_builtin_reflection_compile_type(cs, decoratorPatchTypeName);
        }
        if (decoratorPatchFieldName != ZR_NULL && decoratorPatchTypeName != ZR_NULL &&
            find_compiler_type_prototype_inference(cs, decoratorPatchTypeName) != ZR_NULL) {
            native_module_info_add_field_member(cs->state,
                                                &modulePrototype,
                                                ZR_AST_CLASS_FIELD,
                                                decoratorPatchFieldName,
                                                decoratorPatchTypeName,
                                                ZR_TRUE,
                                                ZR_MEMBER_CONTRACT_ROLE_NONE);
        }

        ZrCore_Array_Push(cs->state, &cs->typePrototypes, &modulePrototype);
        result = ZR_TRUE;
        goto cleanup;
    }

    module = ZrCore_Module_GetFromCache(cs->state, moduleName);
    if (module == ZR_NULL && global->nativeModuleLoader != ZR_NULL) {
        module = global->nativeModuleLoader(cs->state,
                                            moduleName,
                                            global->nativeModuleLoaderUserData);
        if (module != ZR_NULL) {
            if (module->fullPath == ZR_NULL || module->moduleName == ZR_NULL) {
                pathHash = ZrCore_Module_CalculatePathHash(cs->state, moduleName);
                ZrCore_Module_SetInfo(cs->state, module, moduleName, pathHash, moduleName);
            }
            ZrCore_Module_AddToCache(cs->state, moduleName, module);
        }
    }

    if (module == ZR_NULL) {
        goto load_descriptor_fallback;
    }

    {
        SZrString *infoName = ZrCore_String_Create(cs->state,
                                                   ZR_NATIVE_MODULE_INFO_EXPORT_NAME,
                                                   strlen(ZR_NATIVE_MODULE_INFO_EXPORT_NAME));
        const SZrTypeValue *moduleInfoValue;
        if (infoName == ZR_NULL) {
            goto load_descriptor_fallback;
        }

        moduleInfoValue = ZrCore_Module_GetPubExport(cs->state, module, infoName);
        if (moduleInfoValue == ZR_NULL || moduleInfoValue->type != ZR_VALUE_TYPE_OBJECT) {
            goto load_descriptor_fallback;
        }

        moduleInfo = ZR_CAST_OBJECT(cs->state, moduleInfoValue->value.object);
    }

    goto translate_module_info;

load_descriptor_fallback:
    if (global != ZR_NULL && moduleNameText != ZR_NULL) {
        nativeDescriptor = ZrLibrary_NativeRegistry_FindModule(global, moduleNameText);
    }
    if (nativeDescriptor == ZR_NULL) {
        goto cleanup;
    }

    moduleInfo = native_metadata_make_module_info(cs->state, nativeDescriptor, ZR_NULL);
    if (moduleInfo == ZR_NULL) {
        goto cleanup;
    }

translate_module_info:
    native_module_info_init_prototype(cs->state, &modulePrototype, moduleName, ZR_OBJECT_PROTOTYPE_TYPE_MODULE);
    type_inference_apply_default_builtin_root(cs,
                                              &modulePrototype,
                                              ZR_OBJECT_PROTOTYPE_TYPE_MODULE,
                                              moduleNameText);

    functionsArray = native_module_info_get_array_field(cs->state, moduleInfo, "functions");
    for (TZrSize i = 0; i < native_module_info_array_length(functionsArray); i++) {
        SZrObject *entry = native_module_info_array_get_object(cs->state, functionsArray, i);
        SZrString *name = native_module_info_get_string_field(cs->state, entry, "name");
        SZrString *returnTypeName = native_module_info_get_string_field(cs->state, entry, "returnTypeName");
        TZrUInt32 parameterCount = native_module_info_exact_parameter_count(cs->state, entry);
        TZrUInt32 minArgumentCount = native_module_info_min_argument_count(cs->state, entry, parameterCount);
        TZrUInt32 contractRole = (TZrUInt32)native_module_info_get_int_field(cs->state, entry, "contractRole", 0);
        SZrObject *parametersArray = native_module_info_get_array_field(cs->state, entry, "parameters");
        SZrObject *genericParametersArray = native_module_info_get_array_field(cs->state, entry, "genericParameters");
        if (name != ZR_NULL) {
            native_module_info_add_method_member(cs,
                                                 &modulePrototype,
                                                 ZR_AST_CLASS_METHOD,
                                                 name,
                                                 returnTypeName,
                                                 ZR_TRUE,
                                                 parameterCount,
                                                 minArgumentCount,
                                                 parametersArray,
                                                 genericParametersArray,
                                                 contractRole);
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
                                                ZR_TRUE,
                                                ZR_MEMBER_CONTRACT_ROLE_NONE);
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
                                            ZR_TRUE,
                                            ZR_MEMBER_CONTRACT_ROLE_NONE);
    }

    typeHintsArray = native_module_info_get_array_field(cs->state, moduleInfo, "typeHints");
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
        TZrUInt64 protocolMask = (TZrUInt64)native_module_info_get_int_field(cs->state, entry, "protocolMask", 0);
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

        native_module_info_add_field_member(cs->state,
                                            &modulePrototype,
                                            ZR_AST_CLASS_FIELD,
                                            name,
                                            name,
                                            ZR_TRUE,
                                            ZR_MEMBER_CONTRACT_ROLE_NONE);

        if (find_compiler_type_prototype_inference(cs, name) != ZR_NULL) {
            continue;
        }

        native_module_info_init_prototype(cs->state,
                                          &typePrototype,
                                          name,
                                          (EZrObjectPrototypeType)prototypeTypeValue);
        native_module_info_copy_type_metadata(cs, &typePrototype, entry);
        typePrototype.protocolMask = protocolMask;
        typePrototype.extendsTypeName = extendsTypeName;
        typePrototype.enumValueTypeName = enumValueTypeName;
        typePrototype.allowValueConstruction = allowValueConstruction;
        typePrototype.allowBoxedConstruction = allowBoxedConstruction;
        typePrototype.constructorSignature = constructorSignature;
        if (extendsTypeName != ZR_NULL) {
            native_module_info_add_inherit(cs->state, &typePrototype, extendsTypeName);
        }
        type_inference_apply_default_builtin_root(cs,
                                                  &typePrototype,
                                                  (EZrObjectPrototypeType)prototypeTypeValue,
                                                  moduleNameText);
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
            TZrUInt32 contractRole =
                    (TZrUInt32)native_module_info_get_int_field(cs->state, fieldEntry, "contractRole", 0);
            if (fieldName != ZR_NULL) {
                native_module_info_add_field_member(cs->state,
                                                    &typePrototype,
                                                    fieldMemberType,
                                                    fieldName,
                                                    fieldTypeName,
                                                    ZR_FALSE,
                                                    contractRole);
            }
        }

        for (TZrSize methodIndex = 0; methodIndex < native_module_info_array_length(methodsArray); methodIndex++) {
            SZrObject *methodEntry = native_module_info_array_get_object(cs->state, methodsArray, methodIndex);
            SZrString *methodName = native_module_info_get_string_field(cs->state, methodEntry, "name");
            SZrString *returnTypeName = native_module_info_get_string_field(cs->state, methodEntry, "returnTypeName");
            TZrBool isStatic = native_module_info_get_bool_field(cs->state, methodEntry, "isStatic", ZR_FALSE);
            TZrUInt32 parameterCount = native_module_info_exact_parameter_count(cs->state, methodEntry);
            TZrUInt32 minArgumentCount =
                    native_module_info_min_argument_count(cs->state, methodEntry, parameterCount);
            TZrUInt32 contractRole =
                    (TZrUInt32)native_module_info_get_int_field(cs->state, methodEntry, "contractRole", 0);
            SZrObject *parametersArray = native_module_info_get_array_field(cs->state, methodEntry, "parameters");
            SZrObject *methodGenericParametersArray =
                    native_module_info_get_array_field(cs->state, methodEntry, "genericParameters");
            if (methodName != ZR_NULL) {
                native_module_info_add_method_member(cs,
                                                     &typePrototype,
                                                     methodMemberType,
                                                     methodName,
                                                     returnTypeName,
                                                     isStatic,
                                                     parameterCount,
                                                     minArgumentCount,
                                                     parametersArray,
                                                     methodGenericParametersArray,
                                                     contractRole);
            }
        }

        for (TZrSize metaIndex = 0; metaIndex < native_module_info_array_length(metaMethodsArray); metaIndex++) {
            SZrObject *metaEntry = native_module_info_array_get_object(cs->state, metaMethodsArray, metaIndex);
            TZrInt64 metaTypeValue =
                    native_module_info_get_int_field(cs->state, metaEntry, "metaType", ZR_META_ENUM_MAX);
            SZrString *returnTypeName = native_module_info_get_string_field(cs->state, metaEntry, "returnTypeName");
            TZrUInt32 parameterCount = native_module_info_exact_parameter_count(cs->state, metaEntry);
            TZrUInt32 minArgumentCount =
                    native_module_info_min_argument_count(cs->state, metaEntry, parameterCount);
            SZrObject *parametersArray = native_module_info_get_array_field(cs->state, metaEntry, "parameters");
            SZrObject *metaGenericParametersArray =
                    native_module_info_get_array_field(cs->state, metaEntry, "genericParameters");

            if (metaTypeValue < 0 || metaTypeValue >= ZR_META_ENUM_MAX) {
                continue;
            }

            native_module_info_add_meta_method_member(cs,
                                                      &typePrototype,
                                                      (EZrMetaType)metaTypeValue,
                                                      returnTypeName,
                                                      parameterCount,
                                                      minArgumentCount,
                                                      parametersArray,
                                                      metaGenericParametersArray);
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
                                                    ZR_TRUE,
                                                    ZR_MEMBER_CONTRACT_ROLE_NONE);
            }
        }

        ZrCore_Array_Push(cs->state, &cs->typePrototypes, &typePrototype);
    }

    for (TZrSize hintIndex = 0; hintIndex < native_module_info_array_length(typeHintsArray); hintIndex++) {
        SZrObject *hintEntry = native_module_info_array_get_object(cs->state, typeHintsArray, hintIndex);
        native_module_info_add_property_hint_member(cs, &modulePrototype, hintEntry);
    }

    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &modulePrototype);
    result = ZR_TRUE;

cleanup:
    if (pushedImportStack && global != ZR_NULL && global->importCompileInfoStack.length > 0) {
        global->importCompileInfoStack.length--;
    }

    return result;
}

static void type_inference_ensure_imported_module_runtime_metadata(SZrCompilerState *cs, SZrString *moduleName) {
    if (cs == ZR_NULL || moduleName == ZR_NULL) {
        return;
    }

    ensure_builtin_reflection_compile_type(cs, moduleName);
    if (find_compiler_type_prototype_inference(cs, moduleName) == ZR_NULL) {
        ensure_import_module_compile_info(cs, moduleName);
    }
}

static SZrString *type_inference_build_canonical_module_member_type_name(SZrCompilerState *cs,
                                                                         SZrString *canonicalMemberTypeName,
                                                                         SZrString *requestedMemberTypeName) {
    const TZrChar *canonicalText;
    const TZrChar *requestedText;
    const TZrChar *requestedGenericStart;
    const TZrChar *canonicalGenericStart;
    TZrSize canonicalBaseLength;
    TZrChar buffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    TZrInt32 written;

    if (cs == ZR_NULL || canonicalMemberTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    canonicalText = ZrCore_String_GetNativeString(canonicalMemberTypeName);
    if (canonicalText == ZR_NULL) {
        return ZR_NULL;
    }

    requestedText = requestedMemberTypeName != ZR_NULL ? ZrCore_String_GetNativeString(requestedMemberTypeName) : ZR_NULL;
    requestedGenericStart = requestedText != ZR_NULL ? strchr(requestedText, '<') : ZR_NULL;
    if (requestedGenericStart == ZR_NULL) {
        return canonicalMemberTypeName;
    }

    canonicalGenericStart = strchr(canonicalText, '<');
    canonicalBaseLength = canonicalGenericStart != ZR_NULL ? (TZrSize)(canonicalGenericStart - canonicalText)
                                                           : strlen(canonicalText);
    written = snprintf(buffer,
                       sizeof(buffer),
                       "%.*s%s",
                       (int)canonicalBaseLength,
                       canonicalText,
                       requestedGenericStart);
    if (written <= 0 || (TZrSize)written >= sizeof(buffer)) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(cs->state, buffer, (TZrSize)written);
}

static void type_inference_normalize_array_type_from_name(SZrCompilerState *cs, SZrInferredType *type) {
    SZrInferredType normalizedType;

    if (cs == ZR_NULL || type == ZR_NULL || type->typeName == ZR_NULL) {
        return;
    }
    if (type->baseType == ZR_VALUE_TYPE_ARRAY && type->elementTypes.length > 0) {
        return;
    }

    ZrParser_InferredType_Init(cs->state, &normalizedType, ZR_VALUE_TYPE_OBJECT);
    if (inferred_type_from_type_name(cs, type->typeName, &normalizedType) &&
        normalizedType.baseType == ZR_VALUE_TYPE_ARRAY &&
        normalizedType.elementTypes.length > 0) {
        normalizedType.isNullable = type->isNullable;
        normalizedType.ownershipQualifier = type->ownershipQualifier;
        ZrParser_InferredType_Free(cs->state, type);
        ZrParser_InferredType_Copy(cs->state, type, &normalizedType);
    }
    ZrParser_InferredType_Free(cs->state, &normalizedType);
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
    type_inference_normalize_array_type_from_name(cs, &currentType);

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
                            if (knownPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
                                ZrParser_InferredType_Free(cs->state, &currentType);
                                ZrParser_InferredType_Init(cs->state, &currentType, ZR_VALUE_TYPE_OBJECT);
                                currentIsPrototypeReference = ZR_FALSE;
                                continue;
                            }
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
                    if (currentType.typeName != ZR_NULL &&
                        getenv("ZR_VM_TRACE_PROJECT_STARTUP") != ZR_NULL &&
                        ZrCore_String_GetNativeString(currentType.typeName) != ZR_NULL &&
                        strchr(ZrCore_String_GetNativeString(currentType.typeName), '<') != ZR_NULL) {
                        fprintf(stderr,
                                "[zr-debug-site] type_inference_native.currentType=%s member=%s\n",
                                ZrCore_String_GetNativeString(currentType.typeName),
                                memberLookupName != ZR_NULL ? ZrCore_String_GetNativeString(memberLookupName) : "<null>");
                    }
                    type_inference_ensure_imported_module_runtime_metadata(cs, currentType.typeName);
                    memberInfo = find_compiler_type_member_inference(cs, currentType.typeName, memberLookupName);
                }
                if (memberInfo == ZR_NULL) {
                    ZrParser_InferredType_Free(cs->state, &currentType);
                    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                    return ZR_TRUE;
                }

                {
                    SZrString *prototypeCandidateTypeName = memberInfo->fieldTypeName;
                    if (type_name_is_module_prototype_inference(cs, currentType.typeName) &&
                        memberResolvedTypeName != ZR_NULL) {
                        SZrString *canonicalMemberTypeName =
                                type_inference_build_canonical_module_member_type_name(cs,
                                                                                      memberInfo->fieldTypeName,
                                                                                      memberResolvedTypeName);
                        if (canonicalMemberTypeName != ZR_NULL) {
                            prototypeCandidateTypeName = canonicalMemberTypeName;
                        }
                    }

                if ((memberInfo->memberType == ZR_AST_STRUCT_FIELD || memberInfo->memberType == ZR_AST_CLASS_FIELD) &&
                    prototypeCandidateTypeName != ZR_NULL &&
                    find_compiler_type_prototype_inference(cs, prototypeCandidateTypeName) != ZR_NULL &&
                    !type_name_is_module_prototype_inference(cs, prototypeCandidateTypeName)) {
                    nextIsPrototypeReference = ZR_TRUE;
                }
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
                    TZrBool ffiHandled = ZR_FALSE;

                    if (memberInfo->contractRole == ZR_MEMBER_CONTRACT_ROLE_TASK_AWAIT) {
                        SZrFunctionCall *awaitCall = &members->nodes[i + 1]->data.functionCall;
                        SZrInferredType awaitedType;
                        SZrAstNode *awaitedNode =
                                (awaitCall != ZR_NULL && awaitCall->args != ZR_NULL && awaitCall->args->count > 0)
                                        ? awaitCall->args->nodes[0]
                                        : ZR_NULL;

                        ZrParser_InferredType_Init(cs->state, &awaitedType, ZR_VALUE_TYPE_OBJECT);
                        if (awaitedNode == ZR_NULL || !ZrParser_ExpressionType_Infer(cs, awaitedNode, &awaitedType)) {
                            ZrParser_InferredType_Free(cs->state, &currentType);
                            ZrParser_InferredType_Free(cs->state, &nextType);
                            ZrParser_InferredType_Free(cs->state, &awaitedType);
                            return ZR_FALSE;
                        }

                        if (!inferred_type_is_task_handle(cs, &awaitedType)) {
                            ZrParser_Compiler_Error(cs,
                                                    "%await expects a zr.task.Task<T>; call .start() on the TaskRunner first",
                                                    awaitedNode->location);
                            ZrParser_InferredType_Free(cs->state, &currentType);
                            ZrParser_InferredType_Free(cs->state, &nextType);
                            ZrParser_InferredType_Free(cs->state, &awaitedType);
                            return ZR_FALSE;
                        }

                        ZrParser_InferredType_Free(cs->state, &awaitedType);
                    }

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

                    if (!infer_ffi_member_call_type(cs,
                                                    &currentType,
                                                    memberInfo,
                                                    &members->nodes[i + 1]->data.functionCall,
                                                    &nextType,
                                                    &ffiHandled)) {
                        ZrParser_InferredType_Free(cs->state, &currentType);
                        ZrParser_InferredType_Free(cs->state, &nextType);
                        free_resolved_call_signature(cs->state, &resolvedMemberSignature);
                        return ZR_FALSE;
                    }

                    if (!ffiHandled) {
                        inferred_type_from_member_call(cs,
                                                       memberInfo,
                                                       &members->nodes[i + 1]->data.functionCall,
                                                       &resolvedMemberSignature,
                                                       &nextType);
                    }
                    free_resolved_call_signature(cs->state, &resolvedMemberSignature);
                    i++;
                    nextIsPrototypeReference = ZR_FALSE;
                } else {
                    if (memberExpr->property->type == ZR_AST_TYPE &&
                        find_compiler_type_prototype_inference(cs, currentType.typeName) != ZR_NULL &&
                        type_name_is_module_prototype_inference(cs, currentType.typeName) &&
                        memberResolvedTypeName != ZR_NULL) {
                        SZrString *qualifiedMemberTypeName =
                                type_inference_build_canonical_module_member_type_name(cs,
                                                                                      memberInfo->fieldTypeName,
                                                                                      memberResolvedTypeName);
                        SZrString *candidateTypeName =
                                qualifiedMemberTypeName != ZR_NULL ? qualifiedMemberTypeName : memberResolvedTypeName;
                        inferred_type_from_type_name(cs, candidateTypeName, &nextType);
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
                SZrTypeMemberInfo *callMetaMember = ZR_NULL;
                SZrResolvedCallSignature resolvedCallSignature;
                TZrChar genericDiagnostic[ZR_PARSER_ERROR_BUFFER_LENGTH];
                SZrInferredType nextType;

                if (currentIsPrototypeReference) {
                    ZrParser_Compiler_Error(cs,
                                            "Prototype references are not callable; use $target(...) or new target(...)",
                                            memberNode->location);
                    ZrParser_InferredType_Free(cs->state, &currentType);
                    return ZR_FALSE;
                }

                if (currentType.typeName != ZR_NULL) {
                    callMetaMember = find_compiler_type_meta_member_inference(cs, currentType.typeName, ZR_META_CALL);
                }
                if (callMetaMember == ZR_NULL) {
                    ZrParser_InferredType_Free(cs->state, &currentType);
                    ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
                    return ZR_TRUE;
                }

                memset(&resolvedCallSignature, 0, sizeof(resolvedCallSignature));
                ZrParser_InferredType_Init(cs->state, &resolvedCallSignature.returnType, ZR_VALUE_TYPE_OBJECT);
                ZrCore_Array_Construct(&resolvedCallSignature.parameterTypes);
                ZrCore_Array_Construct(&resolvedCallSignature.parameterPassingModes);
                ZrParser_InferredType_Init(cs->state, &nextType, ZR_VALUE_TYPE_OBJECT);
                genericDiagnostic[0] = '\0';

                if (!receiver_ownership_can_call_member(currentType.ownershipQualifier,
                                                        callMetaMember->receiverQualifier)) {
                    ZrParser_Compiler_Error(cs,
                                            receiver_ownership_call_error(currentType.ownershipQualifier),
                                            memberNode->location);
                    ZrParser_InferredType_Free(cs->state, &currentType);
                    ZrParser_InferredType_Free(cs->state, &nextType);
                    free_resolved_call_signature(cs->state, &resolvedCallSignature);
                    return ZR_FALSE;
                }

                if (resolve_generic_member_call_signature_detailed(cs,
                                                                   callMetaMember,
                                                                   &memberNode->data.functionCall,
                                                                   &resolvedCallSignature,
                                                                   genericDiagnostic,
                                                                   sizeof(genericDiagnostic)) !=
                    ZR_GENERIC_CALL_RESOLVE_OK) {
                    ZrParser_Compiler_Error(cs,
                                            genericDiagnostic[0] != '\0' ? genericDiagnostic
                                                                         : "Unable to resolve callable @call signature",
                                            memberNode->location);
                    ZrParser_InferredType_Free(cs->state, &currentType);
                    ZrParser_InferredType_Free(cs->state, &nextType);
                    free_resolved_call_signature(cs->state, &resolvedCallSignature);
                    return ZR_FALSE;
                }

                if (!validate_member_call_arguments(cs,
                                                    callMetaMember,
                                                    &resolvedCallSignature,
                                                    &memberNode->data.functionCall,
                                                    memberNode->location) ||
                    !inferred_type_from_member_call(cs,
                                                    callMetaMember,
                                                    &memberNode->data.functionCall,
                                                    &resolvedCallSignature,
                                                    &nextType)) {
                    ZrParser_InferredType_Free(cs->state, &currentType);
                    ZrParser_InferredType_Free(cs->state, &nextType);
                    free_resolved_call_signature(cs->state, &resolvedCallSignature);
                    return ZR_FALSE;
                }

                free_resolved_call_signature(cs->state, &resolvedCallSignature);
                ZrParser_InferredType_Free(cs->state, &currentType);
                ZrParser_InferredType_Init(cs->state, &currentType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Copy(cs->state, &currentType, &nextType);
                ZrParser_InferredType_Free(cs->state, &nextType);
                currentIsPrototypeReference = ZR_FALSE;
                continue;
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
        case ZR_OWNERSHIP_QUALIFIER_LOANED: return "%loaned ";
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
                                       const TZrChar *text);

static TZrBool type_name_string_try_append_async_surface(const SZrInferredType *type,
                                                         TZrChar *buffer,
                                                         TZrSize bufferSize,
                                                         TZrSize *writeIndex) {
    const TZrChar *baseName;
    const TZrChar *innerStart;
    const TZrChar *innerEnd;
    TZrSize innerLength;

    if (type == ZR_NULL || buffer == ZR_NULL || writeIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    baseName = type_name_string_get_base_or_named_type(type);
    if (baseName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strncmp(baseName, "TaskRunner<", 11) == 0) {
        innerStart = baseName + 11;
    } else if (strncmp(baseName, "zr.task.TaskRunner<", 19) == 0) {
        innerStart = baseName + 19;
    } else {
        return ZR_FALSE;
    }

    innerEnd = strrchr(innerStart, '>');
    if (innerEnd == ZR_NULL || innerEnd <= innerStart) {
        return ZR_FALSE;
    }

    innerLength = (TZrSize)(innerEnd - innerStart);
    if (!type_name_string_append(buffer, bufferSize, writeIndex, "%async ")) {
        return ZR_FALSE;
    }

    if (*writeIndex + innerLength >= bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer + *writeIndex, innerStart, innerLength);
    *writeIndex += innerLength;
    buffer[*writeIndex] = '\0';
    return ZR_TRUE;
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
    if (!type_name_string_append(buffer, bufferSize, writeIndex, ownershipPrefix)) {
        return ZR_FALSE;
    }

    if (!type_name_string_try_append_async_surface(type, buffer, bufferSize, writeIndex) &&
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
