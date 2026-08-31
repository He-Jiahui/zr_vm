//
// Created by Codex on 2026/7/20.
//

#include "zr_vm_core/object.h"

#include <stdint.h>

#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/type_layout.h"

static const SZrTypeLayout *object_inline_array_resolve_layout(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 typeLayoutId) {
    const SZrTypeLayout *layout;

    if (state == ZR_NULL || function == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_NULL;
    }
    layout = ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(
            function, typeLayoutId);
    if (layout == ZR_NULL) {
        layout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(
                function, typeLayoutId, state);
    }
    return layout != ZR_NULL && ZrCore_TypeLayout_Validate(layout)
                   ? layout
                   : ZR_NULL;
}

static SZrFunction *object_inline_array_refresh_layout_function(
        SZrObject *array) {
    SZrFunction *function;
    SZrRawObject *rawFunction;
    SZrRawObject *forwardedFunction;

    if (array == ZR_NULL || array->inlineArrayLayoutFunction == ZR_NULL) {
        return ZR_NULL;
    }
    function = array->inlineArrayLayoutFunction;
    rawFunction = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
    forwardedFunction = (SZrRawObject *)rawFunction->garbageCollectMark.forwardingAddress;
    if (forwardedFunction != ZR_NULL) {
        function = ZR_CAST_FUNCTION(ZR_NULL, forwardedFunction);
        array->inlineArrayLayoutFunction = function;
    }
    return function;
}

static const SZrTypeLayout *object_inline_array_resolve_recorded_layout(
        SZrState *state,
        SZrObject *array,
        SZrFunction **outFunction) {
    SZrFunction *function = object_inline_array_refresh_layout_function(array);
    const SZrTypeLayout *layout = object_inline_array_resolve_layout(
            state,
            function,
            array != ZR_NULL ? array->inlineArrayElementLayoutId :
                               ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE);

    if (outFunction != ZR_NULL) {
        *outFunction = function;
    }
    if (array == ZR_NULL || layout == ZR_NULL ||
        layout->byteSize != array->inlineArrayElementByteSize ||
        layout->layoutHash != array->inlineArrayElementLayoutHash) {
        return ZR_NULL;
    }
    return layout;
}

static TZrBool object_inline_array_checked_add(
        TZrSize left,
        TZrSize right,
        TZrSize *outValue) {
    if (outValue == ZR_NULL || left > ZR_MAX_SIZE - right) {
        return ZR_FALSE;
    }
    *outValue = left + right;
    return ZR_TRUE;
}

static TZrBool object_inline_array_checked_multiply(
        TZrSize left,
        TZrSize right,
        TZrSize *outValue) {
    if (outValue == ZR_NULL || (right != 0u && left > ZR_MAX_SIZE / right)) {
        return ZR_FALSE;
    }
    *outValue = left * right;
    return ZR_TRUE;
}

