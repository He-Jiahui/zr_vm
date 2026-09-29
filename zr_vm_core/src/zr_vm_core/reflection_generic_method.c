#include "zr_vm_core/reflection.h"
/* MethodSpec 解析与对象构造分层：本文件只核对元数据记录，不分配反射对象。 */
#include "reflection_generic_argument_internal.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_runtime.h"
/* 失败结果始终清空，避免调用方误用上次解析留下的借用指针。 */
static void reflection_clear_resolved_generic_method_spec(
        SZrReflectionResolvedGenericMethodSpec *methodSpec) {
    if (methodSpec != ZR_NULL) {
        ZrCore_Memory_RawSet(methodSpec, 0, sizeof(*methodSpec));
    }
}
/* 比较记录所属 MethodDef 和每个签名节点，避免只凭 token 或哈希接受错误实例。 */
static TZrBool reflection_method_spec_matches_request(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken,
        const SZrMetadataRuntimeGenericOwnerView *ownerView,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount,
        SZrMetadataRuntimeMethodSpecSignatureView *outView) {
    SZrMetadataRuntimeMethodSpecSignatureView view;
    TZrUInt32 index;

    if (!ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(runtime, methodSpecToken, &view) ||
        view.methodToken != ownerView->ownerToken ||
        view.methodRecord != ownerView->ownerRecord ||
        view.argumentCount != argumentCount) {
        return ZR_FALSE;
    }

    for (index = 0u; index < argumentCount; ++index) {
        SZrMetadataRuntimeMethodSpecGenericArgumentView argumentView;
        if (!ZrCore_MetadataRuntime_ReadMethodSpecGenericArgumentView(
                    runtime, methodSpecToken, index, &argumentView) ||
            !ZrCore_Reflection_GenericTypeArgumentMatchesSignatureNode(
                    runtime,
                    &argumentView.signatureView.blob,
                    &argumentView.argumentNode,
                    argumentView.argumentToken,
                    &arguments[index],
                    0u)) {
            return ZR_FALSE;
        }
    }

    *outView = view;
    return ZR_TRUE;
}
/* 两张记录表共用匹配规则；首次命中的记录及调用方实参都以借用形式进入结果。 */
static TZrBool reflection_resolve_generic_method_from_records(
        SZrMetadataRuntime *runtime,
        const SZrMetadataTokenRecord *records,
        TZrUInt32 recordCount,
        const SZrMetadataRuntimeGenericOwnerView *ownerView,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount,
        SZrReflectionResolvedGenericMethodSpec *outMethodSpec) {
    TZrUInt32 index;

    if (records == ZR_NULL) {
        return ZR_FALSE;
    }
    /* 只将签名表 token 交给 MethodSpec 读取器，其他元数据记录不参与匹配。 */
    for (index = 0u; index < recordCount; ++index) {
        SZrMetadataRuntimeMethodSpecSignatureView view;
        if (ZR_METADATA_TOKEN_TABLE(records[index].token) != ZR_METADATA_TABLE_SIGNATURE ||
            !reflection_method_spec_matches_request(runtime,
                                                    records[index].token,
                                                    ownerView,
                                                    arguments,
                                                    argumentCount,
                                                    &view)) {
            continue;
        }

        outMethodSpec->methodSpecToken = view.methodSpecToken;
        outMethodSpec->methodSpecRecord = view.methodSpecRecord;
        outMethodSpec->genericMethodToken = view.methodToken;
        outMethodSpec->genericMethodRecord = view.methodRecord;
        outMethodSpec->genericSignatureHash = view.signatureHash;
        outMethodSpec->genericArgumentCount = view.argumentCount;
        outMethodSpec->genericArgumentListBlobOffset = view.argumentListBlobOffset;
        outMethodSpec->requestedArguments = arguments;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}
/* 由泛型 MethodDef 与实参反查现存 MethodSpec；结果借用实参数组和 runtime 记录。 */
TZrBool ZrCore_Reflection_ResolveConstructedGenericMethod(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken genericMethodToken,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount,
        SZrReflectionResolvedGenericMethodSpec *outMethodSpec) {
    SZrMetadataRuntimeGenericOwnerView ownerView;
    SZrFunction *metadataFunction;
    TZrUInt32 index;

    reflection_clear_resolved_generic_method_spec(outMethodSpec);
    if (runtime == ZR_NULL || outMethodSpec == ZR_NULL || arguments == ZR_NULL || argumentCount == 0u ||
        ZR_METADATA_TOKEN_TABLE(genericMethodToken) != ZR_METADATA_TABLE_MEMBER_DEF ||
        !ZrCore_MetadataRuntime_ReadGenericOwnerView(runtime, genericMethodToken, &ownerView) ||
        ownerView.methodDefRow == ZR_NULL || ownerView.genericParamCount != argumentCount) {
        return ZR_FALSE;
    }
    /* 在扫描签名记录前先验证每个实参，防止无效 token 与类型进入深层匹配。 */
    for (index = 0u; index < argumentCount; ++index) {
        if (!ZrCore_Reflection_ValidateGenericTypeArgument(runtime, &arguments[index], 0u)) {
            return ZR_FALSE;
        }
    }
    /* 没有函数元数据载体无法定位 MethodSpec；优先其自身记录，未命中再查模块记录。 */
    metadataFunction = runtime->metadataFunction;
    if (metadataFunction == ZR_NULL) {
        return ZR_FALSE;
    }
    if (reflection_resolve_generic_method_from_records(
                runtime,
                metadataFunction->metadataTokenRecords,
                metadataFunction->metadataTokenRecordLength,
                &ownerView,
                arguments,
                argumentCount,
                outMethodSpec)) {
        return ZR_TRUE;
    }
    return reflection_resolve_generic_method_from_records(
            runtime,
            metadataFunction->moduleMetadataTokenRecords,
            metadataFunction->moduleMetadataTokenRecordLength,
            &ownerView,
            arguments,
            argumentCount,
            outMethodSpec);
}
