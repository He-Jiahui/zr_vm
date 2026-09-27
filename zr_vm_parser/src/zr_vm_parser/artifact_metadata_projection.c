#include "zr_vm_parser/artifact_projection.h"

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
