#include "zr_vm_core/reflection.h"

#include "reflection_object_internal.h"
#include "reflection_descriptor_native_internal.h"

#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include "xxHash/xxhash.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief 在全局 `$zr` 根对象下维护 TypeId 身份对象和惰性 descriptor 关联。
 *
 * TypeId 的字段存放可由普通对象容器读取的身份数据；lookup key 用于去重，
 * auth key 则把这些字段和缓存中的同一对象地址绑定，ReadTypeIdObject 以此认证。
 * @note 缓存归属是 state->global；本层不存 MetadataRuntime，也不核 TypeId 与 native closure 绑定 runtime 的对应关系。
 * TODO: 本文件未为 `$zr` 身份缓存单独加锁；需确认共享同一 global 的 mutator 是否由上层串行化。
 */
#define ZR_REFLECTION_TYPE_ID_CACHE_KEY_BUFFER_SIZE 192u

/* 这些字段只描述身份或其惰性 descriptor；真实性由全局身份缓存中的对象地址确认。 */
static const TZrChar *kTypeIdCacheField = "__zr_reflection_type_id_cache";
static const TZrChar *kTypeIdMarkerField = "__zr_isTypeId";
static const TZrChar *kTypeIdCanonicalNameField = "__zr_canonicalTypeName";
static const TZrChar *kTypeIdCanonicalIdField = "__zr_canonicalTypeId";
static const TZrChar *kTypeIdTokenField = "__zr_typeToken";
static const TZrChar *kTypeIdSignatureHashField = "__zr_signatureHash";
static const TZrChar *kTypeIdGenerationField = "__zr_metadataGeneration";
static const TZrChar *kTypeIdCategoryField = "__zr_typeCategory";
static const TZrChar *kTypeIdDescriptorField = "__zr_descriptor";

/** @brief 限定当前反射类别枚举的连续有效区间。
 * @return ERASED 到 ENUM（含）之间为 true。
 */
static TZrBool reflection_type_category_is_valid(EZrReflectionTypeCategory category) {
    return category >= ZR_REFLECTION_TYPE_CATEGORY_ERASED &&
           category <= ZR_REFLECTION_TYPE_CATEGORY_ENUM;
}

/** @brief 将反射类别映射为 TypeLiteral 对外使用的短名称。
 * @note ERASED 和未识别值沿用通用名称 `type`。
 */
static const TZrChar *reflection_type_category_name(EZrReflectionTypeCategory category) {
    switch (category) {
        case ZR_REFLECTION_TYPE_CATEGORY_CLASS:
            return "class";
        case ZR_REFLECTION_TYPE_CATEGORY_CONCRETE_CLASS:
            return "concrete-class";
        case ZR_REFLECTION_TYPE_CATEGORY_INSTANCE_CLASS:
            return "instance-class";
        case ZR_REFLECTION_TYPE_CATEGORY_STRUCT:
            return "struct";
        case ZR_REFLECTION_TYPE_CATEGORY_INTERFACE:
            return "interface";
        case ZR_REFLECTION_TYPE_CATEGORY_RESOURCE_CLASS:
            return "resource-class";
        case ZR_REFLECTION_TYPE_CATEGORY_REF_STRUCT:
            return "ref-struct";
        case ZR_REFLECTION_TYPE_CATEGORY_ENUM:
            return "enum";
        case ZR_REFLECTION_TYPE_CATEGORY_ERASED:
        default:
            return "type";
    }
}

/** @brief 以无符号 TypeValue 写入 TypeId 的一个数值身份字段。
 * @pre state、object、fieldName 必须指向同一 GC domain 中有效对象和字段名。
 * @return 由通用反射字段写入 helper 报告的结果。
 */
static TZrBool reflection_type_identity_set_uint(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        TZrUInt64 value) {
    SZrTypeValue fieldValue;

    ZrCore_Value_InitAsUInt(state, &fieldValue, value);
    return ZrCore_Reflection_ObjectSetFieldValue(state, object, fieldName, &fieldValue);
}

