#include "zr_vm_core/reflection.h"

#include "reflection_generic_argument_internal.h"
#include "reflection_object_internal.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_runtime.h"

#include <limits.h>
#include <string.h>
/** @brief 限制对象化泛型实参的递归深度，避免深输入耗尽 C 栈。 */
#define ZR_REFLECTION_GENERIC_ARGUMENT_OBJECT_MAX_DEPTH 64u
#define ZR_REFLECTION_GENERIC_ARGUMENT_OBJECT_MAX_NODES 1024u /* 限制两遍解码的总节点数和原生 arena 分配。 */
/** @brief 每一遍解码各持有一个游标；arena 为空时只计数，非空时写入 preorder 节点。 */
typedef struct SZrReflectionGenericArgumentObjectDecoder {
    SZrState *state;
    SZrReflectionGenericTypeArgument *arena;
    TZrUInt32 capacity;
    TZrUInt32 cursor;
} SZrReflectionGenericArgumentObjectDecoder;
/** @brief 把枚举 kind 映射到反射对象 schema 的规范名称；NONE/未知 kind 不可编码。 */
static const TZrChar *generic_argument_object_kind_name(
        EZrReflectionGenericTypeArgumentKind kind) {
    switch (kind) {
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE:
            return "primitive";
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN:
            return "typeToken";
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY:
            return "array";
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE:
            return "tuple";
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP:
            return "ownership";
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE:
            return "nullable";
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION:
            return "union";
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NONE:
        default:
            return ZR_NULL;
    }
}
/** @brief 仅接受非负且可表示为 UInt32 的 INT64 字段，拒绝隐式数值转换。 */
static TZrBool generic_argument_object_read_uint32(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        TZrUInt32 *outValue) {
    const SZrTypeValue *value = ZrCore_Reflection_ObjectGetFieldValue(
            state, object, fieldName);

    if (outValue != ZR_NULL) {
        *outValue = 0u;
    }
    if (value == ZR_NULL || outValue == ZR_NULL ||
        value->type != ZR_VALUE_TYPE_INT64 ||
        value->value.nativeObject.nativeInt64 < 0 ||
        (TZrUInt64)value->value.nativeObject.nativeInt64 > (TZrUInt64)UINT32_MAX) {
        return ZR_FALSE;
    }
    *outValue = (TZrUInt32)value->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}
