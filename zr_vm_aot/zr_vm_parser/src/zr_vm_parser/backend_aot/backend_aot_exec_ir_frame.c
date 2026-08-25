#include "backend_aot_exec_ir_frame.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/type_layout.h"

static TZrBool backend_aot_exec_ir_is_power_of_two(TZrUInt32 value) {
    return (TZrBool)(value != 0u && (value & (value - 1u)) == 0u);
}

static TZrBool backend_aot_exec_ir_frame_storage_contains(TZrUInt32 frameByteSize,
                                                          TZrUInt32 byteOffset,
                                                          TZrUInt32 storageSize) {
    return (TZrBool)(byteOffset <= frameByteSize &&
                     storageSize <= frameByteSize - byteOffset);
}

static TZrBool backend_aot_exec_ir_typed_local_is_parameter_eligible(
        const SZrFunctionTypedLocalBinding *binding) {
    return (TZrBool)(binding != ZR_NULL &&
                     (binding->name != ZR_NULL ||
                      (binding->roleFlags &
                       ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) != 0u));
}

static TZrBool backend_aot_exec_ir_complete_frame_slot_is_parameter(
        const SZrFunction *function,
        TZrUInt32 stackSlot) {
    TZrUInt32 parameterBindingCount = 0u;

    if (function == ZR_NULL || function->parameterCount == 0u) {
        return ZR_FALSE;
    }
    if (function->typedLocalBindings == ZR_NULL ||
        function->typedLocalBindingLength == 0u) {
        return (TZrBool)(stackSlot < function->parameterCount);
    }

    for (TZrUInt32 index = 0u; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];

        if (!backend_aot_exec_ir_typed_local_is_parameter_eligible(binding)) {
            continue;
        }
        if (parameterBindingCount >= function->parameterCount) {
            break;
        }
        if (binding->stackSlot == stackSlot) {
            return ZR_TRUE;
        }
        parameterBindingCount++;
    }

    return ZR_FALSE;
}

static const SZrFunctionTypedLocalBinding *
backend_aot_exec_ir_find_parameter_binding_for_slot(
        const SZrFunction *function,
        TZrUInt32 stackSlot) {
    TZrUInt32 parameterBindingCount = 0u;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrUInt32 index = 0u;
         index < function->typedLocalBindingLength;
         index++) {
        const SZrFunctionTypedLocalBinding *binding =
                &function->typedLocalBindings[index];

        if (!backend_aot_exec_ir_typed_local_is_parameter_eligible(binding)) {
            continue;
        }
        if (parameterBindingCount >= function->parameterCount) {
            break;
        }
        if (binding->stackSlot == stackSlot) {
            return binding;
        }
        parameterBindingCount++;
    }
    return ZR_NULL;
}

static TZrBool backend_aot_exec_ir_parameter_passing_requires_borrowed_storage(
        TZrUInt32 passingRoleFlags) {
    return (TZrBool)(
            passingRoleFlags ==
                    ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_IN ||
            passingRoleFlags ==
                    ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_REF_READONLY ||
            passingRoleFlags ==
                    ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_SCOPED_REF_READONLY);
}

static const SZrFunctionFrameSlotLayout *
backend_aot_exec_ir_find_frame_layout_for_slot(
        const SZrFunction *function,
        TZrUInt32 stackSlot) {
    if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrUInt32 index = 0u;
         index < function->frameSlotLayoutLength;
         index++) {
        if (function->frameSlotLayouts[index].stackSlot == stackSlot) {
            return &function->frameSlotLayouts[index];
        }
    }
    return ZR_NULL;
}

