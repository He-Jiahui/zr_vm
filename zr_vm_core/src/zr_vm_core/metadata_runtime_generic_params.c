#include "zr_vm_core/metadata_runtime.h"

#include "zr_vm_core/memory.h"

/**
 * @brief 按完整 TypeDef token 查找泛型类型所属行。
 * @pre runtime 已附着可读取的 ZRP metadata；返回指针借用该运行时中的表存储。
 * @note 这里只接受 TypeDef 表 token，避免把同数值的其他 token 当成类型所有者。
 */
static const SZrZrpMetadataTypeDefRow *metadata_runtime_generic_find_type_def_row(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeDefToken) {
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataTypeDefRow *rows;

    if (runtime == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeDefToken) != ZR_METADATA_TABLE_TYPE_DEF ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_TYPE_DEFS, &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow)) {
        return ZR_NULL;
    }

    rows = (const SZrZrpMetadataTypeDefRow *)(const void *)view.data;
    for (TZrUInt32 index = 0u; index < view.count; index++) {
        if (rows[index].token == typeDefToken) {
            return &rows[index];
        }
    }
    return ZR_NULL;
}

/**
 * @brief 按完整 MethodDef token 查找方法泛型参数所属行。
 * @pre runtime 已附着可读取的 ZRP metadata；返回指针借用该运行时中的表存储。
 * @note MEMBER_DEF 是方法 token 的表编码；解析器还会通过方法记录检查它确实是方法。
 */
static const SZrZrpMetadataMethodDefRow *metadata_runtime_generic_find_method_def_row(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodDefToken) {
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataMethodDefRow *rows;

    if (runtime == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(methodDefToken) != ZR_METADATA_TABLE_MEMBER_DEF ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_METHOD_DEFS, &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow)) {
        return ZR_NULL;
    }

    rows = (const SZrZrpMetadataMethodDefRow *)(const void *)view.data;
    for (TZrUInt32 index = 0u; index < view.count; index++) {
        if (rows[index].token == methodDefToken) {
            return &rows[index];
        }
    }
    return ZR_NULL;
}

/**
 * @brief 建立供参数查询与反射构造共享的 TypeDef/MethodDef 泛型参数区间视图。
 * @pre ownerToken 必须解析为当前 runtime 中有效的 TypeDef 或 MethodDef；输出在失败时清零。
 * @return 成功时同时提供 owner record、定义行及该 owner 在 GenericParam 表中的起始索引和数量。
 * TODO: ZRP 校验了 TypeDef 的 GenericParam 区间，未见 MethodDef 同等检查；确认 attach 是否应拒绝越界范围。
 */
TZrBool ZrCore_MetadataRuntime_ReadGenericOwnerView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken ownerToken,
        SZrMetadataRuntimeGenericOwnerView *outView) {
    const SZrMetadataTokenRecord *ownerRecord;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (runtime == ZR_NULL || ownerToken == 0u || outView == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (ZR_METADATA_TOKEN_TABLE(ownerToken)) {
        case ZR_METADATA_TABLE_TYPE_DEF: {
            const SZrZrpMetadataTypeDefRow *row =
                    metadata_runtime_generic_find_type_def_row(runtime, ownerToken);
            ownerRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, ownerToken);
            if (row == ZR_NULL || ownerRecord == ZR_NULL) {
                return ZR_FALSE;
            }
            outView->ownerToken = ownerToken;
            outView->ownerRecord = ownerRecord;
            outView->typeDefRow = row;
            outView->firstGenericParamIndex = row->firstGenericParamIndex;
            outView->genericParamCount = row->genericParamCount;
            return ZR_TRUE;
        }

        case ZR_METADATA_TABLE_MEMBER_DEF: {
            const SZrZrpMetadataMethodDefRow *row =
                    metadata_runtime_generic_find_method_def_row(runtime, ownerToken);
            ownerRecord = ZrCore_MetadataRuntime_ResolveMethodRecord(runtime, ownerToken);
            if (row == ZR_NULL || ownerRecord == ZR_NULL) {
                return ZR_FALSE;
            }
            outView->ownerToken = ownerToken;
            outView->ownerRecord = ownerRecord;
            outView->methodDefRow = row;
            outView->firstGenericParamIndex = row->firstGenericParamIndex;
            outView->genericParamCount = row->genericParamCount;
            return ZR_TRUE;
        }

        default:
            return ZR_FALSE;
    }
}

