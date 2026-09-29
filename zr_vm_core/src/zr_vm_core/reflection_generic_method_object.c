#include "zr_vm_core/reflection.h"

#include "reflection_object_internal.h"
#include "zr_vm_core/metadata_runtime.h"

/**
 * @file
 * @brief 将解析后的 MethodSpec 组装为构造泛型方法反射对象。
 *
 * 元数据匹配由 resolver 负责；本文件复核解析载体，再组合方法定义对象与
 * MethodSpec 上下文。链接对象图期间会临时 pin 两个子对象，并只撤销本函数新增
 * 的 pin。返回对象交由调用方管理，调用方应在下一次可能触发 GC 的操作前将其
 * 放入 VM root。
 */

/**
 * @brief 重新解析借用的 MethodSpec 载体，并核对其元数据身份与布局。
 * @param runtime 用于解析 MethodDef 和 MethodSpec 的元数据 runtime。
 * @param input resolver 先前产生、其实参数组仍须有效的借用载体。
 * @param outResolved 接收本次解析得到的载体；不得与 input 重叠使用。
 * @return 当前解析结果与载体记录一致时返回 true；输入失效或字段不一致时返回 false。
 * @note 该检查不保留 input 或实参数组；构造对象前立即重新解析，避免使用过期载体。
 */
static TZrBool generic_method_object_revalidate(
        SZrMetadataRuntime *runtime,
        const SZrReflectionResolvedGenericMethodSpec *input,
        SZrReflectionResolvedGenericMethodSpec *outResolved) {
    if (runtime == ZR_NULL || input == ZR_NULL || outResolved == ZR_NULL ||
        input->requestedArguments == ZR_NULL || input->genericArgumentCount == 0u ||
        !ZrCore_Reflection_ResolveConstructedGenericMethod(
                runtime,
                input->genericMethodToken,
                input->requestedArguments,
                input->genericArgumentCount,
                outResolved)) {
        return ZR_FALSE;
    }
    return (TZrBool)(
            outResolved->methodSpecToken == input->methodSpecToken &&
            outResolved->methodSpecRecord == input->methodSpecRecord &&
            outResolved->genericMethodToken == input->genericMethodToken &&
            outResolved->genericMethodRecord == input->genericMethodRecord &&
            outResolved->genericSignatureHash == input->genericSignatureHash &&
            outResolved->genericArgumentCount == input->genericArgumentCount &&
            outResolved->genericArgumentListBlobOffset ==
                    input->genericArgumentListBlobOffset);
}

/**
 * @brief 从已解析的 MethodSpec 构造带定义对象和实参上下文的反射对象。
 * @param state 用于分配对象的 VM 线程状态。
 * @param runtime 解析载体所属且仍有效的元数据 runtime。
 * @param methodSpec resolver 产生的载体；其中借用的实参数组在本调用期间须有效。
 * @return 构造成功时返回构造泛型方法对象；解析、分配、pin 或字段链接失败时返回 null。
 * @note 会先重新解析并比较载体，再构造泛型方法定义和 MethodSpec 上下文。两者在
 *       后续分配及字段链接期间保持 pin；退出时仅撤销本函数新增的 pin。返回值不由
 *       本函数建立持久 root，调用方须在下一次可能触发 GC 前将它存入 VM root。
 * BUG: 临时 pin 经全局 ignored registry 无锁登记和撤销；同一 GC domain 的多个
 *       RUNNING mutator 并发调用此入口时可能丢失 root 或破坏登记索引。
 */
SZrObject *ZrCore_Reflection_BuildConstructedGenericMethodObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        const SZrReflectionResolvedGenericMethodSpec *methodSpec) {
    SZrReflectionResolvedGenericMethodSpec resolved;
    SZrObject *definitionObject;
    SZrObject *methodObject = ZR_NULL;
    SZrObject *result = ZR_NULL;
    const SZrTypeValue *nameValue;
    TZrBool definitionPinned = ZR_FALSE;
    TZrBool methodPinned = ZR_FALSE;

    if (state == ZR_NULL ||
        !generic_method_object_revalidate(runtime, methodSpec, &resolved)) {
        return ZR_NULL;
    }

    definitionObject = ZrCore_Reflection_BuildGenericMethodDefinitionObject(
            state, runtime, resolved.genericMethodToken);
    if (definitionObject == ZR_NULL ||
        !ZrCore_Reflection_ObjectPinRaw(
                state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(definitionObject),
                &definitionPinned)) {
        return ZR_NULL;
    }

    methodObject = ZrCore_Reflection_BuildMethodSpecGenericContextObject(
            state, runtime, resolved.methodSpecToken);
    if (methodObject == ZR_NULL ||
        !ZrCore_Reflection_ObjectPinRaw(
                state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(methodObject),
                &methodPinned)) {
        goto cleanup;
    }

    nameValue = ZrCore_Reflection_ObjectGetFieldValue(
            state, definitionObject, "name");
    if (nameValue == ZR_NULL || nameValue->type != ZR_VALUE_TYPE_STRING ||
        !ZrCore_Reflection_ObjectSetString(
                state, methodObject, "kind", "constructedGenericMethod") ||
        !ZrCore_Reflection_ObjectSetFieldValue(
                state, methodObject, "name", nameValue) ||
        !ZrCore_Reflection_ObjectSetBool(
                state, methodObject, "isGenericMethodDefinition", ZR_FALSE) ||
        !ZrCore_Reflection_ObjectSetObject(
                state,
                methodObject,
                "genericMethodDefinition",
                definitionObject,
                ZR_VALUE_TYPE_OBJECT)) {
        goto cleanup;
    }
    result = methodObject;

cleanup:
    ZrCore_Reflection_ObjectUnpinRaw(
            state->global,
            methodObject != ZR_NULL
                    ? ZR_CAST_RAW_OBJECT_AS_SUPER(methodObject)
                    : ZR_NULL,
            methodPinned);
    ZrCore_Reflection_ObjectUnpinRaw(
            state->global,
            ZR_CAST_RAW_OBJECT_AS_SUPER(definitionObject),
            definitionPinned);
    return result;
}

/**
 * @brief 解析泛型方法 token 与实参，并构造对应的反射对象。
 * @param state 用于分配对象的 VM 线程状态。
 * @param runtime 用于验证方法定义和泛型实参的元数据 runtime。
 * @param genericMethodToken 必须标识具有泛型形参的 MethodDef。
 * @param arguments 本次调用借用的泛型实参数组。
 * @param argumentCount 实参数量，须与方法定义的泛型形参数量相同。
 * @return 参数解析和对象构造均成功时返回构造泛型方法对象，否则返回 null。
 * @note 实参有效性及 MethodSpec 查找由 resolver 负责；本入口不保留 arguments。
 *       返回对象的 root 生命周期由调用方负责。
 * BUG: 本入口委托的构造过程经全局 ignored registry 无锁登记和撤销临时 pin；同一
 *       GC domain 的多个 RUNNING mutator 并发调用时可能丢失 root 或破坏登记索引。
 */
SZrObject *ZrCore_Reflection_MakeGenericMethodObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken genericMethodToken,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount) {
    SZrReflectionResolvedGenericMethodSpec resolved;

    if (state == ZR_NULL || runtime == ZR_NULL ||
        !ZrCore_Reflection_ResolveConstructedGenericMethod(
                runtime,
                genericMethodToken,
                arguments,
                argumentCount,
                &resolved)) {
        return ZR_NULL;
    }
    return ZrCore_Reflection_BuildConstructedGenericMethodObject(
            state, runtime, &resolved);
}
