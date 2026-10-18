#include "artifact/artifact_projection.h"

#include <string.h>

#include "zr_vm_core/reflection.h"
#include "zr_vm_library/native_binding.h"

/* 统一 state 投影的失败诊断；具体失败分支决定 expected/actual 承载的比较值。 */
static EZrArtifactStatus artifact_metadata_projection_fail(
        SZrArtifactDiagnostic *diagnostic,
        EZrArtifactStatus status,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
        diagnostic->sectionKind = ZR_ARTIFACT_SECTION_METADATA_STATE_TABLE;
        diagnostic->expectedHash = expected;
        diagnostic->actualHash = actual;
    }
    return status;
}

/* 只比较可公开反射的类别与原生原型种类；擦除态无成员语义，由上层限制保留级别。
 * TODO: 原生注册把 INVALID 默认原型当作 CLASS，此处却落入拒绝分支；
 * 当前 BuildState 调用未使用 INVALID，需确认默认原型是否允许进入此投影。 */
static TZrBool artifact_metadata_native_category_matches(
        EZrReflectionTypeCategory category,
        EZrObjectPrototypeType prototypeType) {
    if (category == ZR_REFLECTION_TYPE_CATEGORY_ERASED) return ZR_TRUE;
    switch (prototypeType) {
        case ZR_OBJECT_PROTOTYPE_TYPE_CLASS:
            return (TZrBool)(category == ZR_REFLECTION_TYPE_CATEGORY_CLASS ||
                             category == ZR_REFLECTION_TYPE_CATEGORY_CONCRETE_CLASS ||
                             category == ZR_REFLECTION_TYPE_CATEGORY_INSTANCE_CLASS ||
                             category == ZR_REFLECTION_TYPE_CATEGORY_RESOURCE_CLASS);
        case ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE:
            return (TZrBool)(category == ZR_REFLECTION_TYPE_CATEGORY_INTERFACE);
        case ZR_OBJECT_PROTOTYPE_TYPE_STRUCT:
            return (TZrBool)(category == ZR_REFLECTION_TYPE_CATEGORY_STRUCT ||
                             category == ZR_REFLECTION_TYPE_CATEGORY_REF_STRUCT);
        case ZR_OBJECT_PROTOTYPE_TYPE_ENUM:
            return (TZrBool)(category == ZR_REFLECTION_TYPE_CATEGORY_ENUM);
        /* TODO: 此处把 NATIVE 当资源类，运行时 reflection_type_category_for_prototype
         * 却将它归入 ERASED；当前未发现此种原生类型描述符，需确认预留类别的公开契约。 */
        case ZR_OBJECT_PROTOTYPE_TYPE_NATIVE:
            return (TZrBool)(category == ZR_REFLECTION_TYPE_CATEGORY_RESOURCE_CLASS);
        default:
            return ZR_FALSE;
    }
}

/* 借用描述符数组前核对 count/pointer 对，避免后续计数读取缺失的原生声明表。 */
static TZrBool artifact_metadata_native_shape_is_valid(
        const ZrLibTypeDescriptor *descriptor) {
    return (TZrBool)((descriptor->fieldCount == 0u || descriptor->fields != ZR_NULL) &&
                     (descriptor->methodCount == 0u || descriptor->methods != ZR_NULL) &&
                     (descriptor->metaMethodCount == 0u ||
                      descriptor->metaMethods != ZR_NULL) &&
                     (descriptor->enumMemberCount == 0u ||
                      descriptor->enumMembers != ZR_NULL));
}

/* 将多个方法声明映射到属性身份计数，供完整 metadata state 与原生描述符对账。
 * BUG: 此处仅凭非空 propertyName 计数；原生注册还要求访问模式非 NONE 且方法有效。
 * propertyName 非空、访问模式为 NONE 时，合法的零属性保留数被误拒、错误的一属性数被接受。
 * test_artifact_schema_metadata_graph.c 的 propertyName="x" 夹具保留默认 NONE；
 * 后续应与 native_registry_add_methods 的属性发布条件对齐。 */