static TZrBool backend_aot_exec_ir_validate_required_borrowed_parameter_rows(
        SZrState *state,
        const SZrFunction *function) {
    const TZrUInt16 requiredFlags =
            ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS;
    TZrUInt32 parameterBindingCount = 0u;

    if (state == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u;
         function->typedLocalBindings != ZR_NULL &&
         index < function->typedLocalBindingLength;
         index++) {
        const SZrFunctionTypedLocalBinding *binding =
                &function->typedLocalBindings[index];
        const SZrFunctionFrameSlotLayout *layout;
        const SZrTypeLayout *typeLayout;
        TZrUInt32 passingRoleFlags;

        if (!backend_aot_exec_ir_typed_local_is_parameter_eligible(binding)) {
            continue;
        }
        if (parameterBindingCount++ >= function->parameterCount) {
            break;
        }
        passingRoleFlags =
                binding->roleFlags &
                ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_MASK;
        if ((binding->roleFlags &
             ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) != 0u ||
            !backend_aot_exec_ir_parameter_passing_requires_borrowed_storage(
                    passingRoleFlags) ||
            binding->type.staticCType != ZR_STATIC_C_TYPE_STRUCT) {
            continue;
        }
        layout = backend_aot_exec_ir_find_frame_layout_for_slot(
                function, binding->stackSlot);
        if (binding->type.staticCTypeId ==
                    ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
            layout == ZR_NULL || layout->isParameter == 0u ||
            layout->slotKind !=
                    (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
            layout->typeLayoutId != binding->type.staticCTypeId ||
            (layout->reserved0 & requiredFlags) != requiredFlags) {
            return ZR_FALSE;
        }
        typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(
                function, binding->type.staticCTypeId, state);
        if (typeLayout == ZR_NULL ||
            !ZrCore_TypeLayout_Validate(typeLayout) ||
            typeLayout->cTypeId != binding->type.staticCTypeId ||
            (typeLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT &&
             typeLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_exec_ir_validate_parameter_bindings(
        const SZrFunction *function) {
    TZrUInt32 parameterBindingCount = 0u;
    TZrBool identityAvailabilityKnown = ZR_FALSE;
    TZrBool parametersHaveIdentity = ZR_FALSE;
    TZrBool passingFormAvailabilityKnown = ZR_FALSE;
    TZrBool explicitParametersHavePassingForm = ZR_FALSE;

    if (function == ZR_NULL ||
        (function->typedLocalBindingLength > 0u &&
         function->typedLocalBindings == ZR_NULL) ||
        (function->parameterMetadataCount > 0u &&
         function->parameterMetadata == ZR_NULL) ||
        function->parameterMetadataCount > function->parameterCount) {
        return ZR_FALSE;
    }
    for (TZrUInt32 metadataIndex = 0u;
         metadataIndex < function->parameterMetadataCount;
         metadataIndex++) {
        const TZrBool hasDefaultValue =
                function->parameterMetadata[metadataIndex].hasDefaultValue;

        if (hasDefaultValue != ZR_FALSE && hasDefaultValue != ZR_TRUE) {
            return ZR_FALSE;
        }
    }
    if (function->typedLocalBindingLength == 0u) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding =
                &function->typedLocalBindings[index];
        TZrBool hasSymbolIdentity;
        TZrBool hasTypeIdentity;
        TZrBool hasPlaceIdentity;
        TZrBool hasCompleteIdentity;
        TZrBool isReceiver;
        TZrBool hasPassingForm;
        TZrUInt32 passingRoleFlags;
        TZrUInt32 previousParameterCount = 0u;

        passingRoleFlags =
                binding->roleFlags &
                ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_MASK;
        if (!backend_aot_exec_ir_typed_local_is_parameter_eligible(binding)) {
            if (passingRoleFlags != 0u) {
                return ZR_FALSE;
            }
            continue;
        }
        if (parameterBindingCount >= function->parameterCount) {
            if ((binding->roleFlags &
                 ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) != 0u ||
                passingRoleFlags != 0u) {
                return ZR_FALSE;
            }
            continue;
        }

        isReceiver = (TZrBool)(
                (binding->roleFlags &
                 ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) != 0u);
        hasPassingForm = (TZrBool)(passingRoleFlags != 0u);
        if (!isReceiver) {
            if (passingFormAvailabilityKnown &&
                explicitParametersHavePassingForm != hasPassingForm) {
                return ZR_FALSE;
            }
            passingFormAvailabilityKnown = ZR_TRUE;
            explicitParametersHavePassingForm = hasPassingForm;
        }

        hasSymbolIdentity = (TZrBool)(binding->symbolId != 0u);
        hasTypeIdentity = (TZrBool)(binding->typeId != 0u);
        hasPlaceIdentity = (TZrBool)(binding->placeId != 0u);
        hasCompleteIdentity = (TZrBool)(hasSymbolIdentity &&
                                        hasTypeIdentity &&
                                        hasPlaceIdentity);
        if (binding->stackSlot >= function->stackSize ||
            (hasSymbolIdentity || hasTypeIdentity || hasPlaceIdentity) !=
                    hasCompleteIdentity) {
            return ZR_FALSE;
        }
        if (!isReceiver) {
            if (identityAvailabilityKnown &&
                parametersHaveIdentity != hasCompleteIdentity) {
                return ZR_FALSE;
            }
            identityAvailabilityKnown = ZR_TRUE;
            parametersHaveIdentity = hasCompleteIdentity;
        }

        for (TZrUInt32 previous = 0u; previous < index; previous++) {
            const SZrFunctionTypedLocalBinding *previousBinding =
                    &function->typedLocalBindings[previous];

            if (!backend_aot_exec_ir_typed_local_is_parameter_eligible(
                        previousBinding)) {
                continue;
            }
            if (previousParameterCount >= function->parameterCount) {
                break;
            }
            if (previousBinding->stackSlot == binding->stackSlot ||
                (hasCompleteIdentity &&
                 (previousBinding->symbolId == binding->symbolId ||
                  previousBinding->placeId == binding->placeId))) {
                return ZR_FALSE;
            }
            previousParameterCount++;
        }

        parameterBindingCount++;
    }

    return (TZrBool)(parameterBindingCount == function->parameterCount);
}

static TZrBool backend_aot_exec_ir_parameter_defaults_are_index_aligned(
        const SZrFunction *function) {
    if (function == ZR_NULL || function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount != function->parameterCount) {
        return ZR_FALSE;
    }

    for (TZrUInt32 bindingIndex = 0u;
         bindingIndex < function->typedLocalBindingLength;
         bindingIndex++) {
        if ((function->typedLocalBindings[bindingIndex].roleFlags &
             ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) != 0u) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static void backend_aot_exec_ir_project_parameter_default_declaration(
        const SZrFunction *function,
        TZrUInt32 parameterIndex,
        SZrAotExecIrParameterLayout *destination) {
    const SZrFunctionMetadataParameter *metadata;

    if (function == ZR_NULL || destination == ZR_NULL ||
        !backend_aot_exec_ir_parameter_defaults_are_index_aligned(function) ||
        parameterIndex >= function->parameterCount) {
        return;
    }

    metadata = &function->parameterMetadata[parameterIndex];
    if (metadata->hasDefaultValue != ZR_TRUE) {
        return;
    }

    destination->defaultDeclarationKnown = ZR_TRUE;
    destination->hasDeclaredDefault = ZR_TRUE;
}

static TZrBool backend_aot_exec_ir_validate_receiver_role(
        const SZrFunction *function) {
    const TZrUInt32 knownRoleFlags =
            ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER |
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_MASK;
    TZrUInt32 receiverCount = 0u;

    if (function == ZR_NULL ||
        (function->typedLocalBindingLength > 0u &&
         function->typedLocalBindings == ZR_NULL)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding =
                &function->typedLocalBindings[index];
        const TZrUInt32 passingRoleFlags =
                binding->roleFlags &
                ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_MASK;

        if ((binding->roleFlags & ~knownRoleFlags) != 0u ||
            (passingRoleFlags != 0u &&
             (passingRoleFlags & (passingRoleFlags - 1u)) != 0u) ||
            ((binding->roleFlags &
              ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) != 0u &&
             passingRoleFlags != 0u)) {
            return ZR_FALSE;
        }
        if ((binding->roleFlags & ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) == 0u) {
            continue;
        }

        receiverCount++;
        {
            const TZrBool hasSymbolIdentity = (TZrBool)(binding->symbolId != 0u);
            const TZrBool hasTypeIdentity = (TZrBool)(binding->typeId != 0u);
            const TZrBool hasPlaceIdentity = (TZrBool)(binding->placeId != 0u);
            const TZrBool hasAnyIdentity =
                    (TZrBool)(hasSymbolIdentity || hasTypeIdentity || hasPlaceIdentity);
            const TZrBool hasCompleteIdentity =
                    (TZrBool)(hasSymbolIdentity && hasTypeIdentity && hasPlaceIdentity);

            if (hasAnyIdentity && !hasCompleteIdentity) {
                return ZR_FALSE;
            }
        }
        if (receiverCount > 1u ||
            binding->stackSlot != 0u ||
            function->parameterCount == 0u) {
            return ZR_FALSE;
        }

        if (function->frameSlotLayouts != ZR_NULL) {
            for (TZrUInt32 layoutIndex = 0u;
                 layoutIndex < function->frameSlotLayoutLength;
                 layoutIndex++) {
                const SZrFunctionFrameSlotLayout *layout =
                        &function->frameSlotLayouts[layoutIndex];

                if (layout->stackSlot == binding->stackSlot &&
                    layout->isParameter == 0u) {
                    return ZR_FALSE;
                }
            }
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_exec_ir_validate_frame_layout(
        SZrState *state,
        const SZrFunction *function) {
    const TZrUInt16 knownFlags =
            ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_CONSTRUCTOR_INITIALIZATION_BITMAP |
            ZR_FUNCTION_FRAME_SLOT_FLAG_INLINE_RECEIVER_ARGUMENT |
            ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS;
    TZrBool hasCompleteSlotTable;
    TZrUInt32 parameterLayoutCount = 0u;

    if (state == ZR_NULL || function == ZR_NULL ||
        function->parameterCount > function->stackSize ||
        !backend_aot_exec_ir_validate_parameter_bindings(function) ||
        !backend_aot_exec_ir_validate_receiver_role(function)) {
        return ZR_FALSE;
    }
    if (function->frameSlotLayoutLength == 0u) {
        return (TZrBool)(
                function->frameByteSize == 0u &&
                function->frameByteAlign == 0u &&
                backend_aot_exec_ir_validate_required_borrowed_parameter_rows(
                        state, function));
    }
    if (function->frameSlotLayouts == ZR_NULL ||
        function->frameSlotLayoutLength > function->stackSize ||
        (TZrSize)function->frameSlotLayoutLength >
                ((TZrSize)-1) / sizeof(SZrAotExecIrFrameSlotLayout) ||
        function->frameByteSize == 0u ||
        !backend_aot_exec_ir_is_power_of_two(function->frameByteAlign)) {
        return ZR_FALSE;
    }
    hasCompleteSlotTable = (TZrBool)(
            function->frameSlotLayoutLength == function->stackSize);

    for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *layout = &function->frameSlotLayouts[index];
        const SZrFunctionTypedLocalBinding *parameterBinding = ZR_NULL;
        const SZrTypeLayout *typeLayout = ZR_NULL;
        const TZrUInt16 flags = layout->reserved0;
        TZrUInt32 storageSize = layout->byteSize;
        TZrUInt32 storageAlign = layout->byteAlign;

        if ((flags & (TZrUInt16)~knownFlags) != 0u ||
            layout->stackSlot >= function->stackSize ||
            layout->byteSize == 0u ||
            !backend_aot_exec_ir_is_power_of_two(layout->byteAlign) ||
            layout->slotKind > (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
            layout->isParameter > 1u ||
            (layout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
             layout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) ||
            (layout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&
             layout->typeLayoutId != ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE)) {
            return ZR_FALSE;
        }
        if (hasCompleteSlotTable &&
            layout->isParameter !=
                    backend_aot_exec_ir_complete_frame_slot_is_parameter(
                            function, layout->stackSlot)) {
            return ZR_FALSE;
        }
        if (layout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
            typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(
                    function, layout->typeLayoutId, state);
            if (typeLayout == ZR_NULL ||
                !ZrCore_TypeLayout_Validate(typeLayout) ||
                typeLayout->cTypeId != layout->typeLayoutId ||
                (typeLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT &&
                 typeLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION) ||
                typeLayout->byteSize != layout->byteSize ||
                typeLayout->byteAlign != layout->byteAlign) {
                return ZR_FALSE;
            }
        }
        if (layout->isParameter != 0u) {
            parameterLayoutCount++;
            if (parameterLayoutCount > function->parameterCount) {
                return ZR_FALSE;
            }
            parameterBinding =
                    backend_aot_exec_ir_find_parameter_binding_for_slot(
                            function, layout->stackSlot);
            if (parameterBinding != ZR_NULL &&
                (parameterBinding->roleFlags &
                 ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) == 0u) {
                const TZrUInt32 passingRoleFlags =
                        parameterBinding->roleFlags &
                        ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_MASK;
                const TZrBool hasBorrowedStorage = (TZrBool)(
                        (flags & ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS) !=
                        0u);
                const TZrBool requiresBorrowedStorage =
                        backend_aot_exec_ir_parameter_passing_requires_borrowed_storage(
                                passingRoleFlags);

                if (requiresBorrowedStorage &&
                    parameterBinding->type.staticCType ==
                            ZR_STATIC_C_TYPE_STRUCT) {
                    const SZrTypeLayout *bindingTypeLayout;

                    if (parameterBinding->type.staticCTypeId ==
                                ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
                        layout->slotKind !=
                                (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
                        layout->typeLayoutId !=
                                parameterBinding->type.staticCTypeId ||
                        !hasBorrowedStorage) {
                        return ZR_FALSE;
                    }
                    bindingTypeLayout =
                            ZrCore_Function_ResolvePrototypeFrameTypeLayout(
                                    function,
                                    parameterBinding->type.staticCTypeId,
                                    state);
                    if (bindingTypeLayout == ZR_NULL ||
                        !ZrCore_TypeLayout_Validate(bindingTypeLayout) ||
                        (bindingTypeLayout->kind !=
                                 (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT &&
                         bindingTypeLayout->kind !=
                                 (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION)) {
                        return ZR_FALSE;
                    }
                } else if (passingRoleFlags != 0u && hasBorrowedStorage) {
                    return ZR_FALSE;
                }
            }
        }
        for (TZrUInt32 previous = 0u; previous < index; previous++) {
            if (function->frameSlotLayouts[previous].stackSlot == layout->stackSlot) {
                return ZR_FALSE;
            }
        }
        if ((flags & ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS) == 0u &&
            (flags & (ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
                      ZR_FUNCTION_FRAME_SLOT_FLAG_INLINE_RECEIVER_ARGUMENT |
                      ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS)) != 0u) {
            return ZR_FALSE;
        }
        if (flags != 0u &&
            layout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
            return ZR_FALSE;
        }
        if ((flags & ZR_FUNCTION_FRAME_SLOT_FLAG_CONSTRUCTOR_INITIALIZATION_BITMAP) != 0u &&
            (layout->isParameter == 0u ||
             layout->stackSlot != 0u ||
             (flags & (ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
                       ZR_FUNCTION_FRAME_SLOT_FLAG_INLINE_RECEIVER_ARGUMENT |
                       ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS)) != 0u)) {
            return ZR_FALSE;
        }
        if ((flags & ZR_FUNCTION_FRAME_SLOT_FLAG_CONSTRUCTOR_INITIALIZATION_BITMAP) != 0u) {
            TZrUInt32 bitmapByteOffset;
            TZrUInt32 initializedFieldWordCount;

            if (!ZrCore_Function_GetInlineConstructorInitializedFieldBitmapLayout(
                        state,
                        function,
                        &bitmapByteOffset,
                        &initializedFieldWordCount)) {
                return ZR_FALSE;
            }
        }
        if ((flags & ZR_FUNCTION_FRAME_SLOT_FLAG_INLINE_RECEIVER_ARGUMENT) != 0u &&
            (flags & (ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
                      ZR_FUNCTION_FRAME_SLOT_FLAG_CONSTRUCTOR_INITIALIZATION_BITMAP |
                      ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS)) != 0u) {
            return ZR_FALSE;
        }
        if ((flags & ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS) != 0u) {
            const TZrUInt16 required =
                    ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
                    ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
                    ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS;

            if ((flags & required) != required || layout->isParameter == 0u ||
                layout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
                (flags & (ZR_FUNCTION_FRAME_SLOT_FLAG_INLINE_RECEIVER_ARGUMENT |
                          ZR_FUNCTION_FRAME_SLOT_FLAG_CONSTRUCTOR_INITIALIZATION_BITMAP)) != 0u) {
                return ZR_FALSE;
            }
            storageSize = (TZrUInt32)sizeof(SZrFunctionFrameBorrowedAliasBinding);
            storageAlign = (TZrUInt32)_Alignof(SZrFunctionFrameBorrowedAliasBinding);
        } else if ((flags & ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS) != 0u) {
            if ((flags & ZR_FUNCTION_FRAME_SLOT_FLAG_INLINE_RECEIVER_ARGUMENT) != 0u) {
                return ZR_FALSE;
            }
            storageSize = (TZrUInt32)sizeof(SZrFunctionFrameIndirectAliasBinding);
            storageAlign = (TZrUInt32)_Alignof(SZrFunctionFrameIndirectAliasBinding);
        }
        if (storageAlign > function->frameByteAlign ||
            layout->byteOffset % storageAlign != 0u ||
            !backend_aot_exec_ir_frame_storage_contains(
                    function->frameByteSize, layout->byteOffset, storageSize)) {
            return ZR_FALSE;
        }
    }

    return (TZrBool)(
            backend_aot_exec_ir_validate_required_borrowed_parameter_rows(
                    state, function) &&
            (!hasCompleteSlotTable ||
             parameterLayoutCount == function->parameterCount));
}

static void backend_aot_exec_ir_project_parameter_passing_form(
        const SZrFunctionTypedLocalBinding *source,
        SZrAotExecIrParameterLayout *destination) {
    TZrUInt32 passingRoleFlags;

    if (source == ZR_NULL || destination == ZR_NULL) {
        return;
    }
    passingRoleFlags =
            source->roleFlags &
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_MASK;
    switch (passingRoleFlags) {
        case ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE:
            destination->passingForm =
                    (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_VALUE;
            break;
        case ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_IN:
            destination->passingForm =
                    (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_IN;
            break;
        case ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_REF:
            destination->passingForm =
                    (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_REF;
            break;
        case ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_REF_READONLY:
            destination->passingForm =
                    (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_REF_READONLY;
            break;
        case ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_SCOPED_REF:
            destination->passingForm =
                    (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_SCOPED_REF;
            break;
        case ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_SCOPED_REF_READONLY:
            destination->passingForm =
                    (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_SCOPED_REF_READONLY;
            break;
        case ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_OUT:
            destination->passingForm =
                    (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_OUT;
            break;
        default:
            return;
    }
    destination->passingFormKnown = ZR_TRUE;
}

static TZrBool backend_aot_exec_ir_build_parameter_layouts(
        SZrState *state,
        const SZrFunction *function,
        SZrAotExecIrFrameLayout *outFrameLayout) {
    TZrUInt32 parameterIndex = 0u;

    if (function->parameterCount == 0u) {
        return ZR_TRUE;
    }

    outFrameLayout->parameterLayouts =
            (SZrAotExecIrParameterLayout *)ZrCore_Memory_RawMallocWithType(
                    state->global,
                    sizeof(SZrAotExecIrParameterLayout) * function->parameterCount,
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (outFrameLayout->parameterLayouts == ZR_NULL) {
        return ZR_FALSE;
    }
    outFrameLayout->parameterLayoutCount = function->parameterCount;
    ZrCore_Memory_RawSet(
            outFrameLayout->parameterLayouts,
            0,
            sizeof(SZrAotExecIrParameterLayout) * function->parameterCount);

    if (function->typedLocalBindingLength == 0u) {
        for (parameterIndex = 0u;
             parameterIndex < function->parameterCount;
             parameterIndex++) {
            SZrAotExecIrParameterLayout *destination =
                    &outFrameLayout->parameterLayouts[parameterIndex];

            destination->stackSlot = parameterIndex;
            if (function->parameterMetadataCount == function->parameterCount) {
                destination->type = function->parameterMetadata[parameterIndex].type;
            }
            backend_aot_exec_ir_project_parameter_default_declaration(
                    function, parameterIndex, destination);
        }
        return ZR_TRUE;
    }

    parameterIndex = 0u;
    for (TZrUInt32 bindingIndex = 0u;
         bindingIndex < function->typedLocalBindingLength &&
         parameterIndex < function->parameterCount;
         bindingIndex++) {
        const SZrFunctionTypedLocalBinding *source =
                &function->typedLocalBindings[bindingIndex];
        SZrAotExecIrParameterLayout *destination;

        if (!backend_aot_exec_ir_typed_local_is_parameter_eligible(source)) {
            continue;
        }

        destination = &outFrameLayout->parameterLayouts[parameterIndex];
        destination->stackSlot = source->stackSlot;
        destination->symbolId = source->symbolId;
        destination->typeId = source->typeId;
        destination->placeId = source->placeId;
        destination->roleFlags =
                source->roleFlags & ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER;
        backend_aot_exec_ir_project_parameter_passing_form(
                source, destination);
        destination->type = source->type;
        backend_aot_exec_ir_project_parameter_default_declaration(
                function, parameterIndex, destination);
        parameterIndex++;
    }

    return (TZrBool)(parameterIndex == function->parameterCount);
}

TZrBool backend_aot_exec_ir_build_frame_layout(
        SZrState *state,
        const SZrFunction *function,
        SZrAotExecIrFrameLayout *outFrameLayout) {
    if (state == ZR_NULL || state->global == ZR_NULL ||
        function == ZR_NULL || outFrameLayout == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!backend_aot_exec_ir_validate_frame_layout(state, function)) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outFrameLayout, 0, sizeof(*outFrameLayout));
    outFrameLayout->parameterCount = function->parameterCount;
    outFrameLayout->stackSlotCount = function->stackSize;
    outFrameLayout->generatedFrameSlotCount =
            ZrCore_Function_GetGeneratedFrameSlotCount(function);
    outFrameLayout->closureValueCount = function->closureValueLength;
    outFrameLayout->localVariableCount = function->localVariableLength;
    outFrameLayout->exportedValueCount = function->exportedVariableLength;
    outFrameLayout->frameByteSize = function->frameByteSize;
    outFrameLayout->frameByteAlign = function->frameByteAlign;
    outFrameLayout->slotLayoutCount = function->frameSlotLayoutLength;

    if (!backend_aot_exec_ir_build_parameter_layouts(
                state, function, outFrameLayout)) {
        backend_aot_exec_ir_release_frame_layout(state, outFrameLayout);
        return ZR_FALSE;
    }

    if (function->frameSlotLayoutLength > 0u) {
        outFrameLayout->slotLayouts =
                (SZrAotExecIrFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
                        state->global,
                        sizeof(SZrAotExecIrFrameSlotLayout) *
                                function->frameSlotLayoutLength,
                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (outFrameLayout->slotLayouts == ZR_NULL) {
            backend_aot_exec_ir_release_frame_layout(state, outFrameLayout);
            return ZR_FALSE;
        }

        ZrCore_Memory_RawSet(
                outFrameLayout->slotLayouts,
                0,
                sizeof(SZrAotExecIrFrameSlotLayout) *
                        function->frameSlotLayoutLength);
        for (TZrUInt32 layoutIndex = 0u;
             layoutIndex < function->frameSlotLayoutLength;
             layoutIndex++) {
            const SZrFunctionFrameSlotLayout *sourceLayout =
                    &function->frameSlotLayouts[layoutIndex];
            SZrAotExecIrFrameSlotLayout *destinationLayout =
                    &outFrameLayout->slotLayouts[layoutIndex];

            destinationLayout->stackSlot = sourceLayout->stackSlot;
            destinationLayout->byteOffset = sourceLayout->byteOffset;
            destinationLayout->byteSize = sourceLayout->byteSize;
            destinationLayout->byteAlign = sourceLayout->byteAlign;
            destinationLayout->typeLayoutId = sourceLayout->typeLayoutId;
            destinationLayout->slotKind = sourceLayout->slotKind;
            destinationLayout->isParameter = sourceLayout->isParameter;
            destinationLayout->reserved0 = sourceLayout->reserved0;
        }
    }

    return ZR_TRUE;
}

void backend_aot_exec_ir_release_frame_layout(
        SZrState *state,
        SZrAotExecIrFrameLayout *frameLayout) {
    if (state == ZR_NULL || state->global == ZR_NULL || frameLayout == ZR_NULL) {
        return;
    }

    if (frameLayout->parameterLayouts != ZR_NULL &&
        frameLayout->parameterLayoutCount > 0u) {
        ZrCore_Memory_RawFreeWithType(
                state->global,
                frameLayout->parameterLayouts,
                sizeof(SZrAotExecIrParameterLayout) *
                        frameLayout->parameterLayoutCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (frameLayout->slotLayouts != ZR_NULL &&
        frameLayout->slotLayoutCount > 0u) {
        ZrCore_Memory_RawFreeWithType(
                state->global,
                frameLayout->slotLayouts,
                sizeof(SZrAotExecIrFrameSlotLayout) *
                        frameLayout->slotLayoutCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    ZrCore_Memory_RawSet(frameLayout, 0, sizeof(*frameLayout));
}
