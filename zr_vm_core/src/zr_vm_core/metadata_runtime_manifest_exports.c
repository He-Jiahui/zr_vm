#include "zr_vm_core/metadata_runtime.h"

#include <string.h>

#include "zr_vm_core/memory.h"

/* 失败路径统一清空借用视图，避免调用方复用上一次命中的条目。 */
static void metadata_runtime_clear_manifest_export_view(SZrMetadataRuntimeManifestExportView *outView) {
    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
}

/* 导出查询只做非零 token 与表种类的最低限度筛选。 */
/* TODO: 这里和成员谓词未检查 RID 非零；公开 AttachMetadataRuntime 可原样接收注册表。
 * 需对照 AOT 描述符/ZRP 行验证契约，以零 RID 直接挂载用例确认查询应否拒绝。 */
static TZrBool metadata_runtime_manifest_export_type_token_is_valid(TZrMetadataToken token) {
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(token);
    return (TZrBool)(token != 0u &&
                     (table == ZR_METADATA_TABLE_TYPE_DEF ||
                      table == ZR_METADATA_TABLE_TYPE_SPEC));
}

/* 方法和字段共享 MEMBER_DEF 表，具体 kind 由导出条目提供。 */
static TZrBool metadata_runtime_manifest_export_member_token_is_valid(TZrMetadataToken token) {
    return (TZrBool)(token != 0u && ZR_METADATA_TOKEN_TABLE(token) == ZR_METADATA_TABLE_MEMBER_DEF);
}

/* 匹配前检查 kind、已知 flag 和该 kind 所需 token，供绑定门禁使用。 */
/* TODO: 这里只要求所需 flag 存在，没有排斥另一类 flag/token；需核公开注册表
 * 是否允许双身份条目，并与 AOT 描述符及 ZRP 行验证器的规范化结果对照。 */
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

/* 按种类和目标名查找唯一导出；成功返回依附 codeRegistration 的借用条目。
 * 所有失败出口使 outView 归零，重名条目即使 token 相同也视为歧义。
 */
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

    /* 首次命中后继续扫描，遇第二个同键条目立即拒绝。 */
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
