#include "zr_vm_core/metadata_runtime.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"

/**
 * @brief 在已挂载的 ZRP 定义表中按完整 token 定位 TypeDef 行。
 * @note 供布局绑定视图读取器复用；表缺失、记录尺寸不符或 token 无匹配时统一失败，
 *       不从运行时类型记录推造定义行。
 */
static const SZrZrpMetadataTypeDefRow *metadata_runtime_find_type_def_row(SZrMetadataRuntime *runtime,
                                                                          TZrMetadataToken typeDefToken) {
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataTypeDefRow *rows;
    TZrUInt32 index;

    if (runtime == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeDefToken) != ZR_METADATA_TABLE_TYPE_DEF ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_TYPE_DEFS, &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow)) {
        return ZR_NULL;
    }

    rows = (const SZrZrpMetadataTypeDefRow *)(const void *)view.data;
    for (index = 0u; index < view.count; ++index) {
        if (rows[index].token == typeDefToken) {
            return &rows[index];
        }
    }

    return ZR_NULL;
}

/**
 * @brief 按完整 TypeSpec token 读取 ZRP 行，作为签名与布局关联的可信行来源。
 * @note 仅由 TypeSpec 布局视图调用；行不存在或 section ABI 尺寸不符时拒绝绑定。
 */
static const SZrZrpMetadataTypeSpecRow *metadata_runtime_find_type_spec_row(SZrMetadataRuntime *runtime,
                                                                            TZrMetadataToken typeSpecToken) {
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataTypeSpecRow *rows;
    TZrUInt32 index;

    if (runtime == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeSpecToken) != ZR_METADATA_TABLE_TYPE_SPEC ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_TYPE_SPECS, &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow)) {
        return ZR_NULL;
    }

    rows = (const SZrZrpMetadataTypeSpecRow *)(const void *)view.data;
    for (index = 0u; index < view.count; ++index) {
        if (rows[index].token == typeSpecToken) {
            return &rows[index];
        }
    }

    return ZR_NULL;
}

/**
 * @brief 定位 FieldDef 行并返回其在字段表中的位置，供 owner 区间校验使用。
 * @note outRowIndex 可选；未命中时写入无效哨兵，避免调用者把缺失字段当作行零。
 */
static const SZrZrpMetadataFieldDefRow *metadata_runtime_find_field_def_row(SZrMetadataRuntime *runtime,
                                                                            TZrMetadataToken fieldDefToken,
                                                                            TZrUInt32 *outRowIndex) {
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataFieldDefRow *rows;
    TZrUInt32 index;

    if (outRowIndex != ZR_NULL) {
        *outRowIndex = ~(TZrUInt32)0u;
    }
    if (runtime == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(fieldDefToken) != ZR_METADATA_TABLE_MEMBER_DEF ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_FIELD_DEFS, &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow)) {
        return ZR_NULL;
    }

    rows = (const SZrZrpMetadataFieldDefRow *)(const void *)view.data;
    for (index = 0u; index < view.count; ++index) {
        if (rows[index].token == fieldDefToken) {
            if (outRowIndex != ZR_NULL) {
                *outRowIndex = index;
            }
            return &rows[index];
        }
    }

    return ZR_NULL;
}

/**
 * @brief 检查 FieldDef 行索引是否属于指定 TypeDef 声明的连续字段区间。
 * @note 使用减法形式比较区间，避免 firstFieldIndex + fieldCount 溢出。
 */
static TZrBool metadata_runtime_type_def_contains_field_index(const SZrZrpMetadataTypeDefRow *typeDefRow,
                                                              TZrUInt32 fieldRowIndex) {
    TZrUInt32 firstFieldIndex;
    TZrUInt32 fieldCount;

    if (typeDefRow == ZR_NULL) {
        return ZR_FALSE;
    }

    firstFieldIndex = typeDefRow->firstFieldDefIndex;
    fieldCount = typeDefRow->fieldDefCount;
    return fieldRowIndex >= firstFieldIndex &&
           fieldRowIndex - firstFieldIndex < fieldCount;
}

/**
 * @brief 查询本 runtime 最近成功建立的 token 到布局缓存项。
 * @note 缓存只加速绑定视图已验证的结果；miss 必须回到元数据和注册表验证路径。
 */