/**
 * @brief 判断物理 GenericParam 行是否属于 owner 声明的连续区间。
 * @note 先比较下界再做减法，避免无符号索引下溢；范围只表达存储归属，不代替行内 token 校验。
 */
static TZrBool metadata_runtime_generic_param_index_in_owner_range(
        const SZrMetadataRuntimeGenericOwnerView *range,
        TZrUInt32 genericParamIndex) {
    if (range == ZR_NULL) {
        return ZR_FALSE;
    }
    return (TZrBool)(genericParamIndex >= range->firstGenericParamIndex &&
                     genericParamIndex - range->firstGenericParamIndex < range->genericParamCount);
}

/**
 * @brief 在 owner 的物理区间中按逻辑参数序号定位 GenericParam 行。
 * @pre ownerRange 来自 ReadGenericOwnerView；可选输出索引在查询开始时置为无效哨兵。
 * @note 同时核对行的 ownerToken、parameterIndex 和物理归属，避免损坏或交错的表行被误认。
 * TODO: GenericParam 行验证只检查 ownerToken 的表号/RID，未保证同一 owner 区间的 parameterIndex 唯一；此处返回首个匹配行，确认格式层是否应拒绝重复。
 */
static const SZrZrpMetadataGenericParamRow *metadata_runtime_find_generic_param_row(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken ownerToken,
        TZrUInt32 parameterIndex,
        const SZrMetadataRuntimeGenericOwnerView *ownerRange,
        TZrUInt32 *outGenericParamIndex) {
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataGenericParamRow *rows;

    if (outGenericParamIndex != ZR_NULL) {
        *outGenericParamIndex = ~(TZrUInt32)0u;
    }
    if (runtime == ZR_NULL ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime, ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS, &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow)) {
        return ZR_NULL;
    }

    rows = (const SZrZrpMetadataGenericParamRow *)(const void *)view.data;
    for (TZrUInt32 index = 0u; index < view.count; index++) {
        if (rows[index].ownerToken == ownerToken &&
            rows[index].parameterIndex == parameterIndex &&
            metadata_runtime_generic_param_index_in_owner_range(ownerRange, index)) {
            if (outGenericParamIndex != ZR_NULL) {
                *outGenericParamIndex = index;
            }
            return &rows[index];
        }
    }
    return ZR_NULL;
}

/**
 * @brief 读取一个类型或方法泛型参数的完整运行时视图。
 * @pre ownerToken 必须指向有效 TypeDef 或 MethodDef，parameterIndex 是该 owner 内的零基序号。
 * @return 成功时提供定义行、物理索引、owner record 及名称/约束/标志字段；失败时输出清零。
 * @note 反射解析器和开放泛型方法构造共用此入口，集中执行 owner 区间与参数行归属校验。
 */
TZrBool ZrCore_MetadataRuntime_ReadGenericParamView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken ownerToken,
        TZrUInt32 parameterIndex,
        SZrMetadataRuntimeGenericParamView *outView) {
    SZrMetadataRuntimeGenericOwnerView ownerRange;
    const SZrZrpMetadataGenericParamRow *row;
    TZrUInt32 genericParamIndex = ~(TZrUInt32)0u;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (outView == ZR_NULL ||
        !ZrCore_MetadataRuntime_ReadGenericOwnerView(runtime, ownerToken, &ownerRange)) {
        return ZR_FALSE;
    }

    row = metadata_runtime_find_generic_param_row(runtime,
                                                  ownerToken,
                                                  parameterIndex,
                                                  &ownerRange,
                                                  &genericParamIndex);
    if (row == ZR_NULL) {
        return ZR_FALSE;
    }

    outView->ownerToken = ownerToken;
    outView->ownerRecord = ownerRange.ownerRecord;
    outView->genericParamRow = row;
    outView->genericParamIndex = genericParamIndex;
    outView->parameterIndex = row->parameterIndex;
    outView->nameStringOffset = row->nameStringOffset;
    outView->firstConstraintIndex = row->firstConstraintIndex;
    outView->constraintCount = row->constraintCount;
    outView->flags = row->flags;
    return ZR_TRUE;
}

