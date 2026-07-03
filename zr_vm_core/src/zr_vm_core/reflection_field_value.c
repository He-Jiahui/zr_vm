//
// FieldInfo token value read/write support for runtime reflection.
//

#include "zr_vm_core/reflection.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

#include "reflection_field_value_nested.h"
#include "reflection_field_value_primitive.h"

#include <string.h>

static const SZrTypeLayoutField *reflection_field_value_find_field_layout_by_offset(
        const SZrTypeLayout *ownerTypeLayout,
        TZrUInt32 byteOffset,
        TZrUInt32 fieldTypeLayoutId) {
    TZrUInt32 index;

    if (ownerTypeLayout == ZR_NULL || ownerTypeLayout->fields == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0u; index < ownerTypeLayout->fieldCount; index++) {
        const SZrTypeLayoutField *field = ownerTypeLayout->fields + index;

        if (field->byteOffset == byteOffset &&
            (fieldTypeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
             field->typeLayoutIndex == fieldTypeLayoutId)) {
            return field;
        }
    }

    return ZR_NULL;
}

static TZrBool reflection_field_value_resolve_field_layout(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        TZrUInt32 inlineStorageByteSize,
        SZrReflectionResolvedToken *outResolved,
        const SZrTypeLayoutField **outFieldLayout) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;

    if (outResolved != ZR_NULL) {
        memset(outResolved, 0, sizeof(*outResolved));
    }
    if (outFieldLayout != ZR_NULL) {
        *outFieldLayout = ZR_NULL;
    }

    if (runtime == ZR_NULL ||
        outFieldLayout == ZR_NULL ||
        !ZrCore_Reflection_ResolveToken(runtime, fieldToken, &resolved) ||
        resolved.kind != ZR_REFLECTION_RESOLVED_TOKEN_FIELD ||
        resolved.ownerTypeLayout == ZR_NULL) {
        return ZR_FALSE;
    }

    fieldLayout = reflection_field_value_find_field_layout_by_offset(resolved.ownerTypeLayout,
                                                                     resolved.byteOffset,
                                                                     resolved.fieldTypeLayoutId);
    if (fieldLayout == ZR_NULL ||
        fieldLayout->byteOffset > inlineStorageByteSize ||
        fieldLayout->byteSize > inlineStorageByteSize - fieldLayout->byteOffset) {
        return ZR_FALSE;
    }

    if (outResolved != ZR_NULL) {
        *outResolved = resolved;
    }
    *outFieldLayout = fieldLayout;
    return ZR_TRUE;
}