static const SZrTypeLayout *metadata_runtime_find_type_layout_cache_by_token(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeToken,
        TZrUInt32 *outTypeLayoutId) {
    TZrUInt32 index;

    if (runtime == ZR_NULL || typeToken == 0u) {
        return ZR_NULL;
    }
    for (index = 0u; index < ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY; index++) {
        if (runtime->typeLayoutCacheTokens[index] == typeToken &&
            runtime->typeLayoutCacheLayouts[index] != ZR_NULL) {
            if (outTypeLayoutId != ZR_NULL) {
                *outTypeLayoutId = runtime->typeLayoutCacheIds[index];
            }
            return runtime->typeLayoutCacheLayouts[index];
        }
    }
    return ZR_NULL;
}

/**
 * @brief 以布局 ID 反查有界缓存中的元数据 token。
 * @note 命中要求 token、布局指针和 ID 三者同属有效缓存项；缓存不是全量索引。
 */
static TZrMetadataToken metadata_runtime_find_type_layout_cache_by_id(
        SZrMetadataRuntime *runtime,
        TZrUInt32 typeLayoutId) {
    TZrUInt32 index;

    if (runtime == ZR_NULL || typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return 0u;
    }
    for (index = 0u; index < ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY; index++) {
        if (runtime->typeLayoutCacheIds[index] == typeLayoutId &&
            runtime->typeLayoutCacheTokens[index] != 0u &&
            runtime->typeLayoutCacheLayouts[index] != ZR_NULL) {
            return runtime->typeLayoutCacheTokens[index];
        }
    }
    return 0u;
}

/**
 * @brief 保存已验证的双向 token/layout 关联，供后续反射查询复用。
 * @note 缓存容量固定；满时轮换淘汰最旧槽位，因此 miss 仍须可通过注册表或 ZRP 行重建。
 */
static void metadata_runtime_store_type_layout_cache(SZrMetadataRuntime *runtime,
                                                     TZrMetadataToken typeToken,
                                                     TZrUInt32 typeLayoutId,
                                                     const SZrTypeLayout *typeLayout) {
    TZrUInt32 index;

    if (runtime == ZR_NULL ||
        typeToken == 0u ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        typeLayout == ZR_NULL) {
        return;
    }

    for (index = 0u; index < ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY; index++) {
        if (runtime->typeLayoutCacheTokens[index] == typeToken ||
            runtime->typeLayoutCacheTokens[index] == 0u) {
            runtime->typeLayoutCacheTokens[index] = typeToken;
            runtime->typeLayoutCacheIds[index] = typeLayoutId;
            runtime->typeLayoutCacheLayouts[index] = typeLayout;
            return;
        }
    }

    index = runtime->typeLayoutCacheNextIndex % ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY;
    runtime->typeLayoutCacheTokens[index] = typeToken;
    runtime->typeLayoutCacheIds[index] = typeLayoutId;
    runtime->typeLayoutCacheLayouts[index] = typeLayout;
    runtime->typeLayoutCacheNextIndex =
            (index + 1u) % ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY;
}

/**
 * @brief 限定代码注册 token 表可发布的布局 token 种类。
 * @note TypeRef 需要跨记录身份校验，不能作为注册表直接提供的独立 TypeDef/TypeSpec 项。
 */
static TZrBool metadata_runtime_is_layout_type_token(TZrMetadataToken typeToken) {
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(typeToken);
    return (TZrBool)(table == ZR_METADATA_TABLE_TYPE_DEF ||
                     table == ZR_METADATA_TABLE_TYPE_SPEC);
}

/**
 * @brief 验证 TypeRef 声明的目标及可选模块、签名和布局身份均指向当前 TypeDef 视图。
 * @note 任一非零身份约束不匹配即拒绝绑定，避免仅凭目标 token 将外部引用误当成本地布局。
 */