/** @brief 读取无符号身份字段，并在输出地址有效时先清零。
 * @pre state、object、fieldName 和 outValue 非空；receiver 在字段键物化期间须保持可达。
 * @return 字段存在且存为无符号整数时为 true；有符号整数不会被隐式接收。
 */
static TZrBool reflection_type_identity_read_uint(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        TZrUInt64 *outValue) {
    const SZrTypeValue *value;

    if (outValue != ZR_NULL) {
        *outValue = 0u;
    }
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    value = ZrCore_Reflection_ObjectGetFieldValue(state, object, fieldName);
    if (value == ZR_NULL || !ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return ZR_FALSE;
    }
    *outValue = value->value.nativeObject.nativeUInt64;
    return ZR_TRUE;
}

/** @brief 取出或惰性创建 `$zr` 上的 TypeId 双键缓存。
 * @pre state->global->zrObject 必须是有效根对象；返回缓存由该根字段持有。
 * @return 缓存字段是对象时返回现有缓存；缺失或类型不符时尝试替换为新对象，失败返回 NULL。
 */
static SZrObject *reflection_type_identity_cache(SZrState *state) {
    SZrObject *zrObject;
    const SZrTypeValue *cacheValue;
    SZrObject *cache;

    if (state == ZR_NULL || state->global == ZR_NULL ||
        state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT ||
        state->global->zrObject.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    zrObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    cacheValue = ZrCore_Reflection_ObjectGetFieldValue(state, zrObject, kTypeIdCacheField);
    if (cacheValue != ZR_NULL && cacheValue->type == ZR_VALUE_TYPE_OBJECT &&
        cacheValue->value.object != ZR_NULL) {
        return ZR_CAST_OBJECT(state, cacheValue->value.object);
    }

    cache = ZrCore_Object_New(state, ZR_NULL);
    if (cache == ZR_NULL ||
        !ZrCore_Reflection_ObjectSetObject(
                state, zrObject, kTypeIdCacheField, cache, ZR_VALUE_TYPE_OBJECT)) {
        return ZR_NULL;
    }
    return cache;
}

/** @brief 为 TypeId 去重生成 lookup key。
 * @pre identity、非零 signatureHash 和非空缓冲区有效。
 * @note key 由 generation、token、签名指纹和类别组成；命中后仍须比较名称及完整身份。
 */
static TZrBool reflection_type_identity_make_lookup_key(
        const SZrReflectionTypeIdentity *identity,
        TZrUInt64 signatureHash,
        TZrChar *buffer,
        TZrSize bufferSize) {
    TZrInt32 written;

    if (identity == ZR_NULL || signatureHash == 0u ||
        buffer == ZR_NULL || bufferSize == 0u) {
        return ZR_FALSE;
    }

    written = snprintf(
            buffer,
            bufferSize,
            "lookup:g%u:t%u:s%016llx:c%u",
            identity->metadataGeneration,
            identity->typeToken,
            (unsigned long long)signatureHash,
            (TZrUInt32)identity->category);
    return written > 0 && (TZrSize)written < bufferSize;
}

/** @brief 为 TypeId 认证生成绑定身份字段与规范名称指纹的 auth key。
 * @pre identity、非零 signatureHash、非空规范名称和输出缓冲区有效。
 * @note 此 key 用来验证缓存中是否仍保存同一个 TypeId 对象，不单独替代字段校验。
 */
static TZrBool reflection_type_identity_make_auth_key(
        const SZrReflectionTypeIdentity *identity,
        TZrUInt64 signatureHash,
        SZrString *canonicalTypeName,
        TZrChar *buffer,
        TZrSize bufferSize) {
    TZrInt32 written;
    const TZrChar *typeNameText;
    TZrUInt64 typeNameHash;

    if (identity == ZR_NULL || signatureHash == 0u ||
        canonicalTypeName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0u) {
        return ZR_FALSE;
    }
    typeNameText = ZrCore_String_GetNativeString(canonicalTypeName);
    if (typeNameText == ZR_NULL || typeNameText[0] == '\0') {
        return ZR_FALSE;
    }
    typeNameHash = XXH3_64bits(
            typeNameText, ZrCore_String_GetByteLength(canonicalTypeName));

    written = snprintf(
            buffer,
            bufferSize,
            "auth:g%u:t%u:i%u:s%016llx:n%016llx:c%u",
            identity->metadataGeneration,
            identity->typeToken,
            identity->canonicalTypeId,
            (unsigned long long)signatureHash,
            (unsigned long long)typeNameHash,
            (TZrUInt32)identity->category);
    return written > 0 && (TZrSize)written < bufferSize;
}

/** @brief 检查去重命中的对象与请求身份相符。
 * @pre identity、state 和两个名称对象须有效且在同一 GC domain。
 * @note 请求的 canonicalTypeId 为零时不比较该可选字段，其余身份字段和名称仍须匹配。
 */
static TZrBool reflection_type_identity_matches(
        SZrState *state,
        SZrObject *object,
        SZrString *canonicalTypeName,
        const SZrReflectionTypeIdentity *identity,
        TZrUInt64 signatureHash) {
    SZrReflectionTypeIdentity decoded;
    SZrString *decodedName = ZR_NULL;

    return ZrCore_Reflection_ReadTypeIdObject(state, object, &decoded, &decodedName) &&
           decodedName != ZR_NULL && ZrCore_String_Equal(decodedName, canonicalTypeName) &&
           (identity->canonicalTypeId == 0u ||
            decoded.canonicalTypeId == identity->canonicalTypeId) &&
           decoded.typeToken == identity->typeToken &&
           decoded.signatureHash == signatureHash &&
           decoded.metadataGeneration == identity->metadataGeneration &&
           decoded.category == identity->category;
}

/** @brief 快速检查对象是否带有 TypeId 标记。
 * @note 该 marker 只是格式筛选；调用方需要可信身份时还必须调用 ReadTypeIdObject。
 */
TZrBool ZrCore_Reflection_IsTypeIdObject(SZrState *state, SZrObject *object) {
    const SZrTypeValue *marker;

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }
    marker = ZrCore_Reflection_ObjectGetFieldValue(state, object, kTypeIdMarkerField);
    return marker != ZR_NULL && marker->type == ZR_VALUE_TYPE_BOOL &&
                   marker->value.nativeObject.nativeBool != 0u
           ? ZR_TRUE
           : ZR_FALSE;
}