/**
 * @brief 将约束行的可选签名 blob 转成经过格式验证的借用切片。
 * @pre row 来自当前 runtime 的 GenericParamConstraint 表；零长度表示该约束没有附加签名。
 * @note 非空切片必须位于签名池且通过签名格式验证；失败时清空输出，避免暴露部分解析结果。
 */
static TZrBool metadata_runtime_generic_get_constraint_blob(
        SZrMetadataRuntime *runtime,
        const SZrZrpMetadataGenericParamConstraintRow *row,
        SZrZrpMetadataPoolSliceView *outBlob) {
    if (outBlob != ZR_NULL) {
        ZrCore_Memory_RawSet(outBlob, 0, sizeof(*outBlob));
    }
    if (runtime == ZR_NULL || row == ZR_NULL || outBlob == ZR_NULL) {
        return ZR_FALSE;
    }
    if (row->signatureBlobLength == 0u) {
        return ZR_TRUE;
    }

    if (!ZrCore_ZrpMetadata_GetPoolSlice(runtime->zrpMetadataBuffer,
                                         runtime->zrpMetadataBufferLength,
                                         &runtime->zrpMetadataHeader,
                                         ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                         row->signatureBlobOffset,
                                         row->signatureBlobLength,
                                         outBlob) ||
        !ZrCore_ZrpMetadata_ValidateSignatureBlob(outBlob->data, outBlob->byteLength)) {
        ZrCore_Memory_RawSet(outBlob, 0, sizeof(*outBlob));
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/**
 * @brief 读取泛型参数的第 constraintIndex 个约束及其解析后的类型和签名视图。
 * @pre ownerToken/parameterIndex 必须先能解析为泛型参数，constraintIndex 为该参数内的零基序号。
 * @return 成功时同时返回约束行、类型记录和可选签名切片；任何索引、token 或 blob 校验失败均返回 false。
 * @note 约束行以 GenericParam 的物理索引关联，而调用端使用 owner 内逻辑序号；这里负责连接两种编号。
 */
TZrBool ZrCore_MetadataRuntime_ReadGenericParamConstraintView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken ownerToken,
        TZrUInt32 parameterIndex,
        TZrUInt32 constraintIndex,
        SZrMetadataRuntimeGenericParamConstraintView *outView) {
    SZrMetadataRuntimeGenericParamView genericParamView;
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataGenericParamConstraintRow *rows;
    const SZrZrpMetadataGenericParamConstraintRow *row;
    const SZrMetadataTokenRecord *constraintTypeRecord;
    TZrUInt32 absoluteConstraintIndex;
    SZrZrpMetadataPoolSliceView signatureBlob;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
    if (outView == ZR_NULL ||
        !ZrCore_MetadataRuntime_ReadGenericParamView(runtime,
                                                     ownerToken,
                                                     parameterIndex,
                                                     &genericParamView) ||
        constraintIndex >= genericParamView.constraintCount ||
        !ZrCore_MetadataRuntime_GetZrpSectionView(runtime,
                                                  ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS,
                                                  &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow)) {
        return ZR_FALSE;
    }

    absoluteConstraintIndex = genericParamView.firstConstraintIndex + constraintIndex;
    if (absoluteConstraintIndex < genericParamView.firstConstraintIndex ||
        absoluteConstraintIndex >= view.count) {
        return ZR_FALSE;
    }

    rows = (const SZrZrpMetadataGenericParamConstraintRow *)(const void *)view.data;
    row = &rows[absoluteConstraintIndex];
    if (row->genericParamIndex != genericParamView.genericParamIndex) {
        return ZR_FALSE;
    }

    constraintTypeRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, row->constraintTypeToken);
    if (constraintTypeRecord == ZR_NULL ||
        !metadata_runtime_generic_get_constraint_blob(runtime, row, &signatureBlob)) {
        return ZR_FALSE;
    }

    outView->genericParamView = genericParamView;
    outView->constraintRow = row;
    outView->constraintIndex = constraintIndex;
    outView->constraintTypeToken = row->constraintTypeToken;
    outView->constraintTypeRecord = constraintTypeRecord;
    outView->signatureBlob = signatureBlob;
    return ZR_TRUE;
}