static TZrBool metadata_runtime_type_ref_matches_type_def_layout(
        SZrMetadataRuntime *runtime,
        const SZrMetadataTokenRecord *typeRefRecord,
        const SZrMetadataRuntimeTypeDefLayoutBindingView *typeDefView) {
    if (typeRefRecord == ZR_NULL ||
        typeDefView == ZR_NULL ||
        typeDefView->typeRecord == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeRefRecord->token) != ZR_METADATA_TABLE_TYPE_REF ||
        ZR_METADATA_TOKEN_TABLE(typeRefRecord->targetMetadataToken) != ZR_METADATA_TABLE_TYPE_DEF ||
        typeRefRecord->targetMetadataToken != typeDefView->typeDefToken) {
        return ZR_FALSE;
    }

    if (typeRefRecord->targetModuleSignatureHash != 0u &&
        (runtime == ZR_NULL ||
         runtime->metadataFunction == ZR_NULL ||
         runtime->metadataFunction->moduleSignatureHash != typeRefRecord->targetModuleSignatureHash)) {
        return ZR_FALSE;
    }
    if (typeRefRecord->targetSignatureToken != 0u &&
        typeRefRecord->targetSignatureToken != typeDefView->typeRecord->relatedToken) {
        return ZR_FALSE;
    }
    if (typeRefRecord->targetSignatureHash != 0u &&
        typeRefRecord->targetSignatureHash != typeDefView->typeRecord->signatureHash) {
        return ZR_FALSE;
    }
    if ((typeRefRecord->layoutVersion != 0u ||
         typeRefRecord->layoutHash != 0u ||
         typeDefView->layoutVersion != 0u ||
         typeDefView->layoutHash != 0u) &&
        (typeRefRecord->layoutVersion != typeDefView->layoutVersion ||
         typeRefRecord->layoutHash != typeDefView->layoutHash)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/**
 * @brief 将已解析的 TypeRef 逐项校验后转为目标 TypeDef 布局视图。
 * @note 失败时清零输出，调用者不得使用部分填充的视图；target token 必须可在当前 runtime 解析。
 */
static TZrBool metadata_runtime_read_type_ref_target_type_def_layout(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeRefToken,
        SZrMetadataRuntimeTypeDefLayoutBindingView *outView) {
    const SZrMetadataTokenRecord *typeRefRecord;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (runtime == ZR_NULL ||
        outView == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeRefToken) != ZR_METADATA_TABLE_TYPE_REF) {
        return ZR_FALSE;
    }

    typeRefRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, typeRefToken);
    if (typeRefRecord == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeRefRecord->targetMetadataToken) != ZR_METADATA_TABLE_TYPE_DEF ||
        !ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(runtime,
                                                             typeRefRecord->targetMetadataToken,
                                                             outView) ||
        outView->typeLayout == ZR_NULL ||
        !metadata_runtime_type_ref_matches_type_def_layout(runtime, typeRefRecord, outView)) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/**
 * @brief 从 AOT 注册表的并行 token 表解析布局 ID，并缓存通过注册表解析的布局关系。
 * @note token 表仅接受 TypeDef/TypeSpec，条目越界、类型不符或 layout 不可解析均视为缺失。
 */
static TZrMetadataToken metadata_runtime_resolve_registration_type_layout_token(
        SZrMetadataRuntime *runtime,
        TZrUInt32 typeLayoutId) {
    const SZrTypeLayout *typeLayout;
    TZrMetadataToken typeToken;

    if (runtime == ZR_NULL ||
        runtime->codeRegistration == ZR_NULL ||
        runtime->codeRegistration->typeLayoutTokens == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        typeLayoutId >= runtime->typeLayoutTokenCount) {
        return 0u;
    }

    typeToken = runtime->codeRegistration->typeLayoutTokens[typeLayoutId];
    if (typeToken == 0u || !metadata_runtime_is_layout_type_token(typeToken)) {
        return 0u;
    }

    typeLayout = ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, typeLayoutId);
    if (typeLayout == ZR_NULL) {
        return 0u;
    }

    metadata_runtime_store_type_layout_cache(runtime, typeToken, typeLayoutId, typeLayout);
    return typeToken;
}

/**
 * @brief 建立 TypeDef token、ZRP 行、代码注册布局及身份版本的一致视图。
 * @pre runtime 已附加同一模块的 token 元数据、ZRP 定义表和 AOT layout registry。
 * @return 全部来源存在且布局版本/哈希兼容时填充视图并返回 true；否则视图保持清零。
 * @note 返回指针借用 runtime 所挂载的数据，不能跨卸载或更换注册表后继续使用。
 */
