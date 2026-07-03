//
// Nested FieldInfo inline-layout traversal helpers.
//

#include "reflection_field_value_nested.h"

#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

#include "reflection_field_value_primitive.h"

static TZrBool reflection_field_value_field_fits_layout(const SZrTypeLayout *layout,
                                                        const SZrTypeLayoutField *field) {
    return layout != ZR_NULL &&
           field != ZR_NULL &&
           field->byteOffset <= layout->byteSize &&
           field->byteSize <= layout->byteSize - field->byteOffset;
}

TZrBool ZrCore_ReflectionFieldValue_ReadNestedLayoutField(
        SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutField *field,
        const TZrByte *inlineStorage,
        SZrTypeValue *outValue) {
    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        outValue == ZR_NULL ||
        !reflection_field_value_field_fits_layout(layout, field)) {
        return ZR_FALSE;
    }

    if ((field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) == 0u ||
        field->byteSize < (TZrUInt32)sizeof(SZrTypeValue)) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, outValue, (const SZrTypeValue *)(const void *)(inlineStorage + field->byteOffset));
    return ZR_TRUE;
}

TZrBool ZrCore_ReflectionFieldValue_WriteNestedLayoutField(
        SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutField *field,
        TZrByte *inlineStorage,
        const SZrTypeValue *value) {
    if (state == ZR_NULL ||
        inlineStorage == ZR_NULL ||
        value == ZR_NULL ||
        !reflection_field_value_field_fits_layout(layout, field)) {
        return ZR_FALSE;
    }

    if ((field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) == 0u ||
        field->byteSize < (TZrUInt32)sizeof(SZrTypeValue)) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, (SZrTypeValue *)(void *)(inlineStorage + field->byteOffset), value);
    return ZR_TRUE;
}

static const SZrTypeLayout *reflection_field_value_resolve_nested_child_layout(
        SZrMetadataRuntime *runtime,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutField *field) {
    TZrUInt32 unsupportedFieldFlags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                                     ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                                     ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    const SZrTypeLayout *childLayout;

    if (runtime == ZR_NULL ||
        !reflection_field_value_field_fits_layout(layout, field) ||
        (field->flags & unsupportedFieldFlags) != 0u ||
        field->typeLayoutIndex == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        field->byteSize == 0u) {
        return ZR_NULL;
    }

    childLayout = ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, field->typeLayoutIndex);
    if (childLayout == ZR_NULL ||
        childLayout->byteSize != field->byteSize ||
        (childLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT &&
         childLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION)) {
        return ZR_NULL;
    }

    return childLayout;
}

TZrBool ZrCore_ReflectionFieldValue_ReadNestedLayoutPath(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        const SZrTypeLayout *layout,
        const TZrByte *inlineStorage,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        SZrTypeValue *outValue) {
    const SZrTypeLayout *currentLayout = layout;
    const TZrByte *currentStorage = inlineStorage;
    TZrUInt32 index;

    if (state == ZR_NULL ||
        runtime == ZR_NULL ||
        currentLayout == ZR_NULL ||
        currentStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0u; index < nestedFieldIndexCount; index++) {
        TZrUInt32 nestedFieldIndex = nestedFieldIndices[index];
        const SZrTypeLayoutField *nestedField;
        const SZrTypeLayout *childLayout;

        if (currentLayout->fields == ZR_NULL ||
            nestedFieldIndex >= currentLayout->fieldCount) {
            return ZR_FALSE;
        }

        nestedField = currentLayout->fields + nestedFieldIndex;
        if (index + 1u == nestedFieldIndexCount) {
            return ZrCore_ReflectionFieldValue_ReadNestedLayoutField(state,
                                                                     currentLayout,
                                                                     nestedField,
                                                                     currentStorage,
                                                                     outValue);
        }

        childLayout = reflection_field_value_resolve_nested_child_layout(runtime,
                                                                         currentLayout,
                                                                         nestedField);
        if (childLayout == ZR_NULL) {
            return ZR_FALSE;
        }
        currentStorage = currentStorage + nestedField->byteOffset;
        currentLayout = childLayout;
    }

    return ZR_FALSE;
}

