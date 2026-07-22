//
// Created by Codex on 2026/7/20.
//

#include "zr_vm_core/type_layout.h"

#include <string.h>

static TZrUInt32 type_layout_normalize_align(TZrUInt32 align) {
    return align > 0u ? align : 1u;
}

static TZrBool type_layout_field_is_gc_value(const SZrTypeLayoutField *field) {
    return (TZrBool)(field != ZR_NULL &&
                     (field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE) != 0u);
}

static TZrBool type_layout_field_is_ownership_value(const SZrTypeLayoutField *field) {
    return (TZrBool)(field != ZR_NULL &&
                     (field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE) != 0u);
}

static TZrBool type_layout_field_is_ref_value(const SZrTypeLayoutField *field) {
    return (TZrBool)(field != ZR_NULL &&
                     (field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_REF_VALUE) != 0u);
}

static TZrBool type_layout_is_blittable(EZrTypeLayoutCopyKind copyKind,
                                        EZrTypeLayoutDropKind dropKind,
                                        TZrUInt32 gcFieldCount,
                                        TZrUInt32 ownershipFieldCount,
                                        TZrUInt32 refFieldCount) {
    return (TZrBool)(copyKind == ZR_TYPE_LAYOUT_COPY_KIND_BITWISE &&
                     dropKind == ZR_TYPE_LAYOUT_DROP_KIND_NONE &&
                     gcFieldCount == 0u &&
                     ownershipFieldCount == 0u &&
                     refFieldCount == 0u);
}

static void type_layout_apply_contract(SZrTypeLayout *layout,
                                       const SZrTypeLayoutContract *contract) {
    TZrBool hasExplicitDomainTransfer;

    if (layout == ZR_NULL) {
        return;
    }

    layout->cTypeId = contract != ZR_NULL ? contract->cTypeId : 0u;
    layout->gcScanKind = (TZrUInt8)(contract != ZR_NULL &&
                                                    contract->gcScanKind != ZR_TYPE_LAYOUT_GC_SCAN_FREE
                                            ? contract->gcScanKind
                                            : (layout->gcFieldCount > 0u
                                                       ? ZR_TYPE_LAYOUT_GC_SCAN_MAPPED
                                                       : ZR_TYPE_LAYOUT_GC_SCAN_FREE));
    layout->gcFieldOffsets = contract != ZR_NULL ? contract->gcFieldOffsets : ZR_NULL;
    layout->ownershipFieldOffsets = contract != ZR_NULL ? contract->ownershipFieldOffsets : ZR_NULL;
    layout->refFieldOffsets = contract != ZR_NULL ? contract->refFieldOffsets : ZR_NULL;
    hasExplicitDomainTransfer = (TZrBool)(
            contract != ZR_NULL && contract->hasDomainTransferContract);
    if (hasExplicitDomainTransfer) {
        layout->domainTransferKind = (TZrUInt8)contract->domainTransferKind;
        layout->domainTransferSchemaVersion =
                contract->domainTransferSchemaVersion;
        layout->domainTransferSchemaHash = contract->domainTransferSchemaHash;
        layout->domainTransferProviderToken =
                contract->domainTransferProviderToken;
        layout->domainTransferProviderContractHash =
                contract->domainTransferProviderContractHash;
    } else if (layout->blittable) {
        layout->domainTransferKind =
                (TZrUInt8)ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY;
        layout->domainTransferSchemaVersion = ZR_TYPE_LAYOUT_SCHEMA_VERSION;
        layout->domainTransferSchemaHash = 0u;
        layout->domainTransferProviderToken = 0u;
        layout->domainTransferProviderContractHash = 0u;
    } else {
        layout->domainTransferKind =
                (TZrUInt8)ZR_DOMAIN_TRANSFER_KIND_FORBIDDEN;
        layout->domainTransferSchemaVersion = 0u;
        layout->domainTransferSchemaHash = 0u;
        layout->domainTransferProviderToken = 0u;
        layout->domainTransferProviderContractHash = 0u;
    }
    layout->customDrop = contract != ZR_NULL ? contract->customDrop : ZR_NULL;
    layout->customDropUserData = contract != ZR_NULL ? contract->customDropUserData : ZR_NULL;
}

