#include "compiler_parameter_metadata.h"

TZrBool compiler_parameter_metadata_write_descriptor(
        SZrCompilerState *cs,
        SZrObject *descriptor,
        const SZrFunctionMetadataParameter *parameter,
        TZrUInt32 position) {
    SZrObject *decoratorsArray;
    SZrTypeValue decoratorsValue;
    ZrExternCompilerTempRoot metadataRoot = {0};
    ZrExternCompilerTempRoot decoratorsRoot = {0};
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || descriptor == ZR_NULL || parameter == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!extern_compiler_temp_root_begin(cs, &metadataRoot) ||
        !extern_compiler_temp_root_begin(cs, &decoratorsRoot)) {
        goto cleanup;
    }
    if (parameter->hasDecoratorMetadata &&
        !extern_compiler_temp_root_set_value(
                &metadataRoot, &parameter->decoratorMetadataValue)) {
        goto cleanup;
    }
    decoratorsArray = extern_compiler_new_array_constant(cs);
    if (decoratorsArray == ZR_NULL ||
        !extern_compiler_temp_root_set_object(
                &decoratorsRoot, decoratorsArray, ZR_VALUE_TYPE_ARRAY)) {
        goto cleanup;
    }

    if (!extern_compiler_descriptor_set_int_field(
                cs, descriptor, "position", position) ||
        !extern_compiler_descriptor_set_int_field(
                cs, descriptor, "decoratorCount", parameter->decoratorCount)) {
        goto cleanup;
    }
    if (parameter->name != ZR_NULL &&
        !extern_compiler_descriptor_set_string_object_field(
                cs, descriptor, "name", parameter->name)) {
        goto cleanup;
    }
    for (TZrUInt32 index = 0U; index < parameter->decoratorCount; index++) {
        SZrString *decoratorName = parameter->decoratorNames[index];
        SZrTypeValue decoratorValue;

        if (decoratorName == ZR_NULL) {
            continue;
        }
        ZrCore_Value_InitAsRawObject(
                cs->state,
                &decoratorValue,
                ZR_CAST_RAW_OBJECT_AS_SUPER(decoratorName));
        decoratorValue.type = ZR_VALUE_TYPE_STRING;
        if (!extern_compiler_push_array_value(
                    cs, decoratorsArray, &decoratorValue)) {
            goto cleanup;
        }
    }
    ZrCore_Value_InitAsRawObject(
            cs->state,
            &decoratorsValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(decoratorsArray));
    decoratorsValue.type = ZR_VALUE_TYPE_ARRAY;
    if (!extern_compiler_set_object_field(
                cs, descriptor, "decorators", &decoratorsValue)) {
        goto cleanup;
    }
    if (parameter->hasDecoratorMetadata &&
        !extern_compiler_set_object_field(
                cs,
                descriptor,
                "metadata",
                &parameter->decoratorMetadataValue)) {
        goto cleanup;
    }
    success = ZR_TRUE;

cleanup:
    extern_compiler_temp_root_end(&decoratorsRoot);
    extern_compiler_temp_root_end(&metadataRoot);
    return success;
}

TZrBool compiler_parameter_metadata_attach_member_array(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *memberInfo,
        SZrAstNodeArray *params,
        SZrAstNode *functionNode,
        const TZrChar *fieldName) {
    SZrFunctionMetadataParameter *parameters = ZR_NULL;
    TZrUInt32 parameterCount = 0U;
    SZrObject *metadataObject;
    SZrObject *parametersArray;
    SZrTypeValue parametersValue;
    SZrAstNode *previousFunctionNode;
    ZrExternCompilerTempRoot metadataRoot = {0};
    ZrExternCompilerTempRoot parametersRoot = {0};
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || memberInfo == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }
    previousFunctionNode = cs->currentFunctionNode;
    cs->currentFunctionNode = functionNode;
    if (!compiler_build_function_parameter_metadata(
                cs, params, ZR_FALSE, &parameters, &parameterCount)) {
        cs->currentFunctionNode = previousFunctionNode;
        return ZR_FALSE;
    }
    cs->currentFunctionNode = previousFunctionNode;

    if (!extern_compiler_temp_root_begin(cs, &metadataRoot) ||
        !extern_compiler_temp_root_begin(cs, &parametersRoot)) {
        goto cleanup;
    }
    metadataObject = memberInfo->hasDecoratorMetadata &&
                             memberInfo->decoratorMetadataValue.type ==
                                     ZR_VALUE_TYPE_OBJECT &&
                             memberInfo->decoratorMetadataValue.value.object != ZR_NULL
                             ? ZR_CAST_OBJECT(
                                       cs->state,
                                       memberInfo->decoratorMetadataValue.value.object)
                             : extern_compiler_new_object_constant(cs);
    parametersArray = extern_compiler_new_array_constant(cs);
    if (metadataObject == ZR_NULL || parametersArray == ZR_NULL ||
        !extern_compiler_temp_root_set_object(
                &metadataRoot, metadataObject, ZR_VALUE_TYPE_OBJECT) ||
        !extern_compiler_temp_root_set_object(
                &parametersRoot, parametersArray, ZR_VALUE_TYPE_ARRAY)) {
        goto cleanup;
    }

    for (TZrUInt32 index = 0U; index < parameterCount; index++) {
        SZrObject *descriptor;
        SZrTypeValue descriptorValue;
        ZrExternCompilerTempRoot descriptorRoot = {0};
        TZrBool descriptorSuccess = ZR_FALSE;

        if (!extern_compiler_temp_root_begin(cs, &descriptorRoot)) {
            goto cleanup;
        }
        descriptor = extern_compiler_new_object_constant(cs);
        if (descriptor != ZR_NULL &&
            extern_compiler_temp_root_set_object(
                    &descriptorRoot, descriptor, ZR_VALUE_TYPE_OBJECT) &&
            compiler_parameter_metadata_write_descriptor(
                    cs, descriptor, &parameters[index], index)) {
            ZrCore_Value_InitAsRawObject(
                    cs->state,
                    &descriptorValue,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor));
            descriptorValue.type = ZR_VALUE_TYPE_OBJECT;
            descriptorSuccess = extern_compiler_push_array_value(
                    cs, parametersArray, &descriptorValue);
        }
        extern_compiler_temp_root_end(&descriptorRoot);
        if (!descriptorSuccess) {
            goto cleanup;
        }
    }

    ZrCore_Value_InitAsRawObject(
            cs->state,
            &parametersValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(parametersArray));
    parametersValue.type = ZR_VALUE_TYPE_ARRAY;
    if (!extern_compiler_set_object_field(
                cs, metadataObject, fieldName, &parametersValue)) {
        goto cleanup;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state,
            &memberInfo->decoratorMetadataValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(metadataObject));
    memberInfo->decoratorMetadataValue.type = ZR_VALUE_TYPE_OBJECT;
    memberInfo->hasDecoratorMetadata = ZR_TRUE;
    success = ZR_TRUE;

cleanup:
    extern_compiler_temp_root_end(&parametersRoot);
    extern_compiler_temp_root_end(&metadataRoot);
    compiler_free_function_parameter_metadata(
            cs->state, parameters, parameterCount);
    return success;
}
