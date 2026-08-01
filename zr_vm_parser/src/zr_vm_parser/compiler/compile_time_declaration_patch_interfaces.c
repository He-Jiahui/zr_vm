#include "compile_time_declaration_patch_interfaces.h"

#include "compile_expression_internal.h"
#include "zr_vm_parser/declaration_transform_contract.h"

#include "zr_vm_core/reflection.h"

static const SZrTypeValue *patch_interface_array_at(
        SZrCompilerState *cs,
        const SZrTypeValue *arrayValue,
        TZrSize index) {
    SZrObject *array;
    SZrTypeValue key;

    if (cs == ZR_NULL || arrayValue == ZR_NULL ||
        arrayValue->type != ZR_VALUE_TYPE_ARRAY ||
        arrayValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    array = ZR_CAST_OBJECT(cs->state, arrayValue->value.object);
    if (array == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsInt(cs->state, &key, (TZrInt64)index);
    return ZrCore_Object_GetValue(cs->state, array, &key);
}

static TZrBool patch_interface_identity_in_array(
        SZrCompilerState *cs,
        const SZrArray *array,
        const SZrString *canonicalName) {
    if (cs == ZR_NULL || array == ZR_NULL || canonicalName == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < array->length; index++) {
        SZrString **candidate =
                (SZrString **)ZrCore_Array_Get((SZrArray *)array, index);
        SZrTypePrototypeInfo *candidateInfo;

        if (candidate == ZR_NULL || *candidate == ZR_NULL) {
            continue;
        }
        if (ZrCore_String_Equal(*candidate, (SZrString *)canonicalName)) {
            return ZR_TRUE;
        }
        candidateInfo = find_compiler_type_prototype(cs, *candidate);
        if (candidateInfo != ZR_NULL && candidateInfo->name != ZR_NULL &&
            ZrCore_String_Equal(
                    candidateInfo->name, (SZrString *)canonicalName)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool patch_interface_name_in_prepared(
        const SZrParserCompileTimePatchInterfaceAdds *interfaceAdds,
        TZrSize count,
        const SZrString *name) {
    if (interfaceAdds == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < count; index++) {
        if (interfaceAdds->typeNames[index] != ZR_NULL &&
            ZrCore_String_Equal(
                    interfaceAdds->typeNames[index], (SZrString *)name)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool patch_interface_error(
        SZrCompilerState *cs,
        const TZrChar *message,
        SZrFileRange location) {
    ZrParser_CompileTime_Error(
            cs, ZR_COMPILE_TIME_ERROR_ERROR, message, location);
    return ZR_FALSE;
}

TZrBool ZrParser_CompileTime_PreparePatchInterfaceAdds(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *targetInfo,
        const SZrTypeValue *interfaceAddsValue,
        SZrFileRange location,
        SZrParserCompileTimePatchInterfaceAdds *result) {
    SZrObject *array;

    if (cs == ZR_NULL || targetInfo == ZR_NULL ||
        interfaceAddsValue == ZR_NULL || result == ZR_NULL ||
        interfaceAddsValue->type != ZR_VALUE_TYPE_ARRAY ||
        interfaceAddsValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(result, 0, sizeof(*result));
    array = ZR_CAST_OBJECT(cs->state, interfaceAddsValue->value.object);
    if (array == ZR_NULL) {
        return ZR_FALSE;
    }
    result->count = array->nodeMap.elementCount;
    if (result->count == 0U) {
        return ZR_TRUE;
    }
    if (result->count > ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS ||
        result->count > (TZrSize)(SIZE_MAX / sizeof(*result->typeIds)) ||
        result->count > (TZrSize)(SIZE_MAX / sizeof(*result->typeNames))) {
        return patch_interface_error(
                cs,
                "declaration_transform.interface_add: interface addition budget exceeded",
                location);
    }

    result->typeIds = (TZrTypeId *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            result->count * sizeof(*result->typeIds),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    result->typeNames = (SZrString **)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            result->count * sizeof(*result->typeNames),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (result->typeIds == ZR_NULL || result->typeNames == ZR_NULL) {
        ZrParser_CompileTime_FreePatchInterfaceAdds(cs, result);
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(
            result->typeIds, 0, result->count * sizeof(*result->typeIds));
    ZrCore_Memory_RawSet(
            result->typeNames, 0, result->count * sizeof(*result->typeNames));

    for (TZrSize index = 0; index < result->count; index++) {
        const SZrTypeValue *value =
                patch_interface_array_at(cs, interfaceAddsValue, index);
        SZrReflectionTypeIdentity identity;
        SZrString *canonicalName = ZR_NULL;
        SZrTypePrototypeInfo *interfaceInfo;

        ZrCore_Memory_RawSet(&identity, 0, sizeof(identity));
        if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT ||
            value->value.object == ZR_NULL ||
            !ZrCore_Reflection_ReadTypeIdObject(
                    cs->state,
                    ZR_CAST_OBJECT(cs->state, value->value.object),
                    &identity,
                    &canonicalName) ||
            identity.canonicalTypeId == ZR_SEMANTIC_ID_INVALID ||
            canonicalName == ZR_NULL) {
            patch_interface_error(
                    cs,
                    "declaration_transform.interface_add: expected canonical TypeId",
                    location);
            ZrParser_CompileTime_FreePatchInterfaceAdds(cs, result);
            return ZR_FALSE;
        }
        interfaceInfo = find_compiler_type_prototype(cs, canonicalName);
        if (interfaceInfo == ZR_NULL ||
            interfaceInfo->type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
            patch_interface_error(
                    cs,
                    "declaration_transform.interface_add: TypeId must resolve to an interface",
                    location);
            ZrParser_CompileTime_FreePatchInterfaceAdds(cs, result);
            return ZR_FALSE;
        }
        if (patch_interface_identity_in_array(
                    cs, &targetInfo->inherits, canonicalName) ||
            patch_interface_identity_in_array(
                    cs, &targetInfo->implements, canonicalName) ||
            patch_interface_name_in_prepared(result, index, canonicalName)) {
            patch_interface_error(
                    cs,
                    "declaration_transform.interface_add: duplicate interface",
                    location);
            ZrParser_CompileTime_FreePatchInterfaceAdds(cs, result);
            return ZR_FALSE;
        }
        result->typeIds[index] = identity.canonicalTypeId;
        result->typeNames[index] = canonicalName;
    }
    return ZR_TRUE;
}

void ZrParser_CompileTime_FreePatchInterfaceAdds(
        SZrCompilerState *cs,
        SZrParserCompileTimePatchInterfaceAdds *interfaceAdds) {
    if (cs == ZR_NULL || interfaceAdds == ZR_NULL) {
        return;
    }
    if (interfaceAdds->typeIds != ZR_NULL) {
        ZR_MEMORY_RAW_FREE_LIST(
                cs->state->global, interfaceAdds->typeIds, interfaceAdds->count);
    }
    if (interfaceAdds->typeNames != ZR_NULL) {
        ZR_MEMORY_RAW_FREE_LIST(
                cs->state->global, interfaceAdds->typeNames, interfaceAdds->count);
    }
    ZrCore_Memory_RawSet(interfaceAdds, 0, sizeof(*interfaceAdds));
}