static TZrBool reflection_field_value_read_field_type_node(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        SZrMetadataRuntimeSignatureTypeNodeView *outNodeView) {
    SZrMetadataRuntimeSignatureView signatureView;

    if (outNodeView != ZR_NULL) {
        memset(outNodeView, 0, sizeof(*outNodeView));
    }

    if (runtime == ZR_NULL ||
        outNodeView == ZR_NULL ||
        !ZrCore_MetadataRuntime_ReadSignatureView(runtime, fieldToken, &signatureView) ||
        signatureView.rootNode != ZR_METADATA_SIGNATURE_NODE_FIELD_SIG ||
        !ZrCore_MetadataRuntime_ReadSignatureTypeNode(&signatureView.blob,
                                                      signatureView.fieldTypeBlobOffset,
                                                      outNodeView)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool reflection_field_value_signature_node_is_inline_layout(
        const SZrMetadataRuntimeSignatureTypeNodeView *nodeView) {
    return nodeView != ZR_NULL &&
           (nodeView->node == ZR_METADATA_SIGNATURE_NODE_TYPE_DEF ||
            nodeView->node == ZR_METADATA_SIGNATURE_NODE_TYPE_REF);
}

static TZrBool reflection_field_value_can_read_inline_borrowed_view(
        const SZrReflectionResolvedToken *resolved,
        const SZrTypeLayoutField *fieldLayout,
        const SZrMetadataRuntimeSignatureTypeNodeView *nodeView) {
    TZrUInt32 unsupportedFieldFlags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                                     ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                                     ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;

    if (resolved == ZR_NULL ||
        fieldLayout == ZR_NULL ||
        resolved->fieldTypeLayout == ZR_NULL ||
        !reflection_field_value_signature_node_is_inline_layout(nodeView) ||
        (fieldLayout->flags & unsupportedFieldFlags) != 0u ||
        fieldLayout->byteSize == 0u ||
        fieldLayout->byteSize != resolved->fieldTypeLayout->byteSize) {
        return ZR_FALSE;
    }

    return (TZrBool)(resolved->fieldTypeLayout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT ||
                     resolved->fieldTypeLayout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION);
}

static TZrBool reflection_field_value_can_write_inline_borrowed_source(
        const SZrReflectionResolvedToken *resolved,
        const SZrTypeLayoutField *fieldLayout,
        const SZrMetadataRuntimeSignatureTypeNodeView *nodeView,
        const SZrTypeValue *value) {
    if (value == ZR_NULL ||
        value->type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        value->value.nativeObject.nativePointer == ZR_NULL ||
        !reflection_field_value_can_read_inline_borrowed_view(resolved, fieldLayout, nodeView)) {
        return ZR_FALSE;
    }

    if (ZrCore_TypeLayout_CanRawCopy(resolved->fieldTypeLayout)) {
        return ZR_TRUE;
    }

    return (TZrBool)(resolved->fieldTypeLayout->copyKind == (TZrUInt8)ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY);
}

static TZrBool reflection_field_value_read_field_info_object_field(
        SZrState *state,
        SZrObject *fieldInfo,
        const TZrChar *fieldName,
        SZrTypeValue *outValue) {
    SZrGcNativeCallPin fieldInfoPin = {0};
    SZrGcNativeCallPin fieldNamePin = {0};
    SZrString *fieldString;
    SZrTypeValue key;
    const SZrTypeValue *storedValue;
    TZrBool result = ZR_FALSE;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (state == ZR_NULL || fieldInfo == ZR_NULL || fieldName == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldInfo), &fieldInfoPin)) {
        return ZR_FALSE;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        ZrCore_Gc_NativeCallUnpin(state->global, &fieldInfoPin);
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;

    if (!ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString), &fieldNamePin)) {
        ZrCore_Gc_NativeCallUnpin(state->global, &fieldInfoPin);
        return ZR_FALSE;
    }

    storedValue = ZrCore_Object_GetValue(state, fieldInfo, &key);
    if (storedValue != ZR_NULL) {
        *outValue = *storedValue;
        result = ZR_TRUE;
    }

    ZrCore_Gc_NativeCallUnpin(state->global, &fieldNamePin);
    ZrCore_Gc_NativeCallUnpin(state->global, &fieldInfoPin);
    return result;
}

