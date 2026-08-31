//
// Created by Codex on 2026/5/16.
//

#ifndef ZR_VM_CORE_TYPE_LAYOUT_H
#define ZR_VM_CORE_TYPE_LAYOUT_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

#define ZR_TYPE_LAYOUT_MAX_NESTING_DEPTH ((TZrUInt32)64u)

struct SZrState;

#define ZR_TYPE_LAYOUT_SCHEMA_VERSION ((TZrUInt32)2u)

typedef enum EZrTypeLayoutKind {
    ZR_TYPE_LAYOUT_KIND_VALUE = 0,
    ZR_TYPE_LAYOUT_KIND_STRUCT = 1,
    ZR_TYPE_LAYOUT_KIND_UNION = 2
} EZrTypeLayoutKind;

typedef enum EZrTypeLayoutCopyKind {
    ZR_TYPE_LAYOUT_COPY_KIND_BITWISE = 0,
    ZR_TYPE_LAYOUT_COPY_KIND_POD = ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
    ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE = 1,
    ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY = ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
    ZR_TYPE_LAYOUT_COPY_KIND_MOVE_ONLY = 2
} EZrTypeLayoutCopyKind;

typedef enum EZrTypeLayoutDropKind {
    ZR_TYPE_LAYOUT_DROP_KIND_NONE = 0,
    ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE = 1,
    ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP = ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
    ZR_TYPE_LAYOUT_DROP_KIND_CUSTOM_THEN_FIELDS = 2
} EZrTypeLayoutDropKind;

typedef enum EZrTypeLayoutGcScanKind {
    ZR_TYPE_LAYOUT_GC_SCAN_FREE = 0,
    ZR_TYPE_LAYOUT_GC_SCAN_MAPPED = 1,
    ZR_TYPE_LAYOUT_GC_SCAN_BARRIERED = 2
} EZrTypeLayoutGcScanKind;

typedef enum EZrDomainTransferKind {
    ZR_DOMAIN_TRANSFER_KIND_FORBIDDEN = 0,
    ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY = 1,
    ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE = 2,
    ZR_DOMAIN_TRANSFER_KIND_IMMUTABLE_HANDLE = 3,
    ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE = 4
} EZrDomainTransferKind;

typedef enum EZrTypeLayoutFieldFlags {
    ZR_TYPE_LAYOUT_FIELD_FLAG_NONE = 0,
    ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT = 1u << 0u,
    ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE = 1u << 1u,
    ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE = 1u << 2u,
    ZR_TYPE_LAYOUT_FIELD_FLAG_REF_VALUE = 1u << 3u,
    ZR_TYPE_LAYOUT_FIELD_FLAG_NESTED_LAYOUT = 1u << 4u
} EZrTypeLayoutFieldFlags;

typedef struct SZrTypeLayoutField {
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 typeLayoutIndex;
    TZrUInt32 flags;
    TZrUInt32 activeTag;
} SZrTypeLayoutField;

typedef struct SZrTypeLayoutMetadata {
    TZrUInt32 cTypeId;
    const TZrUInt32 *gcFieldOffsets;
    const TZrUInt32 *ownershipFieldOffsets;
} SZrTypeLayoutMetadata;

typedef void (*FZrTypeLayoutCustomDrop)(struct SZrState *state,
                                       TZrPtr storage,
                                       TZrPtr userData);

typedef struct SZrTypeLayoutContract {
    TZrUInt32 cTypeId;
    EZrTypeLayoutGcScanKind gcScanKind;
    const TZrUInt32 *gcFieldOffsets;
    TZrUInt32 gcFieldCount;
    const TZrUInt32 *ownershipFieldOffsets;
    TZrUInt32 ownershipFieldCount;
    const TZrUInt32 *refFieldOffsets;
    TZrUInt32 refFieldCount;
    TZrBool hasDomainTransferContract;
    EZrDomainTransferKind domainTransferKind;
    TZrUInt32 domainTransferSchemaVersion;
    TZrUInt64 domainTransferSchemaHash;
    TZrUInt32 domainTransferProviderToken;
    TZrUInt64 domainTransferProviderContractHash;
    FZrTypeLayoutCustomDrop customDrop;
    TZrPtr customDropUserData;
} SZrTypeLayoutContract;

typedef struct SZrTypeLayout {
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt8 kind;
    TZrUInt8 copyKind;
    TZrUInt8 dropKind;
    TZrUInt8 gcScanKind;
    const SZrTypeLayoutField *fields;
    TZrUInt32 fieldCount;
    TZrUInt32 gcFieldCount;
    TZrUInt32 ownershipFieldCount;
    TZrUInt32 refFieldCount;
    TZrUInt32 tagOffset;
    TZrUInt32 tagSize;
    TZrBool blittable;
    TZrUInt8 domainTransferKind;
    TZrUInt8 reserved2;
    TZrUInt8 reserved3;
    TZrUInt32 cTypeId;
    TZrUInt32 layoutVersion;
    TZrUInt32 domainTransferSchemaVersion;
    TZrUInt32 domainTransferProviderToken;
    TZrUInt64 layoutHash;
    TZrUInt64 domainTransferSchemaHash;
    TZrUInt64 domainTransferProviderContractHash;
    const TZrUInt32 *gcFieldOffsets;
    const TZrUInt32 *ownershipFieldOffsets;
    const TZrUInt32 *refFieldOffsets;
    FZrTypeLayoutCustomDrop customDrop;
    TZrPtr customDropUserData;
} SZrTypeLayout;

