#include "compile_time_declaration_patch_interfaces.h"

#include "compile_expression_internal.h"
#include "zr_vm_parser/declaration_transform_contract.h"

#include "zr_vm_core/reflection.h"

/* interfaceAdds 在此转换成 canonical identity 暂存项；目标关系的 detached clone
 * 与最终发布由事务层负责。
 */

/* 按整数键读取已 materialize 的 superarray 元素；返回的值指针借用运行时数组。 */
static const SZrTypeValue *patch_interface_array_at(
        SZrCompilerState *cs,
        const SZrTypeValue *arrayValue,
        TZrSize index) {
    SZrObject *array;
    SZrTypeValue key;

    if (cs == ZR_NULL || arrayValue == ZR_NULL ||
        arrayValue->type != ZR_VALUE_TYPE_ARRAY ||
        arrayValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    array = ZR_CAST_OBJECT(cs->state, arrayValue->value.object);
    if (array == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsInt(cs->state, &key, (TZrInt64)index);
    return ZrCore_Object_GetValue(cs->state, array, &key);
}

/* 既有关系可能保存 alias，先直接比较，再解析到 prototype 的 canonical name 比身份。 */
static TZrBool patch_interface_identity_in_array(
        SZrCompilerState *cs,
        const SZrArray *array,
        const SZrString *canonicalName) {
    if (cs == ZR_NULL || array == ZR_NULL || canonicalName == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < array->length; index++) {
        SZrString **candidate =
                (SZrString **)ZrCore_Array_Get((SZrArray *)array, index);
        SZrTypePrototypeInfo *candidateInfo;

        if (candidate == ZR_NULL || *candidate == ZR_NULL) {
            continue;
        }
        if (ZrCore_String_Equal(*candidate, (SZrString *)canonicalName)) {
            return ZR_TRUE;
        }
        candidateInfo = find_compiler_type_prototype(cs, *candidate);
        if (candidateInfo != ZR_NULL && candidateInfo->name != ZR_NULL &&
            ZrCore_String_Equal(
                    candidateInfo->name, (SZrString *)canonicalName)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 本批名称已规范化，按此前已验证的前缀检测重复项。 */
static TZrBool patch_interface_name_in_prepared(
        const SZrParserCompileTimePatchInterfaceAdds *interfaceAdds,
        TZrSize count,
        const SZrString *name) {
    if (interfaceAdds == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < count; index++) {
        if (interfaceAdds->typeNames[index] != ZR_NULL &&
            ZrCore_String_Equal(
                    interfaceAdds->typeNames[index], (SZrString *)name)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 统一发布声明转换诊断；该 helper 不清理暂存缓冲区，由失败分支负责释放。 */
static TZrBool patch_interface_error(
        SZrCompilerState *cs,
        const TZrChar *message,
        SZrFileRange location) {
    ZrParser_CompileTime_Error(
            cs, ZR_COMPILE_TIME_ERROR_ERROR, message, location);
    return ZR_FALSE;
}

TZrBool ZrParser_CompileTime_PreparePatchInterfaceAdds(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *targetInfo,
        const SZrTypeValue *interfaceAddsValue,
        SZrFileRange location,
        SZrParserCompileTimePatchInterfaceAdds *result) {
    SZrObject *array;

    if (cs == ZR_NULL || targetInfo == ZR_NULL ||
        interfaceAddsValue == ZR_NULL || result == ZR_NULL ||
        interfaceAddsValue->type != ZR_VALUE_TYPE_ARRAY ||
        interfaceAddsValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    /* 参数有效后才重置输出；调用方必须传入不持有旧缓冲区的空结构。 */
    ZrCore_Memory_RawSet(result, 0, sizeof(*result));
    /* 先物化可能延迟构造的 superarray，再按整数键读取其元素。 */
    array = ZR_CAST_OBJECT(cs->state, interfaceAddsValue->value.object);
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY ||
        !ZrCore_Object_SuperArrayMaterializeGeneric(cs->state, array)) {
        return ZR_FALSE;
    }
    /* 空列表没有事务添加项；非空列表先检查条目预算和两种数组的尺寸乘法。 */
    result->count = ZrCore_Object_SuperArrayLength(array);
    if (result->count == 0U) {
        return ZR_TRUE;
    }
    if (result->count > ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS ||
        result->count > (TZrSize)(SIZE_MAX / sizeof(*result->typeIds)) ||
        result->count > (TZrSize)(SIZE_MAX / sizeof(*result->typeNames))) {
        return patch_interface_error(
                cs,
                "declaration_transform.interface_add: interface addition budget exceeded",
                location);
    }

    /* 两个数组按同一 index 配对；数组归 result 所有，name 指针仍是借用引用。 */
    result->typeIds = (TZrTypeId *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            result->count * sizeof(*result->typeIds),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    result->typeNames = (SZrString **)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            result->count * sizeof(*result->typeNames),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (result->typeIds == ZR_NULL || result->typeNames == ZR_NULL) {
        /* BUG: 形状合法的非空列表暂存分配失败只向 executor 返回 false。普通 class/struct
         * 调用者仅恢复当前类型状态并在 typePrototypes 发布前退出；普通 enum
         *（ZR_AST_ENUM_DECLARATION）也在发布前退出且不设错误标志，所以 compile_script
         * 会继续。union 会补诊断，不属于这条静默遗漏路径。 */
        ZrParser_CompileTime_FreePatchInterfaceAdds(cs, result);
        return ZR_FALSE;
    }
    /* 清零尚未处理的槽位，再逐项写入通过认证的 ID 与名称引用。 */
    ZrCore_Memory_RawSet(
            result->typeIds, 0, result->count * sizeof(*result->typeIds));
    ZrCore_Memory_RawSet(
            result->typeNames, 0, result->count * sizeof(*result->typeNames));

    /* 每项先认证 TypeId 并取得 canonical identity，再解析到当前 compiler 的 interface prototype。 */
    for (TZrSize index = 0; index < result->count; index++) {
        const SZrTypeValue *value =
                patch_interface_array_at(cs, interfaceAddsValue, index);
        SZrReflectionTypeIdentity identity;
        SZrString *canonicalName = ZR_NULL;
        SZrTypePrototypeInfo *interfaceInfo;

        ZrCore_Memory_RawSet(&identity, 0, sizeof(identity));
        if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT ||
            value->value.object == ZR_NULL ||
            !ZrCore_Reflection_ReadTypeIdObject(
                    cs->state,
                    ZR_CAST_OBJECT(cs->state, value->value.object),
                    &identity,
                    &canonicalName) ||
            identity.canonicalTypeId == ZR_SEMANTIC_ID_INVALID ||
            canonicalName == ZR_NULL) {
            patch_interface_error(
                    cs,
                    "declaration_transform.interface_add: expected canonical TypeId",
                    location);
            ZrParser_CompileTime_FreePatchInterfaceAdds(cs, result);
            return ZR_FALSE;
        }
        interfaceInfo = find_compiler_type_prototype(cs, canonicalName);
        if (interfaceInfo == ZR_NULL ||
            interfaceInfo->type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
            patch_interface_error(
                    cs,
                    "declaration_transform.interface_add: TypeId must resolve to an interface",
                    location);
            ZrParser_CompileTime_FreePatchInterfaceAdds(cs, result);
            return ZR_FALSE;
        }
        /* 防止目标已有的 inherits/implements 及当前暂存前缀重复引入同一接口。 */
        if (patch_interface_identity_in_array(
                    cs, &targetInfo->inherits, canonicalName) ||
            patch_interface_identity_in_array(
                    cs, &targetInfo->implements, canonicalName) ||
            patch_interface_name_in_prepared(result, index, canonicalName)) {
            patch_interface_error(
                    cs,
                    "declaration_transform.interface_add: duplicate interface",
                    location);
            ZrParser_CompileTime_FreePatchInterfaceAdds(cs, result);
            return ZR_FALSE;
        }
        /* numeric ID 供 transform 校验；名称借用给事务构造 detached 关系数组。 */
        result->typeIds[index] = identity.canonicalTypeId;
        result->typeNames[index] = canonicalName;
    }
    return ZR_TRUE;
}

/* 释放结构拥有的两个 FUNCTION-kind 数组并清零；不销毁其中指向的字符串对象。 */
void ZrParser_CompileTime_FreePatchInterfaceAdds(
        SZrCompilerState *cs,
        SZrParserCompileTimePatchInterfaceAdds *interfaceAdds) {
    if (cs == ZR_NULL || interfaceAdds == ZR_NULL) {
        return;
    }
    /* 两块 backing buffers 独立分配、分别释放；清零后可再次安全清理。 */
    if (interfaceAdds->typeIds != ZR_NULL) {
        ZR_MEMORY_RAW_FREE_LIST(
                cs->state->global, interfaceAdds->typeIds, interfaceAdds->count);
    }
    if (interfaceAdds->typeNames != ZR_NULL) {
        ZR_MEMORY_RAW_FREE_LIST(
                cs->state->global, interfaceAdds->typeNames, interfaceAdds->count);
    }
    ZrCore_Memory_RawSet(interfaceAdds, 0, sizeof(*interfaceAdds));
}
