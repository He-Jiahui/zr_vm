#include "compiler_metadata_string_heap.h"
#include "compiler_internal.h"
#include "compiler_metadata_signature.h"
#include "compiler_metadata_type_ref.h"
#include "type_inference_internal.h"

#include <string.h>

static TZrNativeString metadata_token_native_string(SZrString *value) {
    return value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL;
}

static TZrUInt32 metadata_token_stable_string_index(SZrString *value) {
    TZrNativeString text = metadata_token_native_string(value);
    TZrSize length = text != ZR_NULL ? strlen(text) : 0;
    TZrUInt64 hash;

    if (length == 0) {
        return 0;
    }

    hash = ZrCore_Hash_CreateStable64((const TZrByte *)text, length);
    return (TZrUInt32)(hash & 0x7FFFFFFFu) + 1u;
}

static TZrBool metadata_token_string_heap_contains_index(const SZrMetadataStringHeapBuilder *builder,
                                                         TZrUInt32 stringIndex,
                                                         SZrString *value) {
    if (builder == ZR_NULL || stringIndex == 0u || value == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < builder->length; index++) {
        if (builder->entries[index].stringIndex == stringIndex &&
            builder->entries[index].value != ZR_NULL &&
            ZrCore_String_Equal(builder->entries[index].value, value)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool metadata_token_string_heap_reserve(SZrMetadataStringHeapBuilder *builder,
                                                  TZrUInt32 requiredCapacity) {
    SZrGlobalState *global;
    TZrUInt32 newCapacity;
    SZrMetadataStringHeapEntry *newEntries;

    if (builder == ZR_NULL || builder->compiler == ZR_NULL || builder->compiler->state == ZR_NULL ||
        builder->compiler->state->global == ZR_NULL) {
        return ZR_FALSE;
    }
    if (requiredCapacity <= builder->capacity) {
        return ZR_TRUE;
    }

    newCapacity = builder->capacity > 0u ? builder->capacity * 2u : 8u;
    while (newCapacity < requiredCapacity) {
        if (newCapacity > ZR_METADATA_TOKEN_RID_MASK / 2u) {
            return ZR_FALSE;
        }
        newCapacity *= 2u;
    }

    global = builder->compiler->state->global;
    newEntries = (SZrMetadataStringHeapEntry *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrMetadataStringHeapEntry) * newCapacity,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newEntries == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(newEntries, 0, sizeof(SZrMetadataStringHeapEntry) * newCapacity);
    if (builder->entries != ZR_NULL && builder->length > 0u) {
        ZrCore_Memory_RawCopy(newEntries,
                              builder->entries,
                              sizeof(SZrMetadataStringHeapEntry) * builder->length);
        ZrCore_Memory_RawFreeWithType(global,
                                      builder->entries,
                                      sizeof(SZrMetadataStringHeapEntry) * builder->capacity,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    builder->entries = newEntries;
    builder->capacity = newCapacity;
    return ZR_TRUE;
}

TZrBool metadata_token_string_heap_add(SZrMetadataStringHeapBuilder *builder, SZrString *value) {
    TZrUInt32 stringIndex;

    if (builder == ZR_NULL || metadata_token_string_length(value) == 0u) {
        return ZR_TRUE;
    }

    stringIndex = metadata_token_stable_string_index(value);
    if (stringIndex == 0u) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0; index < builder->length; index++) {
        if (builder->entries[index].stringIndex == stringIndex &&
            (builder->entries[index].value == ZR_NULL ||
             !ZrCore_String_Equal(builder->entries[index].value, value))) {
            return ZR_FALSE;
        }
    }
    if (metadata_token_string_heap_contains_index(builder, stringIndex, value)) {
        return ZR_TRUE;
    }
    if (!metadata_token_string_heap_reserve(builder, builder->length + 1u)) {
        return ZR_FALSE;
    }

    builder->entries[builder->length].stringIndex = stringIndex;
    builder->entries[builder->length].value = value;
    builder->length++;
    return ZR_TRUE;
}

TZrBool metadata_token_string_heap_collect_type(SZrMetadataStringHeapBuilder *builder,
                                                       const SZrFunctionTypedTypeRef *typeRef) {
    TZrNativeString typeNameText;

    if (builder == ZR_NULL || typeRef == ZR_NULL) {
        return ZR_TRUE;
    }

    if (typeRef->isArray) {
        SZrFunctionTypedTypeRef element;

        ZrCore_Memory_RawSet(&element, 0, sizeof(element));
        element.baseType = typeRef->elementBaseType;
        element.elementBaseType = ZR_VALUE_TYPE_OBJECT;
        element.typeName = typeRef->elementTypeName;
        return metadata_token_string_heap_collect_type(builder, &element);
    }

    typeNameText = metadata_token_native_string(typeRef->typeName);
    if (typeNameText != ZR_NULL && strlen(typeNameText) > 0u) {
        SZrString *baseName = ZR_NULL;
        SZrArray argumentTypeNames;

        if (builder->compiler != ZR_NULL &&
            builder->compiler->state != ZR_NULL &&
            try_parse_generic_instance_type_name(builder->compiler->state,
                                                 typeRef->typeName,
                                                 &baseName,
                                                 &argumentTypeNames)) {
            if (!metadata_token_string_heap_add(builder, baseName)) {
                ZrCore_Array_Free(builder->compiler->state, &argumentTypeNames);
                return ZR_FALSE;
            }
            for (TZrSize index = 0; index < argumentTypeNames.length; index++) {
                SZrString **argumentTypeNamePtr =
                        (SZrString **)ZrCore_Array_Get(&argumentTypeNames, index);
                SZrFunctionTypedTypeRef argumentTypeRef;

                ZrCore_Memory_RawSet(&argumentTypeRef, 0, sizeof(argumentTypeRef));
                argumentTypeRef.baseType = ZR_VALUE_TYPE_OBJECT;
                argumentTypeRef.elementBaseType = ZR_VALUE_TYPE_OBJECT;
                argumentTypeRef.typeName = argumentTypeNamePtr != ZR_NULL ? *argumentTypeNamePtr : ZR_NULL;
                if (!metadata_token_string_heap_collect_type(builder, &argumentTypeRef)) {
                    ZrCore_Array_Free(builder->compiler->state, &argumentTypeNames);
                    return ZR_FALSE;
                }
            }
            ZrCore_Array_Free(builder->compiler->state, &argumentTypeNames);
            return ZR_TRUE;
        }

        return metadata_token_string_heap_add(builder, typeRef->typeName);
    }

    return ZR_TRUE;
}

TZrBool metadata_token_string_heap_collect_external_type_ref_names(SZrMetadataStringHeapBuilder *builder,
                                                                          const SZrFunctionTypedTypeRef *typeRef) {
    SZrString *moduleName = ZR_NULL;
    SZrString *memberTypeName = ZR_NULL;
    SZrString *baseName = ZR_NULL;
    SZrArray argumentTypeNames;
    TZrBool parsedGeneric = ZR_FALSE;

    if (builder == ZR_NULL || typeRef == ZR_NULL) {
        return ZR_TRUE;
    }

    if (typeRef->isArray) {
        SZrFunctionTypedTypeRef element;

        ZrCore_Memory_RawSet(&element, 0, sizeof(element));
        element.baseType = typeRef->elementBaseType;
        element.elementBaseType = ZR_VALUE_TYPE_OBJECT;
        element.typeName = typeRef->elementTypeName;
        return metadata_token_string_heap_collect_external_type_ref_names(builder, &element);
    }

    if (typeRef->typeName == ZR_NULL || builder->compiler == ZR_NULL || builder->compiler->state == ZR_NULL) {
        return ZR_TRUE;
    }

    if (compiler_metadata_type_ref_split_module_qualified_type(builder->compiler,
                                                               typeRef->typeName,
                                                               &moduleName,
                                                               &memberTypeName)) {
        if (!metadata_token_string_heap_add(builder, moduleName)) {
            return ZR_FALSE;
        }
    } else if (compiler_metadata_type_ref_resolve_unqualified_alias(builder->compiler,
                                                                    typeRef->typeName,
                                                                    &moduleName,
                                                                    &memberTypeName)) {
        if (!metadata_token_string_heap_add(builder, moduleName)) {
            return ZR_FALSE;
        }
    } else {
        memberTypeName = typeRef->typeName;
    }

    if (try_parse_generic_instance_type_name(builder->compiler->state,
                                             memberTypeName,
                                             &baseName,
                                             &argumentTypeNames)) {
        parsedGeneric = ZR_TRUE;
    } else {
        baseName = memberTypeName;
    }

    if (!metadata_token_string_heap_add(builder, baseName)) {
        if (parsedGeneric) {
            ZrCore_Array_Free(builder->compiler->state, &argumentTypeNames);
        }
        return ZR_FALSE;
    }

    if (parsedGeneric) {
        for (TZrSize index = 0; index < argumentTypeNames.length; index++) {
            SZrString **argumentTypeNamePtr =
                    (SZrString **)ZrCore_Array_Get(&argumentTypeNames, index);
            SZrFunctionTypedTypeRef argumentTypeRef;

            ZrCore_Memory_RawSet(&argumentTypeRef, 0, sizeof(argumentTypeRef));
            argumentTypeRef.baseType = ZR_VALUE_TYPE_OBJECT;
            argumentTypeRef.elementBaseType = ZR_VALUE_TYPE_OBJECT;
            argumentTypeRef.typeName = argumentTypeNamePtr != ZR_NULL ? *argumentTypeNamePtr : ZR_NULL;
            if (!metadata_token_string_heap_collect_external_type_ref_names(builder, &argumentTypeRef)) {
                ZrCore_Array_Free(builder->compiler->state, &argumentTypeNames);
                return ZR_FALSE;
            }
        }
        ZrCore_Array_Free(builder->compiler->state, &argumentTypeNames);
    }

    return ZR_TRUE;
}

TZrBool metadata_token_string_heap_collect_symbol(SZrMetadataStringHeapBuilder *builder,
                                                         const SZrFunctionTypedExportSymbol *symbol) {
    if (symbol == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!metadata_token_string_heap_collect_type(builder, &symbol->valueType)) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0; index < symbol->parameterCount; index++) {
        if (!metadata_token_string_heap_collect_type(builder,
                                                     symbol->parameterTypes != ZR_NULL
                                                             ? &symbol->parameterTypes[index]
                                                             : ZR_NULL)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool metadata_token_type_def_string_collector(SZrCompilerState *cs,
                                                        SZrString *value,
                                                        void *userData) {
    SZrMetadataStringHeapBuilder *builder = (SZrMetadataStringHeapBuilder *)userData;

    ZR_UNUSED_PARAMETER(cs);

    return metadata_token_string_heap_add(builder, value);
}

TZrBool metadata_token_string_heap_collect_effect(SZrMetadataStringHeapBuilder *builder,
                                                         const SZrFunctionModuleEffect *effect,
                                                         const SZrMetadataTokenTargetSignature *targetSignature) {
    if (effect != ZR_NULL) {
        if (!metadata_token_string_heap_add(builder, effect->moduleName) ||
            !metadata_token_string_heap_add(builder, effect->assemblyName) ||
            !metadata_token_string_heap_add(builder, effect->symbolName) ||
            !metadata_token_string_heap_add(builder, effect->requestedModuleVersion) ||
            !metadata_token_string_heap_add(builder, effect->minModuleVersionInclusive) ||
            !metadata_token_string_heap_add(builder, effect->maxModuleVersionExclusive)) {
            return ZR_FALSE;
        }
    }

    if (targetSignature != ZR_NULL && targetSignature->hasSignature) {
        SZrFunctionTypedExportSymbol symbol;

        ZrCore_Memory_RawSet(&symbol, 0, sizeof(symbol));
        symbol.symbolKind = targetSignature->symbolKind;
        symbol.valueType = targetSignature->valueType;
        symbol.genericParameterCount = targetSignature->genericParameterCount;
        symbol.parameterCount = targetSignature->parameterCount;
        symbol.parameterTypes = targetSignature->parameterTypes;
        return metadata_token_string_heap_collect_symbol(builder, &symbol);
    }

    return ZR_TRUE;
}

static int metadata_token_compare_string_heap_entries(const SZrMetadataStringHeapEntry *left,
                                                      const SZrMetadataStringHeapEntry *right) {
    TZrNativeString leftText;
    TZrNativeString rightText;

    if (left == ZR_NULL || right == ZR_NULL) {
        return 0;
    }
    leftText = metadata_token_native_string(left->value);
    rightText = metadata_token_native_string(right->value);
    if (leftText == ZR_NULL) {
        leftText = "";
    }
    if (rightText == ZR_NULL) {
        rightText = "";
    }

    return strcmp(leftText, rightText);
}

void metadata_token_sort_string_heap(SZrMetadataStringHeapBuilder *builder) {
    if (builder == ZR_NULL || builder->entries == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 1; index < builder->length; index++) {
        SZrMetadataStringHeapEntry current = builder->entries[index];
        TZrUInt32 insert = index;

        while (insert > 0 &&
               metadata_token_compare_string_heap_entries(&builder->entries[insert - 1u], &current) > 0) {
            builder->entries[insert] = builder->entries[insert - 1u];
            insert--;
        }
        builder->entries[insert] = current;
    }
}

void metadata_token_string_heap_free(SZrCompilerState *cs, SZrMetadataStringHeapBuilder *builder) {
    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL || builder == ZR_NULL ||
        builder->entries == ZR_NULL || builder->capacity == 0u) {
        return;
    }

    ZrCore_Memory_RawFreeWithType(cs->state->global,
                                  builder->entries,
                                  sizeof(SZrMetadataStringHeapEntry) * builder->capacity,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    builder->entries = ZR_NULL;
    builder->length = 0;
    builder->capacity = 0;
}


TZrBool metadata_token_string_heap_compact(SZrMetadataStringHeapBuilder *builder) {
    SZrGlobalState *global;
    SZrMetadataStringHeapEntry *candidate;
    TZrSize oldBytes;
    TZrSize liveBytes;

    if (builder == ZR_NULL || builder->compiler == ZR_NULL ||
        builder->compiler->state == ZR_NULL || builder->compiler->state->global == ZR_NULL ||
        builder->length > builder->capacity ||
        (builder->capacity != 0u && builder->entries == ZR_NULL) ||
        (builder->capacity == 0u && builder->entries != ZR_NULL) ||
        (builder->capacity != 0u &&
         sizeof(*builder->entries) > ZR_MAX_SIZE / (TZrSize)builder->capacity)) {
        return ZR_FALSE;
    }
    if (builder->length == builder->capacity) {
        return ZR_TRUE;
    }
    global = builder->compiler->state->global;
    oldBytes = (TZrSize)builder->capacity * sizeof(*builder->entries);
    if (builder->length == 0u) {
        ZrCore_Memory_RawFreeWithType(global, builder->entries, oldBytes,
                                   ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        builder->entries = ZR_NULL;
        builder->capacity = 0u;
        return ZR_TRUE;
    }
    liveBytes = (TZrSize)builder->length * sizeof(*builder->entries);
    candidate = (SZrMetadataStringHeapEntry *)ZrCore_Memory_RawMallocWithType(
            global, liveBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (candidate == ZR_NULL) {
        return ZR_FALSE;
    }
    memcpy(candidate, builder->entries, liveBytes);
    ZrCore_Memory_RawFreeWithType(global, builder->entries, oldBytes,
                               ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    builder->entries = candidate;
    builder->capacity = builder->length;
    return ZR_TRUE;
}
