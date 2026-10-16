#ifndef ZR_VM_PARSER_EXEC_IR_FRAME_LAYOUT_H
#define ZR_VM_PARSER_EXEC_IR_FRAME_LAYOUT_H

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/conf.h"

/* Packed-frame classification is intentionally parser-owned.  The legacy
 * EZrExecIrFrameSlotKind values remain unchanged for existing consumers. */
typedef enum EZrExecIrPackedSlotClass {
    ZR_EXEC_IR_PACKED_SLOT_BOXED = 0,
    ZR_EXEC_IR_PACKED_SLOT_SCALAR,
    ZR_EXEC_IR_PACKED_SLOT_INLINE_SPAN,
    ZR_EXEC_IR_PACKED_SLOT_REF,
    ZR_EXEC_IR_PACKED_SLOT_CLASS_COUNT
} EZrExecIrPackedSlotClass;

#define ZR_EXEC_IR_PACKED_SLOT_ADDRESS_ESCAPED ((TZrUInt32)1u << 0u)
#define ZR_EXEC_IR_PACKED_SLOT_MATERIALIZE ((TZrUInt32)1u << 1u)
#define ZR_EXEC_IR_PACKED_SLOT_PARAMETER ((TZrUInt32)1u << 2u)
#define ZR_EXEC_IR_PACKED_SLOT_KNOWN_FLAGS \
    (ZR_EXEC_IR_PACKED_SLOT_ADDRESS_ESCAPED | \
     ZR_EXEC_IR_PACKED_SLOT_MATERIALIZE | \
     ZR_EXEC_IR_PACKED_SLOT_PARAMETER)

typedef struct SZrExecIrPackedValue {
    TZrExecIrValueId valueId;
    TZrExecIrTypeToken typeToken;
    EZrExecIrPackedSlotClass slotClass;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt32 liveStart;
    TZrUInt32 liveEnd; /* half-open; equal bounds means an empty lifetime */
    TZrUInt32 flags;
} SZrExecIrPackedValue;

typedef struct SZrExecIrPackedFrameRequest {
    TZrMetadataToken functionToken;
    const SZrExecIrPackedValue *values;
    TZrUInt32 valueCount;
    TZrUInt32 parameterPrefixCount;
    TZrUInt32 returnBufferSize;
    TZrUInt32 returnBufferAlign;
    TZrUInt32 frameByteLimit; /* zero means no additional limit */
} SZrExecIrPackedFrameRequest;

typedef struct SZrExecIrPackedFrameLayout {
    SZrExecIrFrameLayout frame;
    TZrExecIrValueId *logicalValueIds;
    TZrUInt32 *logicalToPhysical;
    EZrExecIrPackedSlotClass *slotClasses;
} SZrExecIrPackedFrameLayout;

ZR_PARSER_API void ZrParser_ExecIr_PackedFrameLayoutInit(
        SZrExecIrPackedFrameLayout *layout);
ZR_PARSER_API void ZrParser_ExecIr_PackedFrameLayoutFree(
        SZrExecIrPackedFrameLayout *layout);
ZR_PARSER_API TZrBool ZrParser_ExecIr_LayoutPackedFrame(
        const SZrExecIrPackedFrameRequest *request,
        SZrExecIrPackedFrameLayout *layout,
        SZrExecIrDiagnostic *diagnostic);

#endif