static TZrBool reflection_field_value_read_field_info_identity(
        SZrState *state,
        SZrObject *fieldInfo,
        SZrMetadataRuntime **outRuntime,
        TZrMetadataToken *outFieldToken) {
    SZrTypeValue runtimeValue;
    SZrTypeValue tokenValue;

    if (outRuntime != ZR_NULL) {
        *outRuntime = ZR_NULL;
    }
    if (outFieldToken != ZR_NULL) {
        *outFieldToken = 0u;
    }

    if (state == ZR_NULL || fieldInfo == ZR_NULL || outRuntime == ZR_NULL || outFieldToken == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reflection_field_value_read_field_info_object_field(state, fieldInfo, "metadataRuntime", &runtimeValue) ||
        runtimeValue.type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        runtimeValue.value.nativeObject.nativePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reflection_field_value_read_field_info_object_field(state, fieldInfo, "metadataToken", &tokenValue) ||
        !ZR_VALUE_IS_TYPE_INT(tokenValue.type)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(tokenValue.type)) {
        if (tokenValue.value.nativeObject.nativeUInt64 > (TZrUInt64)UINT32_MAX) {
            return ZR_FALSE;
        }
        *outFieldToken = (TZrMetadataToken)tokenValue.value.nativeObject.nativeUInt64;
    } else {
        if (tokenValue.value.nativeObject.nativeInt64 < 0 ||
            tokenValue.value.nativeObject.nativeInt64 > (TZrInt64)UINT32_MAX) {
            return ZR_FALSE;
        }
        *outFieldToken = (TZrMetadataToken)tokenValue.value.nativeObject.nativeInt64;
    }

    *outRuntime = (SZrMetadataRuntime *)runtimeValue.value.nativeObject.nativePointer;
    return ZR_TRUE;
}

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        SZrTypeValue *outValue) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    const TZrByte *fieldAddress;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;
    EZrValueType primitiveValueType;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        outValue == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout)) {
        return ZR_FALSE;
    }

    fieldAddress = ((const TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    if ((fieldLayout->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) != 0u) {
        if (fieldLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue)) {
            return ZR_FALSE;
        }
        ZrCore_Value_Copy(state, outValue, (const SZrTypeValue *)fieldAddress);
        return ZR_TRUE;
    }

    if (!reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode)) {
        return ZR_FALSE;
    }

    if (fieldTypeNode.node == ZR_METADATA_SIGNATURE_NODE_PRIMITIVE) {
        if (fieldTypeNode.payload0 >= (TZrUInt32)ZR_VALUE_TYPE_ENUM_MAX) {
            return ZR_FALSE;
        }
        primitiveValueType = (EZrValueType)fieldTypeNode.payload0;
        return ZrCore_ReflectionFieldValue_LoadPrimitive(state,
                                                         fieldLayout,
                                                         primitiveValueType,
                                                         fieldAddress,
                                                         outValue);
    }

    if (!reflection_field_value_can_read_inline_borrowed_view(&resolved, fieldLayout, &fieldTypeNode)) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsNativePointer(state, outValue, (TZrPtr)fieldAddress);
    return ZR_TRUE;
}

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectValue(
        SZrState *state,
        SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        SZrTypeValue *outValue) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                     runtime,
                                                     fieldToken,
                                                     inlineStorage,
                                                     inlineStorageByteSize,
                                                     outValue);
}

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenNestedValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        SZrTypeValue *outValue) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    const TZrByte *fieldAddress;
    const SZrTypeLayoutField *nestedField;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        outValue == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout) ||
        !reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode) ||
        !reflection_field_value_can_read_inline_borrowed_view(&resolved, fieldLayout, &fieldTypeNode) ||
        resolved.fieldTypeLayout->fields == ZR_NULL ||
        nestedFieldIndex >= resolved.fieldTypeLayout->fieldCount) {
        return ZR_FALSE;
    }

    fieldAddress = ((const TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    nestedField = resolved.fieldTypeLayout->fields + nestedFieldIndex;
    return ZrCore_ReflectionFieldValue_ReadNestedLayoutField(state,
                                                             resolved.fieldTypeLayout,
                                                             nestedField,
                                                             fieldAddress,
                                                             outValue);
}

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectNestedValue(
        SZrState *state,
        SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        SZrTypeValue *outValue) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_ReadFieldInfoTokenNestedValue(state,
                                                           runtime,
                                                           fieldToken,
                                                           inlineStorage,
                                                           inlineStorageByteSize,
                                                           nestedFieldIndex,
                                                           outValue);
}

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenNestedPathValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        SZrTypeValue *outValue) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    const TZrByte *fieldAddress;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        outValue == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout) ||
        !reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode) ||
        !reflection_field_value_can_read_inline_borrowed_view(&resolved, fieldLayout, &fieldTypeNode)) {
        return ZR_FALSE;
    }

    fieldAddress = ((const TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    return ZrCore_ReflectionFieldValue_ReadNestedLayoutPath(state,
                                                            runtime,
                                                            resolved.fieldTypeLayout,
                                                            fieldAddress,
                                                            nestedFieldIndices,
                                                            nestedFieldIndexCount,
                                                            outValue);
}

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue(
        SZrState *state,
        SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        SZrTypeValue *outValue) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_ReadFieldInfoTokenNestedPathValue(state,
                                                               runtime,
                                                               fieldToken,
                                                               inlineStorage,
                                                               inlineStorageByteSize,
                                                               nestedFieldIndices,
                                                               nestedFieldIndexCount,
                                                               outValue);
}

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoTokenNestedPathPrimitiveValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        SZrTypeValue *outValue) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    const TZrByte *fieldAddress;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        primitiveValueType >= (TZrUInt32)ZR_VALUE_TYPE_ENUM_MAX ||
        outValue == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout) ||
        !reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode) ||
        !reflection_field_value_can_read_inline_borrowed_view(&resolved, fieldLayout, &fieldTypeNode)) {
        return ZR_FALSE;
    }

    fieldAddress = ((const TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    return ZrCore_ReflectionFieldValue_ReadNestedLayoutPrimitivePath(
            state,
            runtime,
            resolved.fieldTypeLayout,
            fieldAddress,
            nestedFieldIndices,
            nestedFieldIndexCount,
            (EZrValueType)primitiveValueType,
            outValue);
}

ZR_CORE_API TZrBool ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
        SZrState *state,
        SZrObject *fieldInfo,
        const void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        SZrTypeValue *outValue) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_ReadFieldInfoTokenNestedPathPrimitiveValue(
            state,
            runtime,
            fieldToken,
            inlineStorage,
            inlineStorageByteSize,
            nestedFieldIndices,
            nestedFieldIndexCount,
            primitiveValueType,
            outValue);
}

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenNestedPathValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        const SZrTypeValue *value) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    TZrByte *fieldAddress;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        value == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout) ||
        !reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode) ||
        !reflection_field_value_can_read_inline_borrowed_view(&resolved, fieldLayout, &fieldTypeNode)) {
        return ZR_FALSE;
    }

    fieldAddress = ((TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    return ZrCore_ReflectionFieldValue_WriteNestedLayoutPath(state,
                                                             runtime,
                                                             resolved.fieldTypeLayout,
                                                             fieldAddress,
                                                             nestedFieldIndices,
                                                             nestedFieldIndexCount,
                                                             value);
}

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue(
        SZrState *state,
        SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        const SZrTypeValue *value) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (inlineStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_WriteFieldInfoTokenNestedPathValue(state,
                                                                runtime,
                                                                fieldToken,
                                                                inlineStorage,
                                                                inlineStorageByteSize,
                                                                nestedFieldIndices,
                                                                nestedFieldIndexCount,
                                                                value);
}

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenNestedPathPrimitiveValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        const SZrTypeValue *value) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    TZrByte *fieldAddress;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        primitiveValueType >= (TZrUInt32)ZR_VALUE_TYPE_ENUM_MAX ||
        value == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout) ||
        !reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode) ||
        !reflection_field_value_can_read_inline_borrowed_view(&resolved, fieldLayout, &fieldTypeNode)) {
        return ZR_FALSE;
    }

    fieldAddress = ((TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    return ZrCore_ReflectionFieldValue_WriteNestedLayoutPrimitivePath(
            state,
            runtime,
            resolved.fieldTypeLayout,
            fieldAddress,
            nestedFieldIndices,
            nestedFieldIndexCount,
            (EZrValueType)primitiveValueType,
            value);
}

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue(
        SZrState *state,
        SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        TZrUInt32 primitiveValueType,
        const SZrTypeValue *value) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (inlineStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        primitiveValueType >= (TZrUInt32)ZR_VALUE_TYPE_ENUM_MAX ||
        value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_WriteFieldInfoTokenNestedPathPrimitiveValue(
            state,
            runtime,
            fieldToken,
            inlineStorage,
            inlineStorageByteSize,
            nestedFieldIndices,
            nestedFieldIndexCount,
            primitiveValueType,
            value);
}

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenNestedValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        const SZrTypeValue *value) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    TZrByte *fieldAddress;
    const SZrTypeLayoutField *nestedField;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        value == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout) ||
        !reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode) ||
        !reflection_field_value_can_read_inline_borrowed_view(&resolved, fieldLayout, &fieldTypeNode) ||
        resolved.fieldTypeLayout->fields == ZR_NULL ||
        nestedFieldIndex >= resolved.fieldTypeLayout->fieldCount) {
        return ZR_FALSE;
    }

    fieldAddress = ((TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    nestedField = resolved.fieldTypeLayout->fields + nestedFieldIndex;
    return ZrCore_ReflectionFieldValue_WriteNestedLayoutField(state,
                                                              resolved.fieldTypeLayout,
                                                              nestedField,
                                                              fieldAddress,
                                                              value);
}

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectNestedValue(
        SZrState *state,
        SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        TZrUInt32 nestedFieldIndex,
        const SZrTypeValue *value) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (inlineStorage == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_WriteFieldInfoTokenNestedValue(state,
                                                            runtime,
                                                            fieldToken,
                                                            inlineStorage,
                                                            inlineStorageByteSize,
                                                            nestedFieldIndex,
                                                            value);
}

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoObjectValue(
        SZrState *state,
        SZrObject *fieldInfo,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const SZrTypeValue *value) {
    SZrMetadataRuntime *runtime;
    TZrMetadataToken fieldToken;

    if (inlineStorage == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reflection_field_value_read_field_info_identity(state, fieldInfo, &runtime, &fieldToken)) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                      runtime,
                                                      fieldToken,
                                                      inlineStorage,
                                                      inlineStorageByteSize,
                                                      value);
}