static TZrSize artifact_metadata_native_property_count(
        const ZrLibTypeDescriptor *descriptor) {
    TZrSize propertyCount = 0u;
    TZrSize index;

    for (index = 0u; index < descriptor->methodCount; ++index) {
        const TZrChar *propertyName = descriptor->methods[index].propertyName;
        TZrSize previousIndex;
        TZrBool seen = ZR_FALSE;

        if (propertyName == ZR_NULL || propertyName[0] == '\0') continue;
        /* TODO: 此处把同名声明折叠成一个属性，原生注册却逐次追加属性成员；
         * 需核查 PropertyDef 的身份规则与 native_registry_add_methods 是否应共享去重约束。 */
        for (previousIndex = 0u; previousIndex < index; ++previousIndex) {
            const TZrChar *previousName =
                    descriptor->methods[previousIndex].propertyName;
            if (previousName != ZR_NULL && strcmp(previousName, propertyName) == 0) {
                seen = ZR_TRUE;
                break;
            }
        }
        if (!seen) ++propertyCount;
    }
    return propertyCount;
}

/* 检查描述符表是否可读，并把原生声明数量限制到 artifact 行计数的整数范围。
 * TODO: 此处直接累计字段/方法/元方法/枚举条目，而原生注册会跳过缺名、缺回调或
 * 无效枚举值的条目；需核实 state 的 retainedMemberCount 应计声明槽位还是实际 MemberDef 行。 */
static EZrArtifactStatus artifact_metadata_native_counts(
        const ZrLibTypeDescriptor *descriptor,
        TZrSize *outMemberCount,
        TZrSize *outPropertyCount,
        SZrArtifactDiagnostic *diagnostic) {
    const TZrSize maxCount = (TZrSize)((TZrUInt32)~0u);
    const TZrSize counts[] = {
            descriptor->fieldCount,
            descriptor->methodCount,
            descriptor->metaMethodCount,
            descriptor->enumMemberCount};
    TZrSize memberCount = 0u;
    TZrSize index;

    if (!artifact_metadata_native_shape_is_valid(descriptor)) {
        return artifact_metadata_projection_fail(
                diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u);
    }
    for (index = 0u; index < sizeof(counts) / sizeof(counts[0]); ++index) {
        if (counts[index] > maxCount || memberCount > maxCount - counts[index]) {
            return artifact_metadata_projection_fail(
                    diagnostic, ZR_ARTIFACT_STATUS_COUNT_LIMIT, maxCount, counts[index]);
        }
        memberCount += counts[index];
    }
    *outMemberCount = memberCount;
    *outPropertyCount = artifact_metadata_native_property_count(descriptor);
    return ZR_ARTIFACT_STATUS_OK;
}

/* 把反射身份和保留级别投影成可供 artifact 图校验的 metadata state 摘要。
 * identity、可选原生描述符均只借用，outState 不得与输入重叠；调用方须先确定保留的 MemberDef/PropertyDef/记录数，
 * 并提供与布局、调用契约一致的稳定哈希。失败时清零 outState，不发布部分状态。
 * 目前直接调用位于 source/native/binary 对照测试；完整文档写读还会独立复核交叉链接。 */
