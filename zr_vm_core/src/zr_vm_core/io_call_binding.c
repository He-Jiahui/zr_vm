#include "zr_vm_core/io.h"

#include <string.h>

#include "artifact_schema_internal.h"

TZrBool ZrCore_Io_ReadCallBindings(SZrIo *io, SZrIoFunction *function) {
    TZrByte header[16];
    TZrUInt32 count;
    if (io == ZR_NULL || function == ZR_NULL ||
        ZrCore_Io_Read(io, header, sizeof(header)) != sizeof(header)) return ZR_FALSE;
    count = zr_artifact_read_u32(header + 12u);
    if (zr_artifact_read_u32(header) != ZR_CALL_BINDING_SECTION_MAGIC ||
        zr_artifact_read_u32(header + 4u) != ZR_CALL_BINDING_SCHEMA_VERSION ||
        zr_artifact_read_u32(header + 8u) != ZR_CALL_BINDING_SECTION_ROW_SIZE ||
        count > function->callSiteCacheLength ||
        (function->callSiteCacheLength != 0u && function->callSiteCaches == ZR_NULL)) return ZR_FALSE;
    for (TZrSize index = 0u; index < function->callSiteCacheLength; ++index) {
        memset(&function->callSiteCaches[index].bindingContract, 0, sizeof(SZrCallBindingContract));
        memset(&function->callSiteCaches[index].bindingLocation, 0, sizeof(SZrCallBindingLocation));
    }
    for (TZrUInt32 index = 0u; index < count; ++index) {
        TZrByte row[ZR_CALL_BINDING_SECTION_ROW_SIZE];
        TZrUInt32 cacheIndex;
        SZrIoFunctionCallSiteCacheEntry *entry;
        if (ZrCore_Io_Read(io, row, sizeof(row)) != sizeof(row)) return ZR_FALSE;
        cacheIndex = zr_artifact_read_u32(row);
        if (cacheIndex >= function->callSiteCacheLength) return ZR_FALSE;
        entry = &function->callSiteCaches[cacheIndex];
        if (entry->bindingContract.bindingKind != ZR_CALL_BINDING_NONE ||
            !ZrCore_CallBinding_DecodeContract(row + 4u, ZR_CALL_BINDING_CONTRACT_ENCODED_SIZE,
                                              &entry->bindingContract)) return ZR_FALSE;
        entry->bindingLocation.kind = zr_artifact_read_u32(row + 68u);
        entry->bindingLocation.targetIndex = zr_artifact_read_u32(row + 72u);
        entry->bindingLocation.ownerDepth = zr_artifact_read_u32(row + 76u);
        entry->bindingLocation.flags = zr_artifact_read_u32(row + 80u);
        if (((entry->bindingContract.bindingKind != ZR_CALL_BINDING_TYPED_FUNCTION &&
              (entry->bindingLocation.kind < ZR_CALL_BINDING_RELOCATION_CONSTANT ||
               entry->bindingLocation.kind > ZR_CALL_BINDING_RELOCATION_VM_MODULE)) ||
             (entry->bindingContract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION &&
              (entry->bindingLocation.kind != ZR_CALL_BINDING_RELOCATION_NONE ||
               entry->bindingLocation.targetIndex != ZR_CALL_BINDING_SLOT_NONE ||
               entry->bindingLocation.ownerDepth != 0u))) ||
            entry->bindingLocation.flags != 0u) return ZR_FALSE;
    }
    return ZR_TRUE;
}
