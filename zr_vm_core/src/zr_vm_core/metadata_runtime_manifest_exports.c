#include "zr_vm_core/metadata_runtime.h"

#include <string.h>

#include "zr_vm_core/memory.h"

static void metadata_runtime_clear_manifest_export_view(SZrMetadataRuntimeManifestExportView *outView) {
    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
}

static TZrBool metadata_runtime_manifest_export_type_token_is_valid(TZrMetadataToken token) {
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(token);
    return (TZrBool)(token != 0u &&
                     (table == ZR_METADATA_TABLE_TYPE_DEF ||
                      table == ZR_METADATA_TABLE_TYPE_SPEC));
}

static TZrBool metadata_runtime_manifest_export_member_token_is_valid(TZrMetadataToken token) {
    return (TZrBool)(token != 0u && ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_MEMBER_DEF);
}

static TZrBool metadata_runtime_manifest_export_entry_shape_is_valid(
        const SZrAotManifestExportEntry *entry,
        TZrUInt32 expectedKind) {
    if (entry == ZR_NULL ||
        entry->target == ZR_NULL ||
        entry->kind != expectedKind ||
        (entry->flags & ~ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_KNOWN_MASK) != 0u) {
        return ZR_FALSE;
    }

    switch (entry->kind) {
        case ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE:
            return (TZrBool)((entry->flags & ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_TYPE_TOKEN) != 0u &&
                             metadata_runtime_manifest_export_type_token_is_valid(entry->typeToken));

        case ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD:
        case ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD:
            return (TZrBool)((entry->flags & ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN) != 0u &&
                             metadata_runtime_manifest_export_member_token_is_valid(entry->memberToken));

        default:
            return ZR_FALSE;
    }
}

TZrBool ZrCore_MetadataRuntime_ReadManifestExportView(
        SZrMetadataRuntime *runtime,
        TZrUInt32 kind,
        const TZrChar *target,
        SZrMetadataRuntimeManifestExportView *outView) {
    const SZrAotManifestExportEntry *matchedEntry = ZR_NULL;
    TZrUInt32 matchedIndex = 0u;

    metadata_runtime_clear_manifest_export_view(outView);
    if (runtime == ZR_NULL ||
        target == ZR_NULL ||
        outView == ZR_NULL ||
        runtime->manifestExports == ZR_NULL ||
        runtime->manifestExportCount == 0u) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < runtime->manifestExportCount; ++index) {
        const SZrAotManifestExportEntry *entry = &runtime->manifestExports[index];

        if (entry->kind != kind ||
            entry->target == ZR_NULL ||
            strcmp(entry->target, target) != 0) {
            continue;
        }

        if (matchedEntry != ZR_NULL ||
            !metadata_runtime_manifest_export_entry_shape_is_valid(entry, kind)) {
            metadata_runtime_clear_manifest_export_view(outView);
            return ZR_FALSE;
        }

        matchedEntry = entry;
        matchedIndex = index;
    }

    if (matchedEntry == ZR_NULL) {
        return ZR_FALSE;
    }

    outView->entry = matchedEntry;
    outView->index = matchedIndex;
    outView->kind = matchedEntry->kind;
    outView->target = matchedEntry->target;
    outView->typeToken = matchedEntry->typeToken;
    outView->memberToken = matchedEntry->memberToken;
    return ZR_TRUE;
}