static const SZrTypeLayout *type_layout_initialization_resolve_nested(
        const SZrTypeLayoutRegistryView *registry,
        TZrUInt32 typeLayoutIndex) {
    if (registry == ZR_NULL || registry->layouts == ZR_NULL ||
        typeLayoutIndex >= registry->count) {
        return ZR_NULL;
    }
    return registry->layouts[typeLayoutIndex];
}

static TZrBool type_layout_initialize_storage_with_registry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage,
        TZrUInt32 depth) {
    ZR_UNUSED_PARAMETER(state);

    if (layout == ZR_NULL || storage == ZR_NULL ||
        !ZrCore_TypeLayout_Validate(layout) ||
        depth > ZR_TYPE_LAYOUT_MAX_NESTING_DEPTH) {
        return ZR_FALSE;
    }
    memset(storage, 0, layout->byteSize);
    if (layout->kind == (TZrUInt8)ZR_TYPE_LAYOUT_KIND_VALUE) {
        if (layout->byteSize < sizeof(SZrTypeValue)) {
            return ZR_FALSE;
        }
        ZrCore_Value_ResetAsNull((SZrTypeValue *)storage);
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0u; index < layout->fieldCount; index++) {
        const SZrTypeLayoutField *field = &layout->fields[index];
        TZrUInt32 fieldEnd;

        if (field->byteOffset > UINT32_MAX - field->byteSize) {
            return ZR_FALSE;
        }
        fieldEnd = field->byteOffset + field->byteSize;
        if (fieldEnd > layout->byteSize) {
            return ZR_FALSE;
        }
        if ((field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_NESTED_LAYOUT) != 0u) {
            const SZrTypeLayout *nestedLayout =
                    type_layout_initialization_resolve_nested(
                            registry, field->typeLayoutIndex);
            if (nestedLayout == ZR_NULL ||
                nestedLayout->byteSize != field->byteSize ||
                !type_layout_initialize_storage_with_registry(
                        state,
                        nestedLayout,
                        registry,
                        (TZrByte *)storage + field->byteOffset,
                        depth + 1u)) {
                return ZR_FALSE;
            }
        } else if ((field->flags & ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT) != 0u) {
            if (field->byteSize < sizeof(SZrTypeValue)) {
                return ZR_FALSE;
            }
            ZrCore_Value_ResetAsNull(
                    (SZrTypeValue *)((TZrByte *)storage + field->byteOffset));
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_TypeLayout_InitializeStorageWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage) {
    return type_layout_initialize_storage_with_registry(
            state, layout, registry, storage, 0u);
}

TZrBool ZrCore_TypeLayout_InitializeStorage(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        TZrPtr storage) {
    return ZrCore_TypeLayout_InitializeStorageWithRegistry(
            state, layout, ZR_NULL, storage);
}

void ZrCore_TypeLayout_InitValue(SZrTypeLayout *layout) {
    if (layout == ZR_NULL) {
        return;
    }

    layout->byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout->byteAlign = (TZrUInt32)ZR_ALIGN_SIZE;
    layout->kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_VALUE;
    layout->copyKind = (TZrUInt8)ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE;
    layout->dropKind = (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE;
    layout->gcScanKind = (TZrUInt8)ZR_TYPE_LAYOUT_GC_SCAN_MAPPED;
    layout->fields = ZR_NULL;
    layout->fieldCount = 0u;
    layout->gcFieldCount = 1u;
    layout->ownershipFieldCount = 1u;
    layout->refFieldCount = 0u;
    layout->tagOffset = 0u;
    layout->tagSize = 0u;
    layout->blittable = ZR_FALSE;
    layout->domainTransferKind = (TZrUInt8)ZR_DOMAIN_TRANSFER_KIND_FORBIDDEN;
    layout->reserved2 = 0u;
    layout->reserved3 = 0u;
    layout->cTypeId = 0u;
    layout->layoutVersion = ZR_TYPE_LAYOUT_SCHEMA_VERSION;
    layout->domainTransferSchemaVersion = 0u;
    layout->domainTransferProviderToken = 0u;
    layout->domainTransferSchemaHash = 0u;
    layout->domainTransferProviderContractHash = 0u;
    layout->gcFieldOffsets = ZR_NULL;
    layout->ownershipFieldOffsets = ZR_NULL;
    layout->refFieldOffsets = ZR_NULL;
    layout->customDrop = ZR_NULL;
    layout->customDropUserData = ZR_NULL;
    layout->layoutHash = ZrCore_TypeLayout_ComputeHash(layout);
}

void ZrCore_TypeLayout_InitStruct(SZrTypeLayout *layout,
                                  TZrUInt32 byteSize,
                                  TZrUInt32 byteAlign,
                                  EZrTypeLayoutCopyKind copyKind,
                                  EZrTypeLayoutDropKind dropKind,
                                  const SZrTypeLayoutField *fields,
                                  TZrUInt32 fieldCount) {
    ZrCore_TypeLayout_InitStructWithContract(layout,
                                             byteSize,
                                             byteAlign,
                                             copyKind,
                                             dropKind,
                                             fields,
                                             fieldCount,
                                             ZR_NULL);
}

void ZrCore_TypeLayout_InitStructWithMetadata(SZrTypeLayout *layout,
                                              TZrUInt32 byteSize,
                                              TZrUInt32 byteAlign,
                                              EZrTypeLayoutCopyKind copyKind,
                                              EZrTypeLayoutDropKind dropKind,
                                              const SZrTypeLayoutField *fields,
                                              TZrUInt32 fieldCount,
                                              const SZrTypeLayoutMetadata *metadata) {
    SZrTypeLayoutContract contract;

    if (metadata == ZR_NULL) {
        ZrCore_TypeLayout_InitStruct(layout,
                                     byteSize,
                                     byteAlign,
                                     copyKind,
                                     dropKind,
                                     fields,
                                     fieldCount);
        return;
    }

    memset(&contract, 0, sizeof(contract));
    contract.cTypeId = metadata->cTypeId;
    contract.gcFieldOffsets = metadata->gcFieldOffsets;
    contract.ownershipFieldOffsets = metadata->ownershipFieldOffsets;
    ZrCore_TypeLayout_InitStructWithContract(layout,
                                             byteSize,
                                             byteAlign,
                                             copyKind,
                                             dropKind,
                                             fields,
                                             fieldCount,
                                             &contract);
}

void ZrCore_TypeLayout_InitStructWithContract(SZrTypeLayout *layout,
                                              TZrUInt32 byteSize,
                                              TZrUInt32 byteAlign,
                                              EZrTypeLayoutCopyKind copyKind,
                                              EZrTypeLayoutDropKind dropKind,
                                              const SZrTypeLayoutField *fields,
                                              TZrUInt32 fieldCount,
                                              const SZrTypeLayoutContract *contract) {
    TZrUInt32 gcFieldCount = 0u;
    TZrUInt32 ownershipFieldCount = 0u;
    TZrUInt32 refFieldCount = 0u;

    if (layout == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0u; fields != ZR_NULL && index < fieldCount; index++) {
        if (type_layout_field_is_gc_value(&fields[index])) {
            gcFieldCount++;
        }
        if (type_layout_field_is_ownership_value(&fields[index])) {
            ownershipFieldCount++;
        }
        if (type_layout_field_is_ref_value(&fields[index])) {
            refFieldCount++;
        }
    }

    if (contract != ZR_NULL) {
        if (contract->gcFieldCount > 0u) {
            gcFieldCount = contract->gcFieldCount;
        }
        if (contract->ownershipFieldCount > 0u) {
            ownershipFieldCount = contract->ownershipFieldCount;
        }
        if (contract->refFieldCount > 0u) {
            refFieldCount = contract->refFieldCount;
        }
    }

    layout->byteSize = byteSize > 0u ? byteSize : 1u;
    layout->byteAlign = type_layout_normalize_align(byteAlign);
    layout->kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT;
    layout->copyKind = (TZrUInt8)copyKind;
    layout->dropKind = (TZrUInt8)dropKind;
    layout->fields = fields;
    layout->fieldCount = fieldCount;
    layout->gcFieldCount = gcFieldCount;
    layout->ownershipFieldCount = ownershipFieldCount;
    layout->refFieldCount = refFieldCount;
    layout->tagOffset = 0u;
    layout->tagSize = 0u;
    layout->blittable = type_layout_is_blittable(copyKind,
                                                 dropKind,
                                                 gcFieldCount,
                                                 ownershipFieldCount,
                                                 refFieldCount);
    layout->domainTransferKind = (TZrUInt8)ZR_DOMAIN_TRANSFER_KIND_FORBIDDEN;
    layout->reserved2 = 0u;
    layout->reserved3 = 0u;
    layout->layoutVersion = ZR_TYPE_LAYOUT_SCHEMA_VERSION;
    type_layout_apply_contract(layout, contract);
    if (layout->domainTransferKind ==
                (TZrUInt8)ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY &&
        layout->domainTransferSchemaHash == 0u) {
        layout->domainTransferSchemaHash = ZrCore_TypeLayout_ComputeHash(layout);
        if (layout->domainTransferSchemaHash == 0u) {
            layout->domainTransferSchemaHash = 1u;
        }
    }
    layout->layoutHash = ZrCore_TypeLayout_ComputeHash(layout);
}

void ZrCore_TypeLayout_InitUnion(SZrTypeLayout *layout,
                                 TZrUInt32 byteSize,
                                 TZrUInt32 byteAlign,
                                 TZrUInt32 tagOffset,
                                 TZrUInt32 tagSize,
                                 EZrTypeLayoutCopyKind copyKind,
                                 EZrTypeLayoutDropKind dropKind,
                                 const SZrTypeLayoutField *fields,
                                 TZrUInt32 fieldCount) {
    ZrCore_TypeLayout_InitUnionWithContract(layout,
                                            byteSize,
                                            byteAlign,
                                            tagOffset,
                                            tagSize,
                                            copyKind,
                                            dropKind,
                                            fields,
                                            fieldCount,
                                            ZR_NULL);
}

void ZrCore_TypeLayout_InitUnionWithContract(SZrTypeLayout *layout,
                                             TZrUInt32 byteSize,
                                             TZrUInt32 byteAlign,
                                             TZrUInt32 tagOffset,
                                             TZrUInt32 tagSize,
                                             EZrTypeLayoutCopyKind copyKind,
                                             EZrTypeLayoutDropKind dropKind,
                                             const SZrTypeLayoutField *fields,
                                             TZrUInt32 fieldCount,
                                             const SZrTypeLayoutContract *contract) {
    TZrBool hasExplicitDomainTransfer;

    ZrCore_TypeLayout_InitStructWithContract(layout,
                                             byteSize,
                                             byteAlign,
                                             copyKind,
                                             dropKind,
                                             fields,
                                             fieldCount,
                                             contract);
    if (layout == ZR_NULL) {
        return;
    }

    layout->kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION;
    layout->tagOffset = tagOffset;
    layout->tagSize = tagSize;
    layout->blittable = type_layout_is_blittable(copyKind,
                                                 dropKind,
                                                 layout->gcFieldCount,
                                                 layout->ownershipFieldCount,
                                                 layout->refFieldCount);
    hasExplicitDomainTransfer = (TZrBool)(
            contract != ZR_NULL && contract->hasDomainTransferContract);
    if (!hasExplicitDomainTransfer &&
        layout->domainTransferKind ==
                (TZrUInt8)ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY) {
        layout->domainTransferSchemaHash = 0u;
        layout->domainTransferSchemaHash = ZrCore_TypeLayout_ComputeHash(layout);
        if (layout->domainTransferSchemaHash == 0u) {
            layout->domainTransferSchemaHash = 1u;
        }
    }
    layout->layoutHash = ZrCore_TypeLayout_ComputeHash(layout);
}