TZrBool ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeDefToken,
        SZrMetadataRuntimeTypeDefLayoutBindingView *outView) {
    const SZrMetadataTokenRecord *typeRecord;
    const SZrZrpMetadataTypeDefRow *typeDefRow;
    const SZrTypeLayout *typeLayout;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (runtime == ZR_NULL ||
        outView == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeDefToken) != ZR_METADATA_TABLE_TYPE_DEF) {
        return ZR_FALSE;
    }

    typeRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, typeDefToken);
    typeDefRow = metadata_runtime_find_type_def_row(runtime, typeDefToken);
    if (typeRecord == ZR_NULL ||
        typeDefRow == ZR_NULL ||
        typeDefRow->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    typeLayout = ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, typeDefRow->typeLayoutId);
    if (typeLayout == ZR_NULL ||
        (typeLayout->layoutVersion != 0u &&
         (typeRecord->layoutVersion != typeLayout->layoutVersion ||
          typeRecord->layoutHash != typeLayout->layoutHash))) {
        return ZR_FALSE;
    }

    outView->typeDefToken = typeDefToken;
    outView->typeRecord = typeRecord;
    outView->typeDefRow = typeDefRow;
    outView->typeLayoutId = typeDefRow->typeLayoutId;
    outView->cTypeId = typeDefRow->typeLayoutId;
    outView->layoutVersion = typeRecord->layoutVersion;
    outView->layoutHash = typeRecord->layoutHash;
    outView->typeLayout = typeLayout;
    return ZR_TRUE;
}

/**
 * @brief 将 TypeSpec 行与 token/signature 身份、泛型基类型绑定及注册布局合并为只读视图。
 * @pre runtime 的 ZRP TypeSpec、签名记录与 AOT layout registry 必须来自同一份模块产物。
 * @return 任一关联身份不一致、泛型绑定无效或 layout 缺失时失败且不发布半成品视图。
 * @note 返回的记录和布局借用 runtime 数据；该接口不负责实例化泛型布局。
 */
TZrBool ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        SZrMetadataRuntimeTypeSpecLayoutBindingView *outView) {
    const SZrMetadataTokenRecord *typeRecord;
    const SZrMetadataTokenRecord *signatureRecord;
    const SZrZrpMetadataTypeSpecRow *typeSpecRow;
    const SZrTypeLayout *typeLayout;
    SZrMetadataRuntimeTypeSpecGenericBindingView genericBindingView;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (runtime == ZR_NULL ||
        outView == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeSpecToken) != ZR_METADATA_TABLE_TYPE_SPEC) {
        return ZR_FALSE;
    }

    typeRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, typeSpecToken);
    signatureRecord = ZrCore_MetadataRuntime_ResolveSignatureRecord(runtime, typeSpecToken);
    typeSpecRow = metadata_runtime_find_type_spec_row(runtime, typeSpecToken);
    if (typeRecord == ZR_NULL ||
        signatureRecord == ZR_NULL ||
        typeSpecRow == ZR_NULL ||
        typeSpecRow->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        typeSpecRow->signatureBlobOffset != signatureRecord->signatureBlobOffset ||
        typeSpecRow->signatureBlobLength != signatureRecord->signatureBlobLength ||
        typeSpecRow->signatureHash != signatureRecord->signatureHash ||
        typeSpecRow->signatureHash != typeRecord->signatureHash ||
        !ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView(runtime,
                                                               typeSpecToken,
                                                               &genericBindingView)) {
        return ZR_FALSE;
    }

    typeLayout = ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, typeSpecRow->typeLayoutId);
    if (typeLayout == ZR_NULL ||
        (typeLayout->layoutVersion != 0u &&
         (typeRecord->layoutVersion != typeLayout->layoutVersion ||
          typeRecord->layoutHash != typeLayout->layoutHash))) {
        return ZR_FALSE;
    }

    outView->typeSpecToken = typeSpecToken;
    outView->typeRecord = typeRecord;
    outView->typeSpecRow = typeSpecRow;
    outView->genericBindingView = genericBindingView;
    outView->typeLayoutId = typeSpecRow->typeLayoutId;
    outView->cTypeId = typeSpecRow->typeLayoutId;
    outView->signatureHash = typeSpecRow->signatureHash;
    outView->typeLayout = typeLayout;
    return ZR_TRUE;
}