/** @brief 检查 schema 标志字段的确切 BOOL 值，缺失或类型不符均关闭失败。 */
static TZrBool generic_argument_object_read_bool(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        TZrBool expected) {
    const SZrTypeValue *value = ZrCore_Reflection_ObjectGetFieldValue(
            state, object, fieldName);

    return (TZrBool)(
            value != ZR_NULL && value->type == ZR_VALUE_TYPE_BOOL &&
            (value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE) == expected);
}
/** @brief 比较反射 String 字段与规范文本，供 kind 名和对象种类交叉校验。 */
static TZrBool generic_argument_object_read_string_equals(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        const TZrChar *expected) {
    const SZrTypeValue *value = ZrCore_Reflection_ObjectGetFieldValue(
            state, object, fieldName);
    SZrString *stringValue;
    const TZrChar *text;

    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING ||
        value->value.object == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }
    stringValue = ZR_CAST_STRING(state, value->value.object);
    text = stringValue != ZR_NULL ? ZrCore_String_GetNativeString(stringValue) : ZR_NULL;
    return (TZrBool)(text != ZR_NULL && strcmp(text, expected) == 0);
}
/** @brief 读取嵌套字段并同时约束 ValueType 与对象内部种类，避免跨类对象混入。 */
static SZrObject *generic_argument_object_read_object(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        EZrValueType expectedType,
        EZrObjectInternalType expectedInternalType) {
    const SZrTypeValue *value = ZrCore_Reflection_ObjectGetFieldValue(
            state, object, fieldName);
    SZrObject *result;

    if (value == ZR_NULL || value->type != expectedType || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    result = ZR_CAST_OBJECT(state, value->value.object);
    if (result == ZR_NULL || result->internalType != expectedInternalType) {
        return ZR_NULL;
    }
    return result;
}
/** @brief 从已物化数组按连续整数索引取对象节点；调用方负责保持数组可达。 */
static SZrObject *generic_argument_object_read_array_entry(
        SZrState *state,
        SZrObject *array,
        TZrUInt32 index) {
    SZrTypeValue key;
    const SZrTypeValue *value;

    if (state == ZR_NULL || array == ZR_NULL ||
        array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    value = ZrCore_Object_GetValue(state, array, &key);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(state, value->value.object);
}
/** @brief 从本遍私有 arena 预留连续 preorder 节点；arena 为空时只推进计数游标。 */
static TZrBool generic_argument_object_take_nodes(
        SZrReflectionGenericArgumentObjectDecoder *decoder,
        TZrUInt32 count,
        SZrReflectionGenericTypeArgument **outNodes) {
    if (outNodes != ZR_NULL) {
        *outNodes = ZR_NULL;
    }
    if (decoder == ZR_NULL || count == 0u ||
        decoder->cursor > decoder->capacity || count > decoder->capacity - decoder->cursor) {
        return ZR_FALSE;
    }
    if (decoder->arena != ZR_NULL && outNodes != ZR_NULL) {
        *outNodes = &decoder->arena[decoder->cursor];
    }
    decoder->cursor += count;
    return ZR_TRUE;
}
/** @brief 递归解码一个反射 kind 节点；两遍复用此路径确保计数与写入遍历一致。 */
static TZrBool generic_argument_object_decode(
        SZrReflectionGenericArgumentObjectDecoder *decoder,
        SZrObject *object,
        SZrReflectionGenericTypeArgument *outArgument,
        TZrUInt32 depth) {
    TZrUInt32 kindValue;
    EZrReflectionGenericTypeArgumentKind kind;
    const TZrChar *kindName;
    SZrObject *nestedObject;
    SZrObject *childrenArray;
    SZrReflectionGenericTypeArgument *children = ZR_NULL;
    TZrUInt32 childCount;
    TZrUInt32 index;

    if (decoder == ZR_NULL || decoder->state == ZR_NULL || object == ZR_NULL ||
        depth >= ZR_REFLECTION_GENERIC_ARGUMENT_OBJECT_MAX_DEPTH ||
        !ZrCore_Reflection_IsReflectionObject(decoder->state, object) ||
        !generic_argument_object_read_uint32(
                decoder->state, object, "genericArgumentKindValue", &kindValue) ||
        kindValue <= (TZrUInt32)ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NONE ||
        kindValue > (TZrUInt32)ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION) {
        return ZR_FALSE;
    }
    /* 数值 discriminant 与公开文本必须一致，防止同一对象以两种 kind 解释。 */
    kind = (EZrReflectionGenericTypeArgumentKind)kindValue;
    kindName = generic_argument_object_kind_name(kind);
    if (kindName == ZR_NULL ||
        !generic_argument_object_read_string_equals(
                decoder->state, object, "genericArgumentKind", kindName)) {
        return ZR_FALSE;
    }
    if (outArgument != ZR_NULL) {
        outArgument->kind = kind;
    }
    /* 先确认节点身份，再按 kind 写入描述符；共享 validator 负责元数据语义约束。 */
    switch (kind) {
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE:
            return generic_argument_object_read_uint32(
                    decoder->state,
                    object,
                    "primitiveValueType",
                    outArgument != ZR_NULL ? &outArgument->primitiveValueType : &kindValue);

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN:
            return generic_argument_object_read_uint32(
                    decoder->state,
                    object,
                    "typeToken",
                    outArgument != ZR_NULL ? &outArgument->typeToken : &kindValue);
        /* 三种包装 kind 共用一个子节点槽，递归层仍计入同一有界 arena。 */
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY:
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP:
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE:
            nestedObject = generic_argument_object_read_object(
                    decoder->state,
                    object,
                    "elementType",
                    ZR_VALUE_TYPE_OBJECT,
                    ZR_OBJECT_INTERNAL_TYPE_OBJECT);
            if (nestedObject == ZR_NULL ||
                !generic_argument_object_take_nodes(decoder, 1u, &children) ||
                !generic_argument_object_decode(decoder, nestedObject, children, depth + 1u)) {
                return ZR_FALSE;
            }
            if (outArgument != ZR_NULL) {
                outArgument->elementType = children;
            }
            if (kind == ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY) {
                return generic_argument_object_read_uint32(
                        decoder->state,
                        object,
                        "arrayRank",
                        outArgument != ZR_NULL ? &outArgument->arrayRank : &kindValue);
            }
            if (kind == ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP) {
                if (!generic_argument_object_read_uint32(
                            decoder->state,
                            object,
                            "ownershipQualifier",
                            &kindValue)) {
                    return ZR_FALSE;
                }
                if (outArgument != ZR_NULL) {
                    outArgument->ownershipQualifier =
                            (EZrReflectionOwnershipQualifier)kindValue;
                }
                return ZR_TRUE;
            }
            return ZR_TRUE;
        /* tuple/union 保持对象数组顺序并为子节点预留连续切片，供后续签名匹配。 */
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE:
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION:
            if (!generic_argument_object_read_uint32(
                        decoder->state, object, "childCount", &childCount) ||
                (kind == ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE && childCount == 0u) ||
                childCount > ZR_REFLECTION_GENERIC_ARGUMENT_OBJECT_MAX_NODES) {
                return ZR_FALSE;
            }
            childrenArray = generic_argument_object_read_object(
                    decoder->state,
                    object,
                    "children",
                    ZR_VALUE_TYPE_ARRAY,
                    ZR_OBJECT_INTERNAL_TYPE_ARRAY);
            if (childrenArray != ZR_NULL &&
                !ZrCore_Object_SuperArrayMaterializeGeneric(decoder->state, childrenArray)) {
                return ZR_FALSE;
            }
            if (childrenArray == ZR_NULL ||
                childrenArray->nodeMap.elementCount != (TZrSize)childCount ||
                (childCount > 0u &&
                 !generic_argument_object_take_nodes(decoder, childCount, &children))) {
                return ZR_FALSE;
            }
            if (outArgument != ZR_NULL) {
                outArgument->childCount = childCount;
                outArgument->childTypes = children;
            }
            if (kind == ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION &&
                (!generic_argument_object_read_uint32(
                         decoder->state,
                         object,
                         "unionValueType",
                         outArgument != ZR_NULL ? &outArgument->unionValueType : &kindValue) ||
                 !generic_argument_object_read_uint32(
                         decoder->state,
                         object,
                          "unionNameStringOffset", /* TODO: 这里只窄化为 UInt32；规范要求当前 runtime 的 string-heap offset，需核对由谁校验归属。 */
                         outArgument != ZR_NULL
                                 ? &outArgument->unionNameStringOffset
                                 : &kindValue))) {
                return ZR_FALSE;
            }
            for (index = 0u; index < childCount; ++index) {
                nestedObject = generic_argument_object_read_array_entry(
                        decoder->state, childrenArray, index);
                if (nestedObject == ZR_NULL ||
                    !generic_argument_object_decode(
                            decoder,
                            nestedObject,
                            children != ZR_NULL ? &children[index] : ZR_NULL,
                            depth + 1u)) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NONE:
        default:
            return ZR_FALSE;
    }
}
/** @brief 解码非空顶层实参数组；root 描述符与其递归子节点共享一份 arena。 */
static TZrBool generic_argument_object_decode_array(
        SZrState *state,
        SZrObject *array,
        SZrReflectionGenericArgumentObjectDecoder *decoder,
        SZrReflectionGenericTypeArgument **outArguments,
        TZrUInt32 *outCount) {
    SZrReflectionGenericTypeArgument *arguments = ZR_NULL;
    SZrObject *argumentObject;
    TZrUInt32 argumentCount;
    TZrUInt32 index;

    if (outArguments != ZR_NULL) {
        *outArguments = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0u;
    }
    if (state == ZR_NULL || array == ZR_NULL || decoder == ZR_NULL ||
        outArguments == ZR_NULL || outCount == ZR_NULL ||
        array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY ||
        !ZrCore_Object_SuperArrayMaterializeGeneric(state, array)) {
        return ZR_FALSE;
    }
    /* 方法泛型元数必须非零；顶层数量也计入与嵌套节点共用的资源上限。 */
    if (array->nodeMap.elementCount == 0u ||
        array->nodeMap.elementCount > ZR_REFLECTION_GENERIC_ARGUMENT_OBJECT_MAX_NODES) {
        return ZR_FALSE;
    }

    argumentCount = (TZrUInt32)array->nodeMap.elementCount;
    if (!generic_argument_object_take_nodes(decoder, argumentCount, &arguments)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < argumentCount; ++index) {
        argumentObject = generic_argument_object_read_array_entry(state, array, index);
        if (argumentObject == ZR_NULL ||
            !generic_argument_object_decode(
                    decoder,
                    argumentObject,
                    arguments != ZR_NULL ? &arguments[index] : ZR_NULL,
                    0u)) {
            return ZR_FALSE;
        }
    }
    *outArguments = arguments;
    *outCount = argumentCount;
    return ZR_TRUE;
}
/** @brief 从对象化泛型实参构造 MethodSpec 反射对象。 @pre 非空输入在同步解码期间有效，对象图不被并发改写。 @return carrier、runtime 身份、schema、元数据或资源获取失败时返回 NULL；成功对象由调用方 root。 */
SZrObject *ZrCore_Reflection_MakeGenericMethodFromObjects(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        SZrObject *genericMethodDefinition,
        SZrObject *genericArguments) {
    SZrReflectionGenericArgumentObjectDecoder countDecoder;
    SZrReflectionGenericArgumentObjectDecoder decodeDecoder;
    SZrReflectionGenericTypeArgument *arguments = ZR_NULL;
    SZrReflectionGenericTypeArgument *ignoredArguments;
    const SZrTypeValue *runtimeValue;
    SZrObject *result = ZR_NULL;
    TZrUInt32 genericMethodToken;
    TZrUInt32 declaredArgumentCount;
    TZrUInt32 argumentCount;
    TZrUInt32 ignoredCount;
    TZrUInt32 index;
    TZrSize arenaByteSize = 0u;
    TZrBool definitionPinned = ZR_FALSE;
    TZrBool argumentsPinned = ZR_FALSE;
    /* 定义与实参数组在同步解码期临时 pin。 BUG: 同域多个 RUNNING mutator 可经 native entry 并发 pin/unpin；ignored registry 无锁更新计数和索引，可能丢失 root 或破坏其他登记。 */
    if (state == ZR_NULL || state->global == ZR_NULL || runtime == ZR_NULL ||
        genericMethodDefinition == ZR_NULL ||
        genericArguments == ZR_NULL ||
        !ZrCore_Reflection_ObjectPinRaw(
                state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(genericMethodDefinition),
                &definitionPinned) ||
        !ZrCore_Reflection_ObjectPinRaw(
                state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(genericArguments),
                &argumentsPinned)) {
        goto cleanup;
    }
    if (!ZrCore_Reflection_IsReflectionObject(state, genericMethodDefinition) || /* 仅接受定义 carrier，拒绝构造结果冒充定义。 */
        !generic_argument_object_read_string_equals(
                state, genericMethodDefinition, "kind", "genericMethodDefinition") ||
        !generic_argument_object_read_bool(
                state, genericMethodDefinition, "isGenericMethod", ZR_TRUE) ||
        !generic_argument_object_read_bool(
                state, genericMethodDefinition, "isGenericMethodDefinition", ZR_TRUE) ||
        !generic_argument_object_read_bool(
                state, genericMethodDefinition, "isConstructedGenericMethod", ZR_FALSE) ||
        !generic_argument_object_read_uint32(
                state, genericMethodDefinition, "genericMethodToken", &genericMethodToken) ||
        !generic_argument_object_read_uint32(
                state,
                genericMethodDefinition,
                "genericArgumentCount",
                &declaredArgumentCount)) {
        goto cleanup;
    }
    runtimeValue = ZrCore_Reflection_ObjectGetFieldValue(
            state, genericMethodDefinition, "metadataRuntime"); /* pointer 只作身份比较；元数据访问使用显式 runtime。 */
    if (runtimeValue == ZR_NULL || runtimeValue->type != ZR_VALUE_TYPE_NATIVE_POINTER ||
        runtimeValue->value.nativeObject.nativePointer != runtime ||
        genericArguments->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY ||
        !ZrCore_Object_SuperArrayMaterializeGeneric(state, genericArguments)) {
        goto cleanup;
    }
    if (genericArguments->nodeMap.elementCount != (TZrSize)declaredArgumentCount) {
        goto cleanup;
    }
    countDecoder.state = state; /* 第一遍验证对象形状并计数，按实际节点数分配而不递归分配。 */
    countDecoder.arena = ZR_NULL;
    countDecoder.capacity = ZR_REFLECTION_GENERIC_ARGUMENT_OBJECT_MAX_NODES;
    countDecoder.cursor = 0u;
    if (!generic_argument_object_decode_array(
                state,
                genericArguments,
                &countDecoder,
                &ignoredArguments,
                &ignoredCount) ||
        countDecoder.cursor == 0u) {
        goto cleanup;
    }
    /* 第二遍写入 descriptor 树；游标仅检节点数。 TODO: pin 不冻结输入图，核查 native 调用方是否保证解码期间无并发改写，或需锁/快照。 */
    arenaByteSize = (TZrSize)countDecoder.cursor * sizeof(*arguments);
    arguments = (SZrReflectionGenericTypeArgument *)ZrCore_Memory_RawMallocWithType(
            state->global, arenaByteSize, ZR_MEMORY_NATIVE_TYPE_OBJECT);
    if (arguments == ZR_NULL) {
        goto cleanup;
    }
    ZrCore_Memory_RawSet(arguments, 0, arenaByteSize);
    decodeDecoder.state = state;
    decodeDecoder.arena = arguments;
    decodeDecoder.capacity = countDecoder.cursor;
    decodeDecoder.cursor = 0u;
    if (!generic_argument_object_decode_array(
                state,
                genericArguments,
                &decodeDecoder,
                &arguments,
                &argumentCount) ||
        decodeDecoder.cursor != countDecoder.cursor ||
        argumentCount != declaredArgumentCount) {
        goto cleanup;
    }
    for (index = 0u; index < argumentCount; ++index) { /* runtime validator 再核 token、kind 字段与递归子节点。 */
        if (!ZrCore_Reflection_ValidateGenericTypeArgument(
                    runtime, &arguments[index], 0u)) {
            goto cleanup;
        }
    }
    result = ZrCore_Reflection_MakeGenericMethodObject(
            state,
            runtime,
            (TZrMetadataToken)genericMethodToken,
            arguments,
            argumentCount); /* 下游同步借用 arguments，不把临时 arena 存入结果。 */
/* 所有路径在此释放临时 arena 并配对撤销新建 pin；成功对象仍由调用方 root。 */
cleanup:
    if (arguments != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                state->global,
                arguments,
                arenaByteSize,
                ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }
    ZrCore_Reflection_ObjectUnpinRaw(
            state != ZR_NULL ? state->global : ZR_NULL,
            genericArguments != ZR_NULL
                    ? ZR_CAST_RAW_OBJECT_AS_SUPER(genericArguments)
                    : ZR_NULL,
            argumentsPinned);
    ZrCore_Reflection_ObjectUnpinRaw(
            state != ZR_NULL ? state->global : ZR_NULL,
            genericMethodDefinition != ZR_NULL
                    ? ZR_CAST_RAW_OBJECT_AS_SUPER(genericMethodDefinition)
                    : ZR_NULL,
            definitionPinned);
    return result;
}
