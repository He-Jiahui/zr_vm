//
// Created by Codex on 2026/5/16.
//

#ifndef ZR_VM_CORE_TYPE_LAYOUT_H
#define ZR_VM_CORE_TYPE_LAYOUT_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

struct SZrState;

typedef enum EZrTypeLayoutKind {
    ZR_TYPE_LAYOUT_KIND_VALUE = 0,
    ZR_TYPE_LAYOUT_KIND_STRUCT = 1
} EZrTypeLayoutKind;

typedef enum EZrTypeLayoutCopyKind {
    ZR_TYPE_LAYOUT_COPY_KIND_POD = 0,
    ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY = 1
} EZrTypeLayoutCopyKind;

typedef enum EZrTypeLayoutDropKind {
    ZR_TYPE_LAYOUT_DROP_KIND_NONE = 0,
    ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP = 1
} EZrTypeLayoutDropKind;

typedef enum EZrTypeLayoutFieldFlags {
    ZR_TYPE_LAYOUT_FIELD_FLAG_NONE = 0,
    ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT = 1u << 0u,
    ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE = 1u << 1u,
    ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE = 1u << 2u
} EZrTypeLayoutFieldFlags;

typedef struct SZrTypeLayoutField {
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 typeLayoutIndex;
    TZrUInt32 flags;
} SZrTypeLayoutField;

typedef struct SZrTypeLayout {
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt8 kind;
    TZrUInt8 copyKind;
    TZrUInt8 dropKind;
    TZrUInt8 reserved0;
    const SZrTypeLayoutField *fields;
    TZrUInt32 fieldCount;
    TZrUInt32 gcFieldCount;
    TZrUInt32 ownershipFieldCount;
} SZrTypeLayout;

typedef struct SZrStackFrameLayoutSlot {
    const SZrTypeLayout *typeLayout;
    TZrUInt32 byteOffset;
} SZrStackFrameLayoutSlot;

typedef struct SZrStackFrameLayout {
    TZrUInt32 byteSize;
    TZrUInt32 maxAlign;
    TZrUInt32 slotCount;
} SZrStackFrameLayout;

typedef void (*FZrTypeLayoutGcValueVisitor)(struct SZrState *state,
                                            SZrTypeValue *value,
                                            TZrPtr userData);

ZR_CORE_API void ZrCore_TypeLayout_InitValue(SZrTypeLayout *layout);

ZR_CORE_API void ZrCore_TypeLayout_InitStruct(SZrTypeLayout *layout,
                                              TZrUInt32 byteSize,
                                              TZrUInt32 byteAlign,
                                              EZrTypeLayoutCopyKind copyKind,
                                              EZrTypeLayoutDropKind dropKind,
                                              const SZrTypeLayoutField *fields,
                                              TZrUInt32 fieldCount);

ZR_CORE_API TZrBool ZrCore_TypeLayout_CanRawCopy(const SZrTypeLayout *layout);

ZR_CORE_API TZrBool ZrCore_TypeLayout_CopyInline(struct SZrState *state,
                                                 const SZrTypeLayout *layout,
                                                 TZrPtr destination,
                                                 const void *source);

ZR_CORE_API void ZrCore_TypeLayout_DropInline(struct SZrState *state,
                                              const SZrTypeLayout *layout,
                                              TZrPtr storage);

ZR_CORE_API void ZrCore_TypeLayout_VisitGcValues(struct SZrState *state,
                                                 const SZrTypeLayout *layout,
                                                 TZrPtr storage,
                                                 FZrTypeLayoutGcValueVisitor visitor,
                                                 TZrPtr userData);

ZR_CORE_API TZrBool ZrCore_StackFrameLayout_BuildSequential(SZrStackFrameLayout *frameLayout,
                                                            SZrStackFrameLayoutSlot *slots,
                                                            TZrUInt32 slotCount);

#endif // ZR_VM_CORE_TYPE_LAYOUT_H
