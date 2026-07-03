#ifndef ZR_VM_CORE_REFLECTION_FIELD_VALUE_NESTED_H
#define ZR_VM_CORE_REFLECTION_FIELD_VALUE_NESTED_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

struct SZrMetadataRuntime;
struct SZrState;
struct SZrTypeLayout;
struct SZrTypeLayoutField;
struct SZrTypeValue;

TZrBool ZrCore_ReflectionFieldValue_ReadNestedLayoutField(
        struct SZrState *state,
        const struct SZrTypeLayout *layout,
        const struct SZrTypeLayoutField *field,
        const TZrByte *inlineStorage,
        struct SZrTypeValue *outValue);

TZrBool ZrCore_ReflectionFieldValue_WriteNestedLayoutField(
        struct SZrState *state,
        const struct SZrTypeLayout *layout,
        const struct SZrTypeLayoutField *field,
        TZrByte *inlineStorage,
        const struct SZrTypeValue *value);

TZrBool ZrCore_ReflectionFieldValue_ReadNestedLayoutPath(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        const struct SZrTypeLayout *layout,
        const TZrByte *inlineStorage,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        struct SZrTypeValue *outValue);

TZrBool ZrCore_ReflectionFieldValue_ReadNestedLayoutPrimitivePath(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        const struct SZrTypeLayout *layout,
        const TZrByte *inlineStorage,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        EZrValueType primitiveValueType,
        struct SZrTypeValue *outValue);

TZrBool ZrCore_ReflectionFieldValue_WriteNestedLayoutPath(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        const struct SZrTypeLayout *layout,
        TZrByte *inlineStorage,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        const struct SZrTypeValue *value);

TZrBool ZrCore_ReflectionFieldValue_WriteNestedLayoutPrimitivePath(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        const struct SZrTypeLayout *layout,
        TZrByte *inlineStorage,
        const TZrUInt32 *nestedFieldIndices,
        TZrUInt32 nestedFieldIndexCount,
        EZrValueType primitiveValueType,
        const struct SZrTypeValue *value);

#endif
