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

#define ZR_REFLECTION_TYPE_ID_CACHE_KEY_BUFFER_SIZE 192u

static const TZrChar *kTypeIdCacheField = "__zr_reflection_type_id_cache";
static const TZrChar *kTypeIdMarkerField = "__zr_isTypeId";
static const TZrChar *kTypeIdCanonicalNameField = "__zr_canonicalTypeName";
static const TZrChar *kTypeIdCanonicalIdField = "__zr_canonicalTypeId";
static const TZrChar *kTypeIdTokenField = "__zr_typeToken";
static const TZrChar *kTypeIdSignatureHashField = "__zr_signatureHash";
static const TZrChar *kTypeIdGenerationField = "__zr_metadataGeneration";
static const TZrChar *kTypeIdCategoryField = "__zr_typeCategory";
static const TZrChar *kTypeIdDescriptorField = "__zr_descriptor";

static TZrBool reflection_type_category_is_valid(EZrReflectionTypeCategory category) {
    return category >= ZR_REFLECTION_TYPE_CATEGORY_ERASED &&
           category <= ZR_REFLECTION_TYPE_CATEGORY_ENUM;
}

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

static TZrBool reflection_type_identity_set_uint(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        TZrUInt64 value) {
    SZrTypeValue fieldValue;

    ZrCore_Value_InitAsUInt(state, &fieldValue, value);
    return ZrCore_Reflection_ObjectSetFieldValue(state, object, fieldName, &fieldValue);
}

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

static TZrBool reflection_type_identity_make_cache_key(
        const SZrReflectionTypeIdentity *identity,
        TZrUInt64 signatureHash,
        TZrChar *buffer,
        TZrSize bufferSize) {
    TZrInt32 written;

    if (identity == ZR_NULL || signatureHash == 0u || buffer == ZR_NULL || bufferSize == 0u) {
        return ZR_FALSE;
    }

    written = snprintf(
            buffer,
            bufferSize,
            "g%u:t%u:s%016llx:c%u",
            identity->metadataGeneration,
            identity->typeToken,
            (unsigned long long)signatureHash,
            (TZrUInt32)identity->category);
    return written > 0 && (TZrSize)written < bufferSize;
}

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
           decoded.typeToken == identity->typeToken &&
           decoded.signatureHash == signatureHash &&
           decoded.metadataGeneration == identity->metadataGeneration &&
           decoded.category == identity->category;
}

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

    outIdentity->canonicalTypeId = (TZrUInt32)canonicalTypeId;
    outIdentity->typeToken = (TZrMetadataToken)typeToken;
    outIdentity->signatureHash = signatureHash;
    outIdentity->metadataGeneration = (TZrUInt32)metadataGeneration;
    outIdentity->category = (EZrReflectionTypeCategory)category;
    if (outCanonicalTypeName != ZR_NULL) {
        *outCanonicalTypeName = ZR_CAST_STRING(state, nameValue->value.object);
    }
    return ZR_TRUE;
}

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
    TZrChar cacheKey[ZR_REFLECTION_TYPE_ID_CACHE_KEY_BUFFER_SIZE];

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
    if (!reflection_type_identity_make_cache_key(
                identity, signatureHash, cacheKey, sizeof(cacheKey))) {
        return ZR_NULL;
    }

    cache = reflection_type_identity_cache(state);
    if (cache == ZR_NULL) {
        return ZR_NULL;
    }
    cachedValue = ZrCore_Reflection_ObjectGetFieldValue(state, cache, cacheKey);
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
                state, cache, cacheKey, object, ZR_VALUE_TYPE_OBJECT)) {
        return ZR_NULL;
    }
    return object;
}

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

TZrBool ZrCore_Reflection_BindTypeIdDescriptor(
        SZrState *state,
        SZrObject *typeIdObject,
        SZrObject *descriptor) {
    const SZrTypeValue *currentDescriptor;

    if (state == ZR_NULL || descriptor == ZR_NULL ||
        !ZrCore_Reflection_IsTypeIdObject(state, typeIdObject)) {
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