/** @brief 解码并认证 TypeId 的规范身份字段。
 * @pre object 在读取字段期间须保持 rooted/stable；outCanonicalTypeName 若返回则为借用引用。
 * @return 所有字段满足范围约束且 `$zr` auth key 指向同一对象时为 true；失败时输出保持清零。
 */
TZrBool ZrCore_Reflection_ReadTypeIdObject(
        SZrState *state,
        SZrObject *object,
        SZrReflectionTypeIdentity *outIdentity,
        SZrString **outCanonicalTypeName) {
    const SZrTypeValue *nameValue;
    TZrUInt64 canonicalTypeId;
    TZrUInt64 typeToken;
    TZrUInt64 signatureHash;
    TZrUInt64 metadataGeneration;
    TZrUInt64 category;
    SZrReflectionTypeIdentity decoded;
    SZrString *canonicalTypeName;
    SZrObject *cache;
    const SZrTypeValue *cachedValue;
    TZrChar cacheKey[ZR_REFLECTION_TYPE_ID_CACHE_KEY_BUFFER_SIZE];

    if (outIdentity != ZR_NULL) {
        memset(outIdentity, 0, sizeof(*outIdentity));
    }
    if (outCanonicalTypeName != ZR_NULL) {
        *outCanonicalTypeName = ZR_NULL;
    }
    if (state == ZR_NULL || object == ZR_NULL || outIdentity == ZR_NULL ||
        !ZrCore_Reflection_IsTypeIdObject(state, object)) {
        return ZR_FALSE;
    }

    nameValue = ZrCore_Reflection_ObjectGetFieldValue(state, object, kTypeIdCanonicalNameField);
    if (nameValue == ZR_NULL || nameValue->type != ZR_VALUE_TYPE_STRING ||
        nameValue->value.object == ZR_NULL ||
        !reflection_type_identity_read_uint(state, object, kTypeIdCanonicalIdField, &canonicalTypeId) ||
        !reflection_type_identity_read_uint(state, object, kTypeIdTokenField, &typeToken) ||
        !reflection_type_identity_read_uint(state, object, kTypeIdSignatureHashField, &signatureHash) ||
        !reflection_type_identity_read_uint(state, object, kTypeIdGenerationField, &metadataGeneration) ||
        !reflection_type_identity_read_uint(state, object, kTypeIdCategoryField, &category) ||
        canonicalTypeId > UINT32_MAX || typeToken > UINT32_MAX ||
        metadataGeneration > UINT32_MAX || category > UINT32_MAX || signatureHash == 0u ||
        !reflection_type_category_is_valid((EZrReflectionTypeCategory)category)) {
        return ZR_FALSE;
    }

    canonicalTypeName = ZR_CAST_STRING(state, nameValue->value.object);
    memset(&decoded, 0, sizeof(decoded));
    decoded.canonicalTypeId = (TZrUInt32)canonicalTypeId;
    decoded.typeToken = (TZrMetadataToken)typeToken;
    decoded.signatureHash = signatureHash;
    decoded.metadataGeneration = (TZrUInt32)metadataGeneration;
    decoded.category = (EZrReflectionTypeCategory)category;
    if (!reflection_type_identity_make_auth_key(
                &decoded,
                signatureHash,
                canonicalTypeName,
                cacheKey,
                sizeof(cacheKey))) {
        return ZR_FALSE;
    }
    cache = reflection_type_identity_cache(state);
    cachedValue = cache != ZR_NULL
                          ? ZrCore_Reflection_ObjectGetFieldValue(
                                    state, cache, cacheKey)
                          : ZR_NULL;
    if (cachedValue == ZR_NULL || cachedValue->type != ZR_VALUE_TYPE_OBJECT ||
        cachedValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(object)) {
        return ZR_FALSE;
    }

    *outIdentity = decoded;
    if (outCanonicalTypeName != ZR_NULL) {
        *outCanonicalTypeName = canonicalTypeName;
    }
    return ZR_TRUE;
}