/**
 * @brief 将 FieldDef 的偏移/类型布局绑定到其声明的 TypeDef owner。
 * @pre runtime 已挂载字段表、owner 类型行与注册布局；FieldDef 必须落在 owner 的字段区间内。
 * @return 字段、owner 或任一布局无法一致解析时失败，调用方不应据此访问对象内存。
 * @note 只提供经过结构归属校验的布局元数据，不验证某个具体对象的可读写权限。
 */
TZrBool ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken fieldDefToken,
        SZrMetadataRuntimeFieldDefLayoutBindingView *outView) {
    const SZrMetadataTokenRecord *fieldRecord;
    const SZrMetadataTokenRecord *ownerTypeRecord;
    const SZrZrpMetadataFieldDefRow *fieldDefRow;
    const SZrZrpMetadataTypeDefRow *ownerTypeDefRow;
    const SZrTypeLayout *fieldTypeLayout;
    const SZrTypeLayout *ownerTypeLayout;
    TZrUInt32 fieldRowIndex = ~(TZrUInt32)0u;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (runtime == ZR_NULL ||
        outView == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(fieldDefToken) != ZR_METADATA_TABLE_MEMBER_DEF) {
        return ZR_FALSE;
    }

    fieldRecord = ZrCore_MetadataRuntime_ResolveFieldRecord(runtime, fieldDefToken);
    fieldDefRow = metadata_runtime_find_field_def_row(runtime, fieldDefToken, &fieldRowIndex);
    if (fieldRecord == ZR_NULL ||
        fieldDefRow == ZR_NULL ||
        fieldDefRow->ownerTypeToken == 0u ||
        ZR_METADATA_TOKEN_TABLE(fieldDefRow->ownerTypeToken) != ZR_METADATA_TABLE_TYPE_DEF ||
        fieldDefRow->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_FALSE;
    }

    ownerTypeRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, fieldDefRow->ownerTypeToken);
    ownerTypeDefRow = metadata_runtime_find_type_def_row(runtime, fieldDefRow->ownerTypeToken);
    if (ownerTypeRecord == ZR_NULL ||
        ownerTypeDefRow == ZR_NULL ||
        ownerTypeDefRow->typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        !metadata_runtime_type_def_contains_field_index(ownerTypeDefRow, fieldRowIndex)) {
        return ZR_FALSE;
    }

    fieldTypeLayout = ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, fieldDefRow->typeLayoutId);
    ownerTypeLayout = ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, ownerTypeDefRow->typeLayoutId);
    if (fieldTypeLayout == ZR_NULL || ownerTypeLayout == ZR_NULL) {
        return ZR_FALSE;
    }

    outView->fieldDefToken = fieldDefToken;
    outView->fieldRecord = fieldRecord;
    outView->fieldDefRow = fieldDefRow;
    outView->ownerTypeToken = fieldDefRow->ownerTypeToken;
    outView->ownerTypeRecord = ownerTypeRecord;
    outView->ownerTypeDefRow = ownerTypeDefRow;
    outView->byteOffset = fieldDefRow->byteOffset;
    outView->fieldTypeLayoutId = fieldDefRow->typeLayoutId;
    outView->ownerTypeLayoutId = ownerTypeDefRow->typeLayoutId;
    outView->fieldTypeLayout = fieldTypeLayout;
    outView->ownerTypeLayout = ownerTypeLayout;
    return ZR_TRUE;
}

/**
 * @brief 为反射等运行时消费者解析 TypeDef、TypeSpec 或受约束 TypeRef 的布局。
 * @pre runtime 的元数据与 AOT 注册布局属于同一模块装载实例。
 * @return 成功返回借用的布局指针并可选写出 layout ID；失败返回 NULL 且 ID 为无效哨兵。
 * @note 先查有界缓存，miss 时必须通过对应 binding view；不以 prototype frame 布局兜底。
 * BUG: 成功重新调用 AttachZrpMetadata 会替换 ZRP 行但不清除此缓存；相同 token 的后续命中会绕过新行校验并返回旧布局。
 */