ZR_CORE_API TZrBool ZrCore_Reflection_WriteFieldInfoTokenValue(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldToken,
        void *inlineStorage,
        TZrUInt32 inlineStorageByteSize,
        const SZrTypeValue *value) {
    SZrReflectionResolvedToken resolved;
    const SZrTypeLayoutField *fieldLayout;
    TZrByte *fieldAddress;
    SZrMetadataRuntimeSignatureTypeNodeView fieldTypeNode;
    EZrValueType primitiveValueType;

    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        value == ZR_NULL ||
        !reflection_field_value_resolve_field_layout(runtime,
                                                     fieldToken,
                                                     inlineStorageByteSize,
                                                     &resolved,
                                                     &fieldLayout)) {
        return ZR_FALSE;
    }

    fieldAddress = ((TZrByte *)inlineStorage) + fieldLayout->byteOffset;
    if ((fieldLayout->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) != 0u) {
        if (fieldLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue)) {
            return ZR_FALSE;
        }
        ZrCore_Value_Copy(state, (SZrTypeValue *)fieldAddress, value);
        return ZR_TRUE;
    }

    if (!reflection_field_value_read_field_type_node(runtime, resolved.token, &fieldTypeNode)) {
        return ZR_FALSE;
    }

    if (fieldTypeNode.node == ZR_METADATA_SIGNATURE_NODE_PRIMITIVE) {
        if (fieldTypeNode.payload0 >= (TZrUInt32)ZR_VALUE_TYPE_ENUM_MAX) {
            return ZR_FALSE;
        }
        primitiveValueType = (EZrValueType)fieldTypeNode.payload0;
        return ZrCore_ReflectionFieldValue_StorePrimitive(fieldLayout,
                                                          primitiveValueType,
                                                          fieldAddress,
                                                          value);
    }

    if (!reflection_field_value_can_write_inline_borrowed_source(&resolved, fieldLayout, &fieldTypeNode, value)) {
        return ZR_FALSE;
    }
    return ZrCore_TypeLayout_CopyInline(state,
                                        resolved.fieldTypeLayout,
                                        fieldAddress,
                                        value->value.nativeObject.nativePointer);
}