EZrArtifactStatus ZrParser_ArtifactMetadata_BuildState(
        const SZrReflectionTypeIdentity *identity,
        const ZrLibTypeDescriptor *nativeTypeDescriptor,
        EZrArtifactMetadataPreservationState preservationState,
        TZrUInt32 retainedMemberCount,
        TZrUInt32 retainedPropertyCount,
        TZrUInt32 retainedMetaRecordCount,
        TZrUInt64 layoutHash,
        TZrUInt64 callableContractHash,
        SZrArtifactMetadataStateRow *outState,
        SZrArtifactDiagnostic *diagnostic) {
    TZrSize nativeMemberCount = 0u;
    TZrSize nativePropertyCount = 0u;
    EZrArtifactStatus status;

    if (outState != ZR_NULL) memset(outState, 0, sizeof(*outState));
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (identity == ZR_NULL || outState == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(identity->typeToken) != ZR_METADATA_TABLE_TYPE_DEF ||
        ZR_METADATA_TOKEN_RID(identity->typeToken) == 0u ||
        identity->signatureHash == 0u || identity->metadataGeneration == 0u ||
        identity->category < ZR_REFLECTION_TYPE_CATEGORY_ERASED ||
        identity->category > ZR_REFLECTION_TYPE_CATEGORY_ENUM ||
        preservationState < ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY ||
        preservationState > ZR_ARTIFACT_METADATA_PRESERVATION_FULL ||
        layoutHash == 0u || callableContractHash == 0u) {
        return artifact_metadata_projection_fail(
                diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, 0u);
    }
    /* 保留级别声明消费者能依赖的表：擦除态仅承诺身份，Members 不承诺元数据记录。 */
    if ((preservationState == ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY &&
         (retainedMemberCount != 0u || retainedPropertyCount != 0u ||
          retainedMetaRecordCount != 0u)) ||
        (preservationState == ZR_ARTIFACT_METADATA_PRESERVATION_MEMBERS &&
         retainedMetaRecordCount != 0u) ||
        (preservationState != ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY &&
         identity->category == ZR_REFLECTION_TYPE_CATEGORY_ERASED)) {
        return artifact_metadata_projection_fail(
                diagnostic, ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN, 0u, preservationState);
    }
    if (nativeTypeDescriptor != ZR_NULL) {
        if (!artifact_metadata_native_category_matches(
                    identity->category, nativeTypeDescriptor->prototypeType)) {
            return artifact_metadata_projection_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    identity->category,
                    nativeTypeDescriptor->prototypeType);
        }
        status = artifact_metadata_native_counts(
                nativeTypeDescriptor,
                &nativeMemberCount,
                &nativePropertyCount,
                diagnostic);
        if (status != ZR_ARTIFACT_STATUS_OK) return status;
        /* BUG: 仅属性数不符时，诊断仍报告相等的成员数；
         * test_artifact_schema_metadata_graph.c 的 propertyName="x" 输入可到达此分支，
         * expected/actual 会同为 1，未指出实际不符的属性数。 */
        if (preservationState != ZR_ARTIFACT_METADATA_PRESERVATION_IDENTITY_ONLY &&
            (nativeMemberCount != retainedMemberCount ||
             nativePropertyCount != retainedPropertyCount)) {
            return artifact_metadata_projection_fail(
                    diagnostic,
                    ZR_ARTIFACT_STATUS_ILLEGAL_TOKEN,
                    nativeMemberCount,
                    retainedMemberCount);
        }
    }

    /* 摘要在所有稳定字段就位后计算；图校验和二进制读取端会用相同字段序列复算。 */
    outState->typeToken = identity->typeToken;
    outState->preservationState = preservationState;
    outState->category = (EZrArtifactReflectionCategory)identity->category;
    outState->metadataGeneration = identity->metadataGeneration;
    outState->retainedMemberCount = retainedMemberCount;
    outState->retainedPropertyCount = retainedPropertyCount;
    outState->retainedMetaRecordCount = retainedMetaRecordCount;
    outState->typeSignatureHash = identity->signatureHash;
    outState->layoutHash = layoutHash;
    outState->callableContractHash = callableContractHash;
    outState->metadataHash = ZrCore_Artifact_ComputeMetadataStateHash(outState);
    return ZR_ARTIFACT_STATUS_OK;
}