TZrBool ZrCore_ReflectionFieldValue_ReadNestedLayoutPrimitivePath(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        const SZrTypeLayout *layout,
        const TZrByte *inlineStorage,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        EZrValueType primitiveValueType,
        SZrTypeValue *outValue) {
    const SZrTypeLayout *currentLayout = layout;
    const TZrByte *currentStorage = inlineStorage;
    TZrUInt32 index;

    if (state == ZR_NULL ||
        runtime == ZR_NULL ||
        currentLayout == ZR_NULL ||
        currentStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0u; index < nestedFieldIndexCount; index++) {
        TZrUInt32 nestedFieldIndex = nestedFieldIndices[index];
        const SZrTypeLayoutField *nestedField;
        const SZrTypeLayout *childLayout;

        if (currentLayout->fields == ZR_NULL ||
            nestedFieldIndex >= currentLayout->fieldCount) {
            return ZR_FALSE;
        }

        nestedField = currentLayout->fields + nestedFieldIndex;
        if (index + 1u == nestedFieldIndexCount) {
            if (!reflection_field_value_field_fits_layout(currentLayout, nestedField) ||
                nestedField->typeLayoutIndex != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
                return ZR_FALSE;
            }
            return ZrCore_ReflectionFieldValue_LoadPrimitive(state,
                                                             nestedField,
                                                             primitiveValueType,
                                                             currentStorage + nestedField->byteOffset,
                                                             outValue);
        }

        childLayout = reflection_field_value_resolve_nested_child_layout(runtime,
                                                                         currentLayout,
                                                                         nestedField);
        if (childLayout == ZR_NULL) {
            return ZR_FALSE;
        }
        currentStorage = currentStorage + nestedField->byteOffset;
        currentLayout = childLayout;
    }

    return ZR_FALSE;
}

TZrBool ZrCore_ReflectionFieldValue_WriteNestedLayoutPath(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        const SZrTypeLayout *layout,
        TZrByte *inlineStorage,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        const SZrTypeValue *value) {
    const SZrTypeLayout *currentLayout = layout;
    TZrByte *currentStorage = inlineStorage;
    TZrUInt32 index;

    if (state == ZR_NULL ||
        runtime == ZR_NULL ||
        currentLayout == ZR_NULL ||
        currentStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        value == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0u; index < nestedFieldIndexCount; index++) {
        TZrUInt32 nestedFieldIndex = nestedFieldIndices[index];
        const SZrTypeLayoutField *nestedField;
        const SZrTypeLayout *childLayout;

        if (currentLayout->fields == ZR_NULL ||
            nestedFieldIndex >= currentLayout->fieldCount) {
            return ZR_FALSE;
        }

        nestedField = currentLayout->fields + nestedFieldIndex;
        if (index + 1u == nestedFieldIndexCount) {
            return ZrCore_ReflectionFieldValue_WriteNestedLayoutField(state,
                                                                      currentLayout,
                                                                      nestedField,
                                                                      currentStorage,
                                                                      value);
        }

        childLayout = reflection_field_value_resolve_nested_child_layout(runtime,
                                                                         currentLayout,
                                                                         nestedField);
        if (childLayout == ZR_NULL) {
            return ZR_FALSE;
        }
        currentStorage = currentStorage + nestedField->byteOffset;
        currentLayout = childLayout;
    }

    return ZR_FALSE;
}

TZrBool ZrCore_ReflectionFieldValue_WriteNestedLayoutPrimitivePath(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        const SZrTypeLayout *layout,
        TZrByte *inlineStorage,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        EZrValueType primitiveValueType,
        const SZrTypeValue *value) {
    const SZrTypeLayout *currentLayout = layout;
    TZrByte *currentStorage = inlineStorage;
    TZrUInt32 index;

    if (state == ZR_NULL ||
        runtime == ZR_NULL ||
        currentLayout == ZR_NULL ||
        currentStorage == ZR_NULL ||
        nestedFieldIndices == ZR_NULL ||
        nestedFieldIndexCount == 0u ||
        value == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0u; index < nestedFieldIndexCount; index++) {
        TZrUInt32 nestedFieldIndex = nestedFieldIndices[index];
        const SZrTypeLayoutField *nestedField;
        const SZrTypeLayout *childLayout;

        if (currentLayout->fields == ZR_NULL ||
            nestedFieldIndex >= currentLayout->fieldCount) {
            return ZR_FALSE;
        }

        nestedField = currentLayout->fields + nestedFieldIndex;
        if (index + 1u == nestedFieldIndexCount) {
            if (!reflection_field_value_field_fits_layout(currentLayout, nestedField) ||
                nestedField->typeLayoutIndex != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
                return ZR_FALSE;
            }
            return ZrCore_ReflectionFieldValue_StorePrimitive(nestedField,
                                                              primitiveValueType,
                                                              currentStorage + nestedField->byteOffset,
                                                              value);
        }

        childLayout = reflection_field_value_resolve_nested_child_layout(runtime,
                                                                         currentLayout,
                                                                         nestedField);
        if (childLayout == ZR_NULL) {
            return ZR_FALSE;
        }
        currentStorage = currentStorage + nestedField->byteOffset;
        currentLayout = childLayout;
    }

    return ZR_FALSE;
}
