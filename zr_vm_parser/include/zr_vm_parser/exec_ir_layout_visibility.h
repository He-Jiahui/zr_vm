#ifndef ZR_VM_PARSER_EXEC_IR_LAYOUT_VISIBILITY_H
#define ZR_VM_PARSER_EXEC_IR_LAYOUT_VISIBILITY_H

#include "zr_vm_parser/conf.h"

typedef enum EZrExecIrLayoutVisibilityReason {
    ZR_EXEC_IR_LAYOUT_PRIVATE = 0,
    ZR_EXEC_IR_LAYOUT_REFLECTION_VISIBLE,
    ZR_EXEC_IR_LAYOUT_FFI_VISIBLE,
    ZR_EXEC_IR_LAYOUT_ADDRESS_ESCAPES,
    ZR_EXEC_IR_LAYOUT_SERIALIZATION_VISIBLE,
    ZR_EXEC_IR_LAYOUT_OPEN_WORLD
} EZrExecIrLayoutVisibilityReason;

typedef struct SZrExecIrLayoutVisibility {
    TZrBool privateClosedWorld;
    TZrBool reflectionVisible;
    TZrBool ffiVisible;
    TZrBool addressEscapes;
    TZrBool serializationObserved;
} SZrExecIrLayoutVisibility;

ZR_PARSER_API TZrBool ZrParser_ExecIr_LayoutCanTransform(
        const SZrExecIrLayoutVisibility *visibility,
        EZrExecIrLayoutVisibilityReason *reason);

#endif
