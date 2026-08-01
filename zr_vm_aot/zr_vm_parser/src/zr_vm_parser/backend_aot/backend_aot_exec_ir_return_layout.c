#include "backend_aot_exec_ir_return_layout.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type_layout.h"

static TZrBool backend_aot_exec_ir_nullable_strings_are_equal(
        const SZrString *lhs,
        const SZrString *rhs) {
    if (lhs == rhs) {
        return ZR_TRUE;
    }
    if (lhs == ZR_NULL || rhs == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrCore_String_Equal((SZrString *)lhs, (SZrString *)rhs);
}

static TZrBool backend_aot_exec_ir_type_refs_have_same_semantic_identity(
        const SZrFunctionTypedTypeRef *lhs,
        const SZrFunctionTypedTypeRef *rhs) {
    if (lhs == ZR_NULL || rhs == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(lhs->baseType == rhs->baseType &&
                     lhs->isNullable == rhs->isNullable &&
                     lhs->ownershipQualifier == rhs->ownershipQualifier &&
                     lhs->isArray == rhs->isArray &&
                     backend_aot_exec_ir_nullable_strings_are_equal(
                             lhs->typeName, rhs->typeName) &&
                     lhs->elementBaseType == rhs->elementBaseType &&
                     backend_aot_exec_ir_nullable_strings_are_equal(
                             lhs->elementTypeName, rhs->elementTypeName));
}

static TZrBool backend_aot_exec_ir_return_layout_fields_are_copy_compatible(
        const SZrTypeLayout *lhs,
        const SZrTypeLayout *rhs) {
    TZrUInt32 fieldIndex;

    if (lhs->fieldCount != rhs->fieldCount ||
        (lhs->fieldCount > 0u &&
         (lhs->fields == ZR_NULL || rhs->fields == ZR_NULL))) {
        return ZR_FALSE;
    }
    for (fieldIndex = 0u; fieldIndex < lhs->fieldCount; fieldIndex++) {
        if (lhs->fields[fieldIndex].byteOffset !=
                    rhs->fields[fieldIndex].byteOffset ||
            lhs->fields[fieldIndex].byteSize !=
                    rhs->fields[fieldIndex].byteSize ||
            lhs->fields[fieldIndex].typeLayoutIndex !=
                    rhs->fields[fieldIndex].typeLayoutIndex ||
            lhs->fields[fieldIndex].flags !=
                    rhs->fields[fieldIndex].flags) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_exec_ir_return_layouts_are_copy_compatible(
        const SZrTypeLayout *lhs,
        const SZrTypeLayout *rhs) {
    if (lhs == ZR_NULL || rhs == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(
            lhs->byteSize == rhs->byteSize &&
            lhs->byteAlign == rhs->byteAlign &&
            lhs->kind == rhs->kind &&
            lhs->copyKind == rhs->copyKind &&
            lhs->dropKind == rhs->dropKind &&
            lhs->gcFieldCount == rhs->gcFieldCount &&
            lhs->ownershipFieldCount == rhs->ownershipFieldCount &&
            backend_aot_exec_ir_return_layout_fields_are_copy_compatible(
                    lhs, rhs));
}

static const SZrAotExecIrFrameSlotLayout *backend_aot_exec_ir_find_frame_slot_layout(
        const SZrAotExecIrFrameLayout *frameLayout,
        TZrUInt32 stackSlot) {
    TZrUInt32 layoutIndex;

    if (frameLayout == ZR_NULL || frameLayout->slotLayouts == ZR_NULL) {
        return ZR_NULL;
    }
    for (layoutIndex = 0u; layoutIndex < frameLayout->slotLayoutCount; layoutIndex++) {
        if (frameLayout->slotLayouts[layoutIndex].stackSlot == stackSlot) {
            return &frameLayout->slotLayouts[layoutIndex];
        }
    }
    return ZR_NULL;
}

TZrBool backend_aot_exec_ir_project_direct_inline_return_layout(
        SZrState *state,
        const SZrFunction *function,
        SZrAotExecIrFunction *outFunction) {
    TZrUInt32 directInlineReturnTypeLayoutId =
            ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    const SZrTypeLayout *directInlineReturnTypeLayout = ZR_NULL;
    TZrBool foundTypedReturn = ZR_FALSE;
    TZrBool projectionUnknown;
    TZrUInt32 instructionIndex;

    if (state == ZR_NULL || function == ZR_NULL || outFunction == ZR_NULL) {
        return ZR_FALSE;
    }
    if (function->semIrInstructionLength > 0u &&
        function->semIrInstructions == ZR_NULL) {
        return ZR_FALSE;
    }
    projectionUnknown = (TZrBool)(
            outFunction->callableReturnTypeKnown != ZR_TRUE);

    for (instructionIndex = 0u;
         instructionIndex < function->semIrInstructionLength;
         instructionIndex++) {
        const SZrSemIrInstruction *sourceInstruction;
        const SZrFunctionTypedTypeRef *sourceType;
        const SZrAotExecIrFrameSlotLayout *sourceLayout;
        const SZrTypeLayout *resolvedTypeLayout;

        sourceInstruction = &function->semIrInstructions[instructionIndex];
        if (sourceInstruction->opcode !=
            (TZrUInt32)ZR_SEMIR_OPCODE_RETURN_TYPED) {
            continue;
        }
        foundTypedReturn = ZR_TRUE;
        if (function->semIrTypeTable == ZR_NULL ||
            sourceInstruction->typeTableIndex >= function->semIrTypeTableLength) {
            return ZR_FALSE;
        }
        sourceType = &function->semIrTypeTable[sourceInstruction->typeTableIndex];
        sourceLayout = backend_aot_exec_ir_find_frame_slot_layout(
                &outFunction->frameLayout, sourceInstruction->operand0);
        if (sourceLayout == ZR_NULL ||
            sourceLayout->slotKind !=
                    (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
            sourceLayout->byteSize == 0u ||
            sourceLayout->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
            return ZR_FALSE;
        }
        if (outFunction->callableReturnTypeKnown == ZR_TRUE &&
            !backend_aot_exec_ir_type_refs_have_same_semantic_identity(
                    sourceType, &outFunction->callableReturnType)) {
            return ZR_FALSE;
        }
        if (sourceType->staticCType == ZR_STATIC_C_TYPE_DYNAMIC) {
            if (sourceType->staticCTypeId !=
                ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
                return ZR_FALSE;
            }
            projectionUnknown = ZR_TRUE;
        } else if (sourceType->staticCType == ZR_STATIC_C_TYPE_STRUCT) {
            if (sourceType->staticCTypeId ==
                        ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
                sourceType->staticCTypeId != sourceLayout->typeLayoutId) {
                return ZR_FALSE;
            }
        } else {
            return ZR_FALSE;
        }

        resolvedTypeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(
                function, sourceLayout->typeLayoutId, state);
        if (resolvedTypeLayout == ZR_NULL) {
            return ZR_FALSE;
        }
        if (resolvedTypeLayout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION ||
            (sourceLayout->reserved0 &
             ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS) != 0u) {
            projectionUnknown = ZR_TRUE;
            continue;
        }
        if (resolvedTypeLayout->kind !=
            (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT) {
            return ZR_FALSE;
        }
        if (directInlineReturnTypeLayoutId !=
                    ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE &&
            directInlineReturnTypeLayoutId != sourceLayout->typeLayoutId) {
            if (!backend_aot_exec_ir_return_layouts_are_copy_compatible(
                        directInlineReturnTypeLayout, resolvedTypeLayout)) {
                return ZR_FALSE;
            }
            projectionUnknown = ZR_TRUE;
            continue;
        }
        directInlineReturnTypeLayoutId = sourceLayout->typeLayoutId;
        directInlineReturnTypeLayout = resolvedTypeLayout;
    }

    if (foundTypedReturn && !projectionUnknown &&
        directInlineReturnTypeLayoutId !=
                ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        outFunction->directInlineReturnLayoutKnown = ZR_TRUE;
        outFunction->directInlineReturnTypeLayoutId = directInlineReturnTypeLayoutId;
    }
    return ZR_TRUE;
}