const SZrTypeLayout *ZrCore_MetadataRuntime_ResolveTypeTokenLayout(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeToken,
        TZrUInt32 *outTypeLayoutId) {
    const SZrTypeLayout *typeLayout = ZR_NULL;
    TZrUInt32 typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;

    if (outTypeLayoutId != ZR_NULL) {
        *outTypeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    }
    if (runtime == ZR_NULL || typeToken == 0u) {
        return ZR_NULL;
    }

    typeLayout = metadata_runtime_find_type_layout_cache_by_token(runtime, typeToken, &typeLayoutId);
    if (typeLayout != ZR_NULL) {
        if (outTypeLayoutId != ZR_NULL) {
            *outTypeLayoutId = typeLayoutId;
        }
        return typeLayout;
    }

    switch (ZR_METADATA_TOKEN_TABLE(typeToken)) {
        case ZR_METADATA_TABLE_TYPE_DEF: {
            SZrMetadataRuntimeTypeDefLayoutBindingView view;
            if (!ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(runtime, typeToken, &view) ||
                view.typeLayout == ZR_NULL) {
                return ZR_NULL;
            }
            typeLayoutId = view.typeLayoutId;
            typeLayout = view.typeLayout;
            break;
        }

        case ZR_METADATA_TABLE_TYPE_SPEC: {
            SZrMetadataRuntimeTypeSpecLayoutBindingView view;
            if (!ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(runtime, typeToken, &view)) {
                return ZR_NULL;
            }
            typeLayoutId = view.typeLayoutId;
            typeLayout = view.typeLayout;
            break;
        }

        case ZR_METADATA_TABLE_TYPE_REF: {
            SZrMetadataRuntimeTypeDefLayoutBindingView view;
            if (!metadata_runtime_read_type_ref_target_type_def_layout(runtime, typeToken, &view)) {
                return ZR_NULL;
            }
            typeLayoutId = view.typeLayoutId;
            typeLayout = view.typeLayout;
            break;
        }

        default:
            return ZR_NULL;
    }

    metadata_runtime_store_type_layout_cache(runtime, typeToken, typeLayoutId, typeLayout);
    if (outTypeLayoutId != ZR_NULL) {
        *outTypeLayoutId = typeLayoutId;
    }
    return typeLayout;
}

/**
 * @brief 注册表 token 表未提供可用项时，在 TypeDef 行中重建指定布局 ID 的有效关联。
 * @note 每个候选都重新走公开绑定视图；不接受只有行号相同但记录/布局已失配的结果。
 */
static TZrMetadataToken metadata_runtime_find_type_def_token_for_layout_id(
        SZrMetadataRuntime *runtime,
        TZrUInt32 typeLayoutId,
        const SZrTypeLayout **outTypeLayout) {
    SZrZrpMetadataSectionView sectionView;
    const SZrZrpMetadataTypeDefRow *rows;
    TZrUInt32 index;

    if (outTypeLayout != ZR_NULL) {
        *outTypeLayout = ZR_NULL;
    }
    if (runtime == ZR_NULL ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_TYPE_DEFS, &sectionView) ||
        sectionView.data == ZR_NULL ||
        sectionView.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow)) {
        return 0u;
    }

    rows = (const SZrZrpMetadataTypeDefRow *)sectionView.data;
    for (index = 0u; index < sectionView.count; index++) {
        SZrMetadataRuntimeTypeDefLayoutBindingView bindingView;
        if (rows[index].typeLayoutId != typeLayoutId) {
            continue;
        }
        if (ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(runtime, rows[index].token, &bindingView) &&
            bindingView.typeLayoutId == typeLayoutId &&
            bindingView.typeLayout != ZR_NULL) {
            if (outTypeLayout != ZR_NULL) {
                *outTypeLayout = bindingView.typeLayout;
            }
            return bindingView.typeDefToken;
        }
    }
    return 0u;
}

/**
 * @brief 反查 TypeSpec 行并通过完整 TypeSpec 绑定视图确认布局 ID。
 * @note 这是无 token 表命中时的慢路径；它不会合成缺失的泛型实例布局。
 */