typedef struct SZrTypeLayoutRegistryView {
    const SZrTypeLayout *const *layouts;
    TZrUInt32 count;
} SZrTypeLayoutRegistryView;

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

ZR_CORE_API void ZrCore_TypeLayout_InitStructWithMetadata(SZrTypeLayout *layout,
                                                          TZrUInt32 byteSize,
                                                          TZrUInt32 byteAlign,
                                                          EZrTypeLayoutCopyKind copyKind,
                                                          EZrTypeLayoutDropKind dropKind,
                                                          const SZrTypeLayoutField *fields,
                                                          TZrUInt32 fieldCount,
                                                          const SZrTypeLayoutMetadata *metadata);

ZR_CORE_API void ZrCore_TypeLayout_InitStructWithContract(SZrTypeLayout *layout,
                                                          TZrUInt32 byteSize,
                                                          TZrUInt32 byteAlign,
                                                          EZrTypeLayoutCopyKind copyKind,
                                                          EZrTypeLayoutDropKind dropKind,
                                                          const SZrTypeLayoutField *fields,
                                                          TZrUInt32 fieldCount,
                                                          const SZrTypeLayoutContract *contract);

ZR_CORE_API void ZrCore_TypeLayout_InitUnion(SZrTypeLayout *layout,
                                             TZrUInt32 byteSize,
                                             TZrUInt32 byteAlign,
                                             TZrUInt32 tagOffset,
                                             TZrUInt32 tagSize,
                                             EZrTypeLayoutCopyKind copyKind,
                                             EZrTypeLayoutDropKind dropKind,
                                             const SZrTypeLayoutField *fields,
                                             TZrUInt32 fieldCount);

ZR_CORE_API void ZrCore_TypeLayout_InitUnionWithContract(SZrTypeLayout *layout,
                                                         TZrUInt32 byteSize,
                                                         TZrUInt32 byteAlign,
                                                         TZrUInt32 tagOffset,
                                                         TZrUInt32 tagSize,
                                                         EZrTypeLayoutCopyKind copyKind,
                                                         EZrTypeLayoutDropKind dropKind,
                                                         const SZrTypeLayoutField *fields,
                                                         TZrUInt32 fieldCount,
                                                         const SZrTypeLayoutContract *contract);

ZR_CORE_API TZrUInt64 ZrCore_TypeLayout_ComputeHash(const SZrTypeLayout *layout);

ZR_CORE_API TZrBool ZrCore_TypeLayout_Validate(const SZrTypeLayout *layout);

ZR_CORE_API TZrBool ZrCore_TypeLayout_TryGetGcFieldOffset(const SZrTypeLayout *layout,
                                                          TZrUInt32 fieldIndex,
                                                          TZrUInt32 *outOffset);

ZR_CORE_API TZrBool ZrCore_TypeLayout_TryGetOwnershipFieldOffset(const SZrTypeLayout *layout,
                                                                 TZrUInt32 fieldIndex,
                                                                 TZrUInt32 *outOffset);

ZR_CORE_API TZrBool ZrCore_TypeLayout_TryGetRefFieldOffset(const SZrTypeLayout *layout,
                                                           TZrUInt32 fieldIndex,
                                                           TZrUInt32 *outOffset);

ZR_CORE_API TZrBool ZrCore_TypeLayout_CanRawCopy(const SZrTypeLayout *layout);

/* Returns true only for validated structures with no GC, ownership, ref,
 * or nested-layout fields, so a GC visitor may skip their storage entirely. */
ZR_CORE_API TZrBool ZrCore_TypeLayout_CanSkipGcScan(const SZrTypeLayout *layout);

ZR_CORE_API TZrBool ZrCore_TypeLayout_CopyInline(struct SZrState *state,
                                                 const SZrTypeLayout *layout,
                                                 TZrPtr destination,
                                                 const void *source);

ZR_CORE_API TZrBool ZrCore_TypeLayout_CopyInlineWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr destination,
        const void *source);
ZR_CORE_API TZrBool ZrCore_TypeLayout_InitializeStorageWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage);
ZR_CORE_API TZrBool ZrCore_TypeLayout_InitializeStorage(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        TZrPtr storage);

ZR_CORE_API void ZrCore_TypeLayout_DropInline(struct SZrState *state,
                                              const SZrTypeLayout *layout,
                                              TZrPtr storage);

ZR_CORE_API TZrBool ZrCore_TypeLayout_DropInlineWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage);

ZR_CORE_API TZrBool ZrCore_TypeLayout_DropPartialInlineWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage,
        const TZrUInt64 *initializedFieldWords,
        TZrUInt32 initializedFieldWordCount);

ZR_CORE_API void ZrCore_TypeLayout_VisitGcValues(struct SZrState *state,
                                                 const SZrTypeLayout *layout,
                                                 TZrPtr storage,
                                                 FZrTypeLayoutGcValueVisitor visitor,
                                                 TZrPtr userData);

ZR_CORE_API TZrBool ZrCore_TypeLayout_VisitGcValuesWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage,
        FZrTypeLayoutGcValueVisitor visitor,
        TZrPtr userData);

ZR_CORE_API TZrBool ZrCore_StackFrameLayout_BuildSequential(SZrStackFrameLayout *frameLayout,
                                                            SZrStackFrameLayoutSlot *slots,
                                                            TZrUInt32 slotCount);

#endif // ZR_VM_CORE_TYPE_LAYOUT_H