/**
 * @fn static EZrArtifactStatus artifact_metadata_projection_fail( SZrArtifactDiagnostic *diagnostic, EZrArtifactStatus status, TZrUInt64 expected, TZrUInt64 actual)
 *
 * ## Summary
 * 为metadata state的局部拒绝生成统一节诊断。 形状、身份、保留级别及计数拒绝分支传入各自比较值。
 *
 * ## Parameters
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 * @param[in] status 本拒绝分支需要传播的artifact状态，原样返回。
 * @param[in] expected 分支的预期比较载荷，可能为计数/类别而非哈希。
 * @param[in] actual 分支的实际比较载荷，可能为计数/类别而非哈希。
 *
 * ## Returns
 * @return 原样返回status；可选诊断不残留上一错误的字段。
 *
 * ## Constraints and lifecycle
 * diagnostic可空；非空时先完整清零，再写status、METADATA_STATE_TABLE与expected/actual；两个数是分支的比较载荷，不始终表示哈希。无分配。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:9; zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:113; zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:118。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static TZrBool artifact_metadata_native_category_matches( EZrReflectionTypeCategory category, EZrObjectPrototypeType prototypeType)
 *
 * ## Summary
 * 比较反射类别与原生描述符原型种类，作为metadata状态发布前的局部门禁。 仅由BuildState在提供nativeTypeDescriptor时调用。
 *
 * ## Parameters
 * @param[in] category 待匹配的反射类别；ERASED的保留级别另由入口限制。
 * @param[in] prototypeType 原生描述符的原型枚举；采用本函数列出的映射。
 *
 * ## Returns
 * @return TRUE表示此映射接受；FALSE表示类别不兼容，不自行填写诊断或证明完整native元数据可物化。
 *
 * ## Constraints and lifecycle
 * ERASED直接匹配，但非identity-only的ERASED由外层保留级别拒绝。CLASS可匹配四种class类别，STRUCT可匹配STRUCT/REF_STRUCT；INVALID落入拒绝，NATIVE仅匹配RESOURCE_CLASS。TODO: INVALID在native_registry_add_type默认CLASS而此处拒绝，NATIVE与reflection_type_category_for_prototype默认ERASED也不同；下一步核查这两类描述符是否允许进入BuildState。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:27; zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:173; zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1670; zr_vm_core/src/zr_vm_core/reflection/reflection.c:4674; zr_vm_core/src/zr_vm_core/reflection/reflection.c:4753。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static TZrBool artifact_metadata_native_shape_is_valid( const ZrLibTypeDescriptor *descriptor)
 *
 * ## Summary
 * 在读取原生声明表之前检查各非零count具有数组地址。 native_counts先调用本门禁，再累计槽位与遍历methods。
 *
 * ## Parameters
 * @param[in] descriptor 非空原生类型描述符及计数所指数组，生命周期覆盖整个读取。
 *
 * ## Returns
 * @return TRUE仅表示四组形状条件成立；FALSE不写输出或诊断。
 *
 * ## Constraints and lifecycle
 * descriptor由调用链保证非空；分别检查fields/methods/metaMethods/enumMembers的count-pointer配对。不能验证实际分配长度、元素内容或回调有效性；数组仍由描述符提供者持有。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:54; zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:112。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static TZrSize artifact_metadata_native_property_count( const ZrLibTypeDescriptor *descriptor)
 *
 * ## Summary
 * 按非空且非空串的propertyName归纳去重属性名称数，供native状态对账。 native_counts已验证methods形状后调用；结果作为BuildState的nativePropertyCount。
 *
 * ## Parameters
 * @param[in] descriptor 非空原生类型描述符及计数所指数组，生命周期覆盖整个读取。
 *
 * ## Returns
 * @return 返回按名称去重的数量，不等同于已实际物化的PropertyDef行数。
 *
 * ## Constraints and lifecycle
 * descriptor与方法数组/名称字符串借用至返回；遍历不分配。BUG: 此计数未应用native_registry_add_methods的有效name/callback和propertyReferenceAccess!=NONE条件；可接受的方法带propertyName但access=NONE时不会发布属性，本投影仍计1，可能误拒retainedPropertyCount=0。tests/parser/test_artifact_schema_metadata_graph.c中的propertyName="x"输入具有非空名称而access=NONE的组合；本函数不核对发布条件。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:69; zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:124; zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1301; zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1344; zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1345; zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1377; tests/parser/test_artifact_schema_metadata_graph.c:489; tests/parser/test_artifact_schema_metadata_graph.c:492。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @par artifact_metadata_native_property_count::deduplicate_names
 *
 * ## Summary
 * 同名propertyName只增加一次逻辑属性名称计数。 每个非空当前名称通过对比此前全部名称判断是否已见。
 *
 * ## Constraints and lifecycle
 * 输入名称只借用；先前相同名称不要求对应条目有效。TODO: native_registry_add_methods逐声明追加属性，需在PropertyDef身份/保留计数契约处确认是否应按名称折叠。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:80; zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:82; zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1377。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn static EZrArtifactStatus artifact_metadata_native_counts( const ZrLibTypeDescriptor *descriptor, TZrSize *outMemberCount, TZrSize *outPropertyCount, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 检查四类原生声明表形状及总成员计数的UINT32上界，计算声明槽位与属性名称数。 BuildState核对native与retained计数前使用。
 *
 * ## Parameters
 * @param[in] descriptor 非空原生类型描述符及计数所指数组，生命周期覆盖整个读取。
 * @param[out] outMemberCount 必填声明槽位总数输出，仅成功时赋值。
 * @param[out] outPropertyCount 必填去重属性名称数输出，仅成功时赋值。
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 *
 * ## Returns
 * @return OK时交付总声明槽位数和去重属性名称数；INVALID_ARGUMENT或COUNT_LIMIT拒绝。属性计数沿用同一既有发布条件问题。
 *
 * ## Constraints and lifecycle
 * descriptor及两个输出指针非空；先形状检查，再用maxCount-count防止总和溢出。普通拒绝发生在输出赋值前，输出旧值不在本函数清零；可选diagnostic只在失败时改写。TODO: 直接累加声明槽位而注册会跳过无效条目，需在BuildState/MemberDef契约处确认retainedMemberCount按槽位还是实际发布行计算。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:98; zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:181; zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1301; zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c:1597。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @fn EZrArtifactStatus ZrParser_ArtifactMetadata_BuildState( const SZrReflectionTypeIdentity *identity, const ZrLibTypeDescriptor *nativeTypeDescriptor, EZrArtifactMetadataPreservationState preservationState, TZrUInt32 retainedMemberCount, TZrUInt32 retainedPropertyCount, TZrUInt32 retainedMetaRecordCount, TZrUInt64 layoutHash, TZrUInt64 callableContractHash, SZrArtifactMetadataStateRow *outState, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 将已确定的反射身份、保留级别及稳定契约投影成metadata state行与可复算摘要。 由源/native/binary对照消费者准备局部状态；完整artifact写读端另做跨表图一致性校验。
 *
 * ## Parameters
 * @param[in] identity 非空反射身份，TypeDef token/签名摘要/metadataGeneration已设置。
 * @param[in] nativeTypeDescriptor 可空原生描述符；非空时数组和名称须可读，参与类别/计数对账。
 * @param[in] preservationState IDENTITY_ONLY至FULL的保留级别，与三个保留数及ERASED限制一致。
 * @param[in] retainedMemberCount 调用者声明保留的成员数；本函数不会实际创建成员。
 * @param[in] retainedPropertyCount 调用者声明保留的属性数；须与采用的native投影计数规则相符。
 * @param[in] retainedMetaRecordCount 保留记录数；identity-only和Members级别要求为0。
 * @param[in] layoutHash 非零稳定布局摘要，本层不从布局重新推导。
 * @param[in] callableContractHash 非零调用合同摘要，本层不从方法实现重新推导。
 * @param[out] outState 必填且与输入不重叠的状态行，入口先清零，成功最后计算摘要。
 * @param[out] diagnostic 可空诊断输出，仅按本单元Returns与约束所述字段更新；对象由调用者持有。
 *
 * ## Returns
 * @return OK时所有字段齐备后计算metadataHash；普通失败使非空outState保持全零并返回具体状态。提供native时还受原型映射、形状及声明计数规则约束，不能把OK视为实际native成员已物化。
 *
 * ## Constraints and lifecycle
 * identity/outState必填且输出不与identity或native描述符重叠；可选native数组/名称借用至返回。先清输出/诊断，再核TypeDef RID、签名/代数、类别、保留级别和非零布局/调用哈希；identity-only不留表，Members不留记录，ERASED仅identity-only。无分配，不代替跨表计数证明。BUG: 仅属性数不符时仍将member数写入expected/actual，可能同为1；该既有诊断问题详见native_count_diagnostic块。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:132; tests/parser/test_artifact_schema_metadata_graph.c:412; tests/parser/test_artifact_schema_metadata_graph.c:425; tests/parser/test_artifact_schema_metadata_graph.c:492; zr_vm_core/src/zr_vm_core/artifact/artifact_identity.c:95。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @par ZrParser_ArtifactMetadata_BuildState::preservation_gate
 *
 * ## Summary
 * 限定消费者按保留级别能够依赖的表，防止擦除身份声明完整元数据。 身份/类别/版本/哈希基础门禁通过后，在读取native描述符之前执行。
 *
 * ## Constraints and lifecycle
 * identity-only要求三保留数全0；Members要求记录数0；ERASED不能使用非identity-only。失败尚未发布outState。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:161; zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:162。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @par ZrParser_ArtifactMetadata_BuildState::native_count_diagnostic
 *
 * ## Summary
 * 在native计数与retained声明不符时拒绝状态发布。 非identity-only且任意成员/属性计数不相等时进入。
 *
 * ## Constraints and lifecycle
 * BUG: 条件同时检查member/property，诊断却仅填写nativeMemberCount/retainedMemberCount；属性单独不符时expectedHash与actualHash可同为1，不能从这两个值定位属性差异。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:187; zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:190; tests/parser/test_artifact_schema_metadata_graph.c:489; tests/parser/test_artifact_schema_metadata_graph.c:492。这些位置分别为本单元定义/使用或下层合同证据。
 */

/**
 * @par ZrParser_ArtifactMetadata_BuildState::stable_hash
 *
 * ## Summary
 * 在所有稳定状态字段填写完成后计算可由写读图验证复算的metadata摘要。 全部局部门禁成功后一次性填写调用者outState。
 *
 * ## Constraints and lifecycle
 * outState先前已清零，flags保持0；摘要不含metadataHash自身，覆盖token/保留级别/类别/代数/计数/签名/布局/调用哈希。此发布不代替跨表行一致性检查。
 *
 * ## References
 * zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:201; zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c:202; zr_vm_core/src/zr_vm_core/artifact/artifact_identity.c:95; zr_vm_core/src/zr_vm_core/artifact/artifact_identity.c:100。这些位置分别为本单元定义/使用或下层合同证据。
 */