static TZrMetadataToken metadata_runtime_find_type_spec_token_for_layout_id(
        SZrMetadataRuntime *runtime,
        TZrUInt32 typeLayoutId,
        const SZrTypeLayout **outTypeLayout) {
    SZrZrpMetadataSectionView sectionView;
    const SZrZrpMetadataTypeSpecRow *rows;
    TZrUInt32 index;

    if (outTypeLayout != ZR_NULL) {
        *outTypeLayout = ZR_NULL;
    }
    if (runtime == ZR_NULL ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_TYPE_SPECS, &sectionView) ||
        sectionView.data == ZR_NULL ||
        sectionView.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow)) {
        return 0u;
    }

    rows = (const SZrZrpMetadataTypeSpecRow *)sectionView.data;
    for (index = 0u; index < sectionView.count; index++) {
        SZrMetadataRuntimeTypeSpecLayoutBindingView bindingView;
        if (rows[index].typeLayoutId != typeLayoutId) {
            continue;
        }
        if (ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(runtime, rows[index].token, &bindingView) &&
            bindingView.typeLayoutId == typeLayoutId &&
            bindingView.typeLayout != ZR_NULL) {
            if (outTypeLayout != ZR_NULL) {
                *outTypeLayout = bindingView.typeLayout;
            }
            return bindingView.typeSpecToken;
        }
    }
    return 0u;
}

/**
 * @brief 将已注册布局 ID 反向解析为其 TypeDef/TypeSpec token。
 * @pre layout ID 属于当前 runtime 的代码注册表，且 token 表或 ZRP 行提供可验证关联。
 * @return 无效 ID、缺失布局或无可验证 token 时返回 0；返回 token 不转移任何数据所有权。
 * @note 注册表 token carrier 优先，其次按 TypeDef、TypeSpec 行扫描；相同布局的缓存只保留有限命中。
 */
TZrMetadataToken ZrCore_MetadataRuntime_ResolveTypeLayoutToken(
        SZrMetadataRuntime *runtime,
        TZrUInt32 typeLayoutId) {
    const SZrTypeLayout *typeLayout = ZR_NULL;
    TZrMetadataToken typeToken;

    if (runtime == ZR_NULL || typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return 0u;
    }
    typeToken = metadata_runtime_find_type_layout_cache_by_id(runtime, typeLayoutId);
    if (typeToken != 0u) {
        return typeToken;
    }

    typeToken = metadata_runtime_resolve_registration_type_layout_token(runtime, typeLayoutId);
    if (typeToken != 0u) {
        return typeToken;
    }

    typeToken = metadata_runtime_find_type_def_token_for_layout_id(runtime, typeLayoutId, &typeLayout);
    if (typeToken == 0u) {
        typeToken = metadata_runtime_find_type_spec_token_for_layout_id(runtime, typeLayoutId, &typeLayout);
    }
    if (typeToken == 0u || typeLayout == ZR_NULL) {
        return 0u;
    }

    metadata_runtime_store_type_layout_cache(runtime, typeToken, typeLayoutId, typeLayout);
    return typeToken;
}

/**
 * @brief 以生成 C 使用的 cTypeId 名义反查元数据类型 token。
 * @pre 当前 ABI 约定 cTypeId 与 typeLayoutId 同值；调用方不可将其解释为独立编号空间。
 * @return 沿用布局 ID 反查语义，无法验证映射时返回 0。
 * TODO: 若 ABI 允许 cTypeId 与 typeLayoutId 分离，需增加显式映射而非继续复用此别名。
 */
TZrMetadataToken ZrCore_MetadataRuntime_ResolveCTypeIdToken(
        SZrMetadataRuntime *runtime,
        TZrUInt32 cTypeId) {
    TZrMetadataToken typeToken;

    if (runtime == ZR_NULL || cTypeId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return 0u;
    }

    typeToken = metadata_runtime_find_type_layout_cache_by_id(runtime, cTypeId);
    if (typeToken != 0u) {
        return typeToken;
    }

    typeToken = metadata_runtime_resolve_registration_type_layout_token(runtime, cTypeId);
    if (typeToken != 0u) {
        return typeToken;
    }

    return ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, cTypeId);
}