/** @brief 按规范名称与身份构造或复用 TypeId，并在 `$zr` 缓存中登记双键。
 * @pre canonicalTypeName 在函数可能触发 GC 的分配期间须保持 rooted/stable；identity 为调用方提供的元数据。
 * @return 匹配缓存项或新建对象；输入无效、身份冲突或写入失败时返回 NULL。
 * @note signatureHash 为零时由规范名称计算；缓存 hit 仍须通过完整身份比较。
 * TODO: 编译器调用点以 C 局部保存 get_type_name_from_inferred_type 的结果；需确认所有长名称路径在 miss 分配期间已有 GC root。
 */
SZrObject *ZrCore_Reflection_BuildTypeIdObject(
        SZrState *state,
        SZrString *canonicalTypeName,
        const SZrReflectionTypeIdentity *identity) {
    SZrObject *cache;
    SZrObject *object;
    const SZrTypeValue *cachedValue;
    const TZrChar *typeNameText;
    SZrTypeValue canonicalNameValue;
    TZrUInt64 signatureHash;
    TZrChar lookupKey[ZR_REFLECTION_TYPE_ID_CACHE_KEY_BUFFER_SIZE];
    TZrChar authKey[ZR_REFLECTION_TYPE_ID_CACHE_KEY_BUFFER_SIZE];

    if (state == ZR_NULL || canonicalTypeName == ZR_NULL || identity == ZR_NULL ||
        !reflection_type_category_is_valid(identity->category)) {
        return ZR_NULL;
    }
    typeNameText = ZrCore_String_GetNativeString(canonicalTypeName);
    if (typeNameText == ZR_NULL || typeNameText[0] == '\0') {
        return ZR_NULL;
    }

    signatureHash = identity->signatureHash != 0u
                            ? identity->signatureHash
                            : XXH3_64bits(typeNameText, ZrCore_String_GetByteLength(canonicalTypeName));
    if (!reflection_type_identity_make_lookup_key(
                identity,
                signatureHash,
                lookupKey,
                sizeof(lookupKey)) ||
        !reflection_type_identity_make_auth_key(
                identity,
                signatureHash,
                canonicalTypeName,
                authKey,
                sizeof(authKey))) {
        return ZR_NULL;
    }

    cache = reflection_type_identity_cache(state);
    if (cache == ZR_NULL) {
        return ZR_NULL;
    }
    cachedValue = ZrCore_Reflection_ObjectGetFieldValue(state, cache, lookupKey);
    if (cachedValue != ZR_NULL && cachedValue->type == ZR_VALUE_TYPE_OBJECT &&
        cachedValue->value.object != ZR_NULL) {
        object = ZR_CAST_OBJECT(state, cachedValue->value.object);
        return reflection_type_identity_matches(
                       state, object, canonicalTypeName, identity, signatureHash)
                       ? object
                       : ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(
            state, &canonicalNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(canonicalTypeName));
    canonicalNameValue.type = ZR_VALUE_TYPE_STRING;
    object = ZrCore_Object_New(state, ZR_NULL);
    if (object == ZR_NULL ||
        !ZrCore_Reflection_ObjectSetBool(state, object, kTypeIdMarkerField, ZR_TRUE) ||
        !ZrCore_Reflection_ObjectSetFieldValue(
                state,
                object,
                kTypeIdCanonicalNameField,
                &canonicalNameValue) ||
        !reflection_type_identity_set_uint(
                state, object, kTypeIdCanonicalIdField, identity->canonicalTypeId) ||
        !reflection_type_identity_set_uint(state, object, kTypeIdTokenField, identity->typeToken) ||
        !reflection_type_identity_set_uint(state, object, kTypeIdSignatureHashField, signatureHash) ||
        !reflection_type_identity_set_uint(
                state, object, kTypeIdGenerationField, identity->metadataGeneration) ||
        !reflection_type_identity_set_uint(state, object, kTypeIdCategoryField, identity->category) ||
        !ZrCore_Reflection_ObjectSetObject(
                state, cache, lookupKey, object, ZR_VALUE_TYPE_OBJECT) ||
        /*
         * BUG: 两个别名不是原子发布。lookup 写入成功后，auth 键的字段名分配仍可失败，
         * 使对象只留在 lookup 项；ReadTypeIdObject 因缺少 auth 项拒绝它，后续同键构造
         * 又在命中分支直接返回失败，无法补齐缓存。另 ObjectSetObject 的底层写入无成功值，
         * 若 auth 插入被底层拒绝但 wrapper 返回 true，本函数会返回随后无法通过 Read 的对象。
         */
        !ZrCore_Reflection_ObjectSetObject(
                state, cache, authKey, object, ZR_VALUE_TYPE_OBJECT)) {
        return ZR_NULL;
    }
    return object;
}

/** @brief 认证 TypeId 后解析带类别能力的反射 descriptor，并惰性缓存。
 * @pre typeIdObject 与 state 同 GC domain，且在解析期间保持 rooted/stable；身份由本函数入口认证。
 * @return 已缓存 descriptor 或新建并绑定的 descriptor；任何构造/安装/写回失败均返回 NULL。
 * TODO: descriptor 字段是普通可写对象存储，命中只核非空对象；需确认调用方能否直接改写，以及是否必须经 BindTypeIdDescriptor 绑定。
 * @note native 入口只取非空的 closure runtime，尚未核 TypeId 归属；直接 C 调用也没有此校验。
 */
SZrObject *ZrCore_Reflection_ResolveTypeIdObject(SZrState *state, SZrObject *typeIdObject) {
    const SZrTypeValue *cachedDescriptor;
    SZrReflectionTypeIdentity identity;
    SZrString *canonicalTypeName = ZR_NULL;
    SZrObject *descriptor;

    if (!ZrCore_Reflection_ReadTypeIdObject(
                state, typeIdObject, &identity, &canonicalTypeName)) {
        return ZR_NULL;
    }

    cachedDescriptor = ZrCore_Reflection_ObjectGetFieldValue(
            state, typeIdObject, kTypeIdDescriptorField);
    if (cachedDescriptor != ZR_NULL && cachedDescriptor->type == ZR_VALUE_TYPE_OBJECT &&
        cachedDescriptor->value.object != ZR_NULL) {
        return ZR_CAST_OBJECT(state, cachedDescriptor->value.object);
    }

    descriptor = ZrCore_Reflection_BuildTypeLiteralObject(state, canonicalTypeName);
    /* BUG: descriptor 仅保存在 C 局部且尚未挂到 TypeId；ObjectSetString 会先分配值字符串再 pin receiver。 */
    if (descriptor == ZR_NULL ||
        !ZrCore_Reflection_ObjectSetObject(
                state, descriptor, "id", typeIdObject, ZR_VALUE_TYPE_OBJECT) ||
        !ZrCore_Reflection_ObjectSetObject(
                state, descriptor, "representedTypeId", typeIdObject, ZR_VALUE_TYPE_OBJECT) ||
        !ZrCore_Reflection_ObjectSetString(
                state, descriptor, "category", reflection_type_category_name(identity.category)) ||
        !ZrCore_Reflection_ObjectSetString(
                state, descriptor, "kind", reflection_type_category_name(identity.category)) ||
        !ZrCore_Reflection_AttachDescriptorNativeMethodsInternal(
                state, descriptor, identity.category) ||
        !ZrCore_Reflection_ObjectSetObject(
                state, typeIdObject, kTypeIdDescriptorField, descriptor, ZR_VALUE_TYPE_OBJECT)) {
        return ZR_NULL;
    }
    return descriptor;
}

/** @brief 把给定反射 descriptor 绑定到已认证 TypeId，拒绝替换已有的不同 descriptor。
 * @pre state、descriptor 与 TypeId 必须属于同一 GC domain；TypeId 的身份须可由 state cache 认证。
 * @return 未绑定时写入并返回 helper 结果；已有对象 descriptor 时仅同一对象地址返回 true。
 * @note 缓存字段不是对象时按未绑定处理并覆盖；调用方负责传入与该身份对应的 descriptor。
 */
TZrBool ZrCore_Reflection_BindTypeIdDescriptor(
        SZrState *state,
        SZrObject *typeIdObject,
        SZrObject *descriptor) {
    const SZrTypeValue *currentDescriptor;
    SZrReflectionTypeIdentity identity;

    if (state == ZR_NULL || descriptor == ZR_NULL ||
        !ZrCore_Reflection_ReadTypeIdObject(
                state, typeIdObject, &identity, ZR_NULL)) {
        return ZR_FALSE;
    }
    currentDescriptor = ZrCore_Reflection_ObjectGetFieldValue(
            state, typeIdObject, kTypeIdDescriptorField);
    if (currentDescriptor != ZR_NULL && currentDescriptor->type == ZR_VALUE_TYPE_OBJECT &&
        currentDescriptor->value.object != ZR_NULL) {
        return currentDescriptor->value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor)
                       ? ZR_TRUE
                       : ZR_FALSE;
    }
    return ZrCore_Reflection_ObjectSetObject(
            state, typeIdObject, kTypeIdDescriptorField, descriptor, ZR_VALUE_TYPE_OBJECT);
}
