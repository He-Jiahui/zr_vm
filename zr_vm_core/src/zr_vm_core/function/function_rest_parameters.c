#include "function/function_rest_parameters.h"

#include "gc/gc_temporary_roots.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"

SZrObject *function_pack_rest_parameters(struct SZrState *state,
                                        TZrStackValuePointer callBase,
                                        TZrSize fixedCount,
                                        TZrSize actualCount) {
    SZrFunctionStackAnchor callBaseAnchor;
    SZrObject *array;

    ZR_ASSERT(actualCount >= fixedCount);
    ZrCore_Function_StackAnchorInit(state, callBase, &callBaseAnchor);
    array = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array == ZR_NULL || !ZrCore_Gc_TemporaryRootPush(state, ZR_CAST_RAW_OBJECT_AS_SUPER(array))) {
        return ZR_NULL;
    }

    ZrCore_Object_Init(state, array);
    for (TZrSize index = fixedCount; index < actualCount; ++index) {
        SZrTypeValue key;
        callBase = ZrCore_Function_StackAnchorRestore(state, &callBaseAnchor);
        ZrCore_Value_InitAsInt(state, &key, (TZrInt64)(index - fixedCount));
        ZrCore_Object_SetValue(state, array, &key, ZrCore_Stack_GetValueNoProfile(callBase + 1u + index));
    }

    callBase = ZrCore_Function_StackAnchorRestore(state, &callBaseAnchor);
    for (TZrSize index = fixedCount; index < actualCount; ++index) {
        SZrTypeValue *argument = ZrCore_Stack_GetValueNoProfile(callBase + 1u + index);
        ZrCore_Value_PrepareDestinationForOverwriteNoProfile(state, argument);
        ZrCore_Value_ResetAsNullNoProfile(argument);
    }
    return array;
}

void function_bind_rest_parameter(struct SZrState *state,
                                  const SZrFunction *function,
                                  TZrStackValuePointer frameBase,
                                  SZrObject *array) {
    TZrUInt32 slot = function->parameterCount;
    const SZrFunctionFrameSlotLayout *layout = ZrCore_Function_FindFrameSlotLayout(function, slot);
    SZrStackFramePlace place;
    SZrTypeValue *destination = ZrCore_Stack_GetValueNoProfile(frameBase + slot);

    if (layout != ZR_NULL && layout->slotKind == ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&
        layout->byteSize >= sizeof(*destination) &&
        ZrCore_Function_MakeFrameSlotPlace(state, function, frameBase, slot, &place)) {
        destination = (SZrTypeValue *)place.address;
    }
    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(array));
}
