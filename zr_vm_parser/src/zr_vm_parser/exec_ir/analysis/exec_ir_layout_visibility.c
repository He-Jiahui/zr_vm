#include "zr_vm_parser/exec_ir_layout_visibility.h"

TZrBool ZrParser_ExecIr_LayoutCanTransform(
        const SZrExecIrLayoutVisibility *visibility,
        EZrExecIrLayoutVisibilityReason *reason) {
    EZrExecIrLayoutVisibilityReason result = ZR_EXEC_IR_LAYOUT_PRIVATE;
    if (visibility == ZR_NULL) {
        if (reason != ZR_NULL) *reason = ZR_EXEC_IR_LAYOUT_OPEN_WORLD;
        return ZR_FALSE;
    }
    if (!visibility->privateClosedWorld) result = ZR_EXEC_IR_LAYOUT_OPEN_WORLD;
    else if (visibility->reflectionVisible) result = ZR_EXEC_IR_LAYOUT_REFLECTION_VISIBLE;
    else if (visibility->ffiVisible) result = ZR_EXEC_IR_LAYOUT_FFI_VISIBLE;
    else if (visibility->addressEscapes) result = ZR_EXEC_IR_LAYOUT_ADDRESS_ESCAPES;
    else if (visibility->serializationObserved) result = ZR_EXEC_IR_LAYOUT_SERIALIZATION_VISIBLE;
    if (reason != ZR_NULL) *reason = result;
    return (TZrBool)(result == ZR_EXEC_IR_LAYOUT_PRIVATE);
}