SZrObject *ZrCore_Object_NewInlineArray(
        SZrState *state,
        SZrFunction *layoutFunction,
        TZrUInt32 elementTypeLayoutId,
        TZrUInt32 length) {
    const SZrTypeLayout *elementLayout;
    SZrObject *array;
    TZrSize elementBytes;
    TZrSize allocationBytes;
    TZrSize alignmentSlack;
    uintptr_t unalignedAddress;
    uintptr_t alignedAddress;
    TZrSize elementOffset;
    SZrTypeLayoutRegistryView registry;
    TZrBool hasRegistry;

    elementLayout = object_inline_array_resolve_layout(
            state, layoutFunction, elementTypeLayoutId);
    if (elementLayout == ZR_NULL || elementLayout->byteSize == 0u ||
        elementLayout->byteAlign == 0u ||
        !object_inline_array_checked_multiply(
                (TZrSize)elementLayout->byteSize,
                (TZrSize)length,
                &elementBytes)) {
        return ZR_NULL;
    }

    alignmentSlack = (TZrSize)elementLayout->byteAlign - 1u;
    if (!object_inline_array_checked_add(
                sizeof(SZrObject), alignmentSlack, &allocationBytes) ||
        !object_inline_array_checked_add(
                allocationBytes, elementBytes, &allocationBytes)) {
        return ZR_NULL;
    }

    array = ZrCore_Object_NewCustomized(
            state, allocationBytes, ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Object_Init(state, array);

    unalignedAddress = (uintptr_t)((TZrByte *)array + sizeof(SZrObject));
    alignedAddress = unalignedAddress;
    if (elementLayout->byteAlign > 1u) {
        uintptr_t remainder = alignedAddress % elementLayout->byteAlign;
        if (remainder != 0u) {
            alignedAddress += elementLayout->byteAlign - remainder;
        }
    }
    elementOffset = (TZrSize)(alignedAddress - (uintptr_t)array);
    if (elementOffset > UINT32_MAX ||
        elementBytes > allocationBytes - elementOffset) {
        return ZR_NULL;
    }

    array->inlineArrayLayoutFunction = layoutFunction;
    array->inlineArrayElementLayoutHash = elementLayout->layoutHash;
    array->inlineArrayObjectByteSize = allocationBytes;
    array->inlineArrayElementLayoutId = elementTypeLayoutId;
    array->inlineArrayElementByteOffset = (TZrUInt32)elementOffset;
    array->inlineArrayElementByteSize = elementLayout->byteSize;
    array->inlineArrayLength = 0u;
    hasRegistry = ZrCore_MetadataRuntime_GetFunctionTypeLayoutRegistry(
            layoutFunction, &registry);

    for (TZrUInt32 index = 0u; index < length; index++) {
        TZrByte *elementAddress = (TZrByte *)array + elementOffset +
                                 (TZrSize)index * elementLayout->byteSize;
        if (!(hasRegistry
                      ? ZrCore_TypeLayout_InitializeStorageWithRegistry(
                                state,
                                elementLayout,
                                &registry,
                                elementAddress)
                      : ZrCore_Function_InitInlineStorage(
                                state,
                                layoutFunction,
                                elementTypeLayoutId,
                                elementAddress,
                                elementLayout->byteSize))) {
            return ZR_NULL;
        }
        array->inlineArrayLength = index + 1u;
    }
    return array;
}

TZrBool ZrCore_Object_TryGetInlineArrayElementOffset(
        SZrState *state,
        const SZrObject *array,
        const SZrFunction *expectedLayoutFunction,
        TZrUInt32 expectedTypeLayoutId,
        TZrInt64 index,
        TZrUInt32 *outByteOffset) {
    const SZrTypeLayout *expectedLayout;
    TZrSize relativeOffset;
    TZrSize endOffset;

    if (outByteOffset != ZR_NULL) {
        *outByteOffset = 0u;
    }
    if (state == ZR_NULL || array == ZR_NULL || outByteOffset == ZR_NULL ||
        array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY ||
        array->inlineArrayElementByteSize == 0u ||
        array->inlineArrayObjectByteSize < sizeof(SZrObject) ||
        index < 0 || (TZrUInt64)index >= (TZrUInt64)array->inlineArrayLength) {
        return ZR_FALSE;
    }

    expectedLayout = object_inline_array_resolve_layout(
            state, expectedLayoutFunction, expectedTypeLayoutId);
    if (expectedLayout == ZR_NULL ||
        expectedLayout->byteSize != array->inlineArrayElementByteSize ||
        expectedLayout->layoutHash != array->inlineArrayElementLayoutHash ||
        !object_inline_array_checked_multiply(
                (TZrSize)index,
                array->inlineArrayElementByteSize,
                &relativeOffset) ||
        !object_inline_array_checked_add(
                array->inlineArrayElementByteOffset,
                relativeOffset,
                &relativeOffset) ||
        !object_inline_array_checked_add(
                relativeOffset,
                array->inlineArrayElementByteSize,
                &endOffset) ||
        endOffset > array->inlineArrayObjectByteSize ||
        relativeOffset > UINT32_MAX) {
        return ZR_FALSE;
    }

    *outByteOffset = (TZrUInt32)relativeOffset;
    return ZR_TRUE;
}

TZrBool ZrCore_Object_VisitInlineArrayGcValues(
        SZrState *state,
        SZrObject *array,
        FZrTypeLayoutGcValueVisitor visitor,
        TZrPtr userData) {
    SZrFunction *function;
    const SZrTypeLayout *layout;
    SZrTypeLayoutRegistryView registry;
    TZrBool hasRegistry;

    if (state == ZR_NULL || array == ZR_NULL || visitor == ZR_NULL ||
        array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY ||
        array->inlineArrayElementByteSize == 0u) {
        return ZR_FALSE;
    }
    layout = object_inline_array_resolve_recorded_layout(
            state, array, &function);
    if (layout == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }
    {
        TZrSize elementBytes;
        TZrSize endOffset;

        if (!object_inline_array_checked_multiply(
                    (TZrSize)array->inlineArrayLength,
                    (TZrSize)layout->byteSize,
                    &elementBytes) ||
            !object_inline_array_checked_add(
                    (TZrSize)array->inlineArrayElementByteOffset,
                    elementBytes,
                    &endOffset) ||
            endOffset > array->inlineArrayObjectByteSize) {
            return ZR_FALSE;
        }
    }
    if (ZrCore_TypeLayout_CanSkipGcScan(layout)) {
        return ZR_TRUE;
    }
    hasRegistry = ZrCore_MetadataRuntime_GetFunctionTypeLayoutRegistry(
            function, &registry);
    for (TZrUInt32 index = 0u; index < array->inlineArrayLength; index++) {
        TZrSize byteOffset = (TZrSize)array->inlineArrayElementByteOffset +
                             (TZrSize)index * layout->byteSize;
        TZrPtr storage;

        if (byteOffset > array->inlineArrayObjectByteSize ||
            layout->byteSize > array->inlineArrayObjectByteSize - byteOffset) {
            return ZR_FALSE;
        }
        storage = (TZrByte *)array + byteOffset;
        if (hasRegistry) {
            if (!ZrCore_TypeLayout_VisitGcValuesWithRegistry(
                        state, layout, &registry, storage, visitor, userData)) {
                return ZR_FALSE;
            }
        } else {
            ZrCore_TypeLayout_VisitGcValues(
                    state, layout, storage, visitor, userData);
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Object_DropInlineArrayElements(
        SZrState *state,
        SZrObject *array) {
    SZrFunction *function;
    const SZrTypeLayout *layout;
    SZrTypeLayoutRegistryView registry;
    TZrBool hasRegistry;

    if (state == ZR_NULL || array == ZR_NULL ||
        array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_FALSE;
    }
    if (array->inlineArrayElementByteSize == 0u ||
        array->inlineArrayLength == 0u) {
        return ZR_TRUE;
    }
    layout = object_inline_array_resolve_recorded_layout(
            state, array, &function);
    if (layout == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }
    hasRegistry = ZrCore_MetadataRuntime_GetFunctionTypeLayoutRegistry(
            function, &registry);
    while (array->inlineArrayLength > 0u) {
        TZrUInt32 index = array->inlineArrayLength - 1u;
        TZrSize byteOffset = (TZrSize)array->inlineArrayElementByteOffset +
                             (TZrSize)index * layout->byteSize;
        TZrPtr storage;
        TZrBool dropped;

        if (byteOffset > array->inlineArrayObjectByteSize ||
            layout->byteSize > array->inlineArrayObjectByteSize - byteOffset) {
            return ZR_FALSE;
        }
        storage = (TZrByte *)array + byteOffset;
        dropped = hasRegistry
                          ? ZrCore_TypeLayout_DropInlineWithRegistry(
                                    state, layout, &registry, storage)
                          : (ZrCore_TypeLayout_DropInline(
                                     state, layout, storage),
                             ZR_TRUE);
        if (!dropped) {
            return ZR_FALSE;
        }
        array->inlineArrayLength = index;
    }
    return ZR_TRUE;
}
