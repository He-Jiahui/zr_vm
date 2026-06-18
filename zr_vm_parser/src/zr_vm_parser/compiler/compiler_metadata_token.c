#include "compiler_internal.h"
#include "compiler_metadata_module_hash.h"
#include "compiler_metadata_module_record.h"
#include "compiler_metadata_ref.h"
#include "compiler_metadata_signature.h"
#include "compiler_metadata_type_def.h"
#include "compiler_metadata_type_ref.h"
#include "compiler_metadata_type_spec.h"
#include "module_init_analysis.h"
#include "type_inference_internal.h"

#include <string.h>

typedef struct SZrMetadataStringHeapBuilder {
    SZrCompilerState *compiler;
    SZrMetadataStringHeapEntry *entries;
    TZrUInt32 length;
    TZrUInt32 capacity;
} SZrMetadataStringHeapBuilder;

typedef struct SZrMetadataExplicitTypeRefModule {
    SZrString *moduleName;
    SZrString *requestedModuleVersion;
    SZrString *minModuleVersionInclusive;
    SZrString *maxModuleVersionExclusive;
} SZrMetadataExplicitTypeRefModule;

typedef struct SZrMetadataExplicitTypeRefModuleList {
    SZrCompilerState *compiler;
    SZrMetadataExplicitTypeRefModule *modules;
    TZrUInt32 length;
    TZrUInt32 capacity;
} SZrMetadataExplicitTypeRefModuleList;

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

static TZrBool metadata_token_string_heap_add(SZrMetadataStringHeapBuilder *builder, SZrString *value) {
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

static TZrBool metadata_token_string_heap_collect_type(SZrMetadataStringHeapBuilder *builder,
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

static TZrBool metadata_token_string_heap_collect_external_type_ref_names(SZrMetadataStringHeapBuilder *builder,
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

static TZrBool metadata_token_string_heap_collect_symbol(SZrMetadataStringHeapBuilder *builder,
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

static TZrBool metadata_token_type_def_string_collector(SZrCompilerState *cs,
                                                        SZrString *value,
                                                        void *userData) {
    SZrMetadataStringHeapBuilder *builder = (SZrMetadataStringHeapBuilder *)userData;

    ZR_UNUSED_PARAMETER(cs);

    return metadata_token_string_heap_add(builder, value);
}

static TZrBool metadata_token_string_heap_collect_effect(SZrMetadataStringHeapBuilder *builder,
                                                         const SZrFunctionModuleEffect *effect,
                                                         const SZrMetadataTokenTargetSignature *targetSignature) {
    if (effect != ZR_NULL) {
        if (!metadata_token_string_heap_add(builder, effect->moduleName) ||
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

static void metadata_token_sort_string_heap(SZrMetadataStringHeapBuilder *builder) {
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

static void metadata_token_string_heap_free(SZrCompilerState *cs, SZrMetadataStringHeapBuilder *builder) {
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

static TZrBool metadata_token_find_target_signature(SZrCompilerState *cs,
                                                    const SZrFunctionModuleEffect *effect,
                                                    SZrMetadataTokenTargetSignature *outSignature) {
    const SZrParserModuleInitSummary *summary;
    const SZrModuleInitExportInfo *exportInfo = ZR_NULL;

    if (outSignature != ZR_NULL) {
        ZrCore_Memory_RawSet(outSignature, 0, sizeof(*outSignature));
    }
    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL ||
        effect == ZR_NULL || effect->moduleName == ZR_NULL || effect->symbolName == ZR_NULL ||
        outSignature == ZR_NULL) {
        return ZR_FALSE;
    }

    summary = ZrParser_ModuleInitAnalysis_FindSummary(cs->state->global, effect->moduleName);
    if (summary == ZR_NULL) {
        if (!ZrParser_ModuleInitAnalysis_EnsureSummary(cs, effect->moduleName)) {
            return ZR_FALSE;
        }
        summary = ZrParser_ModuleInitAnalysis_FindSummary(cs->state->global, effect->moduleName);
    }
    if (summary == ZR_NULL ||
        summary->state == ZR_PARSER_MODULE_INIT_SUMMARY_FAILED ||
        !summary->exports.isValid) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < summary->exports.length; index++) {
        const SZrModuleInitExportInfo *candidate =
                (const SZrModuleInitExportInfo *)ZrCore_Array_Get((SZrArray *)&summary->exports, index);
        if (candidate == ZR_NULL ||
            candidate->name == ZR_NULL ||
            !ZrCore_String_Equal(candidate->name, effect->symbolName)) {
            continue;
        }

        if (exportInfo == ZR_NULL) {
            exportInfo = candidate;
        }
        if (effect->targetSignatureHash != 0u &&
            candidate->signatureHash != effect->targetSignatureHash) {
            continue;
        }
        if (effect->targetMetadataToken != 0u &&
            candidate->metadataToken != 0u &&
            candidate->metadataToken != effect->targetMetadataToken) {
            continue;
        }
        if (effect->targetSignatureToken != 0u &&
            candidate->signatureToken != 0u &&
            candidate->signatureToken != effect->targetSignatureToken) {
            continue;
        }

        exportInfo = candidate;
        break;
    }
    if (exportInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    outSignature->symbolKind = exportInfo->symbolKind;
    outSignature->exportKind = exportInfo->exportKind;
    outSignature->readiness = exportInfo->readiness;
    outSignature->valueType = exportInfo->valueType;
    outSignature->parameterCount = exportInfo->parameterCount;
    outSignature->parameterTypes = exportInfo->parameterTypes;
    outSignature->metadataToken = exportInfo->metadataToken;
    outSignature->signatureToken = exportInfo->signatureToken;
    outSignature->signatureHash = exportInfo->signatureHash;
    outSignature->hasSignature = ZR_TRUE;
    return ZR_TRUE;
}

static TZrSize metadata_token_target_method_sig_size(SZrCompilerState *cs,
                                                     const SZrMetadataTokenTargetSignature *targetSignature) {
    if (targetSignature == ZR_NULL || !targetSignature->hasSignature ||
        targetSignature->symbolKind != ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
        return 0;
    }

    return metadata_token_method_signature_size(cs,
                                                &targetSignature->valueType,
                                                targetSignature->parameterCount,
                                                targetSignature->parameterTypes);
}

static TZrSize metadata_token_target_field_sig_size(SZrCompilerState *cs,
                                                    const SZrMetadataTokenTargetSignature *targetSignature) {
    if (targetSignature == ZR_NULL || !targetSignature->hasSignature ||
        targetSignature->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
        return 0;
    }

    return metadata_token_field_signature_size(cs, &targetSignature->valueType);
}

static TZrSize metadata_token_target_signature_size(SZrCompilerState *cs,
                                                    const SZrMetadataTokenTargetSignature *targetSignature) {
    if (targetSignature == ZR_NULL || !targetSignature->hasSignature) {
        return 0;
    }

    if (targetSignature->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
        return metadata_token_target_method_sig_size(cs, targetSignature);
    }

    return metadata_token_target_field_sig_size(cs, targetSignature);
}

static TZrSize metadata_token_member_ref_signature_size(SZrCompilerState *cs,
                                                        const SZrFunctionModuleEffect *effect,
                                                        const SZrMetadataTokenTargetSignature *targetSignature) {
    ZR_UNUSED_PARAMETER(effect);
    return 1 + sizeof(TZrUInt32) + sizeof(TZrUInt32) + sizeof(TZrUInt32) +
           metadata_token_target_signature_size(cs, targetSignature);
}

static void metadata_token_write_target_method_sig(TZrByte *buffer,
                                                   TZrSize *offset,
                                                   SZrCompilerState *cs,
                                                   const SZrMetadataTokenTargetSignature *targetSignature,
                                                   const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                   TZrUInt32 stringHeapEntryCount) {
    metadata_token_write_method_signature(buffer,
                                          offset,
                                          cs,
                                          &targetSignature->valueType,
                                          targetSignature->parameterCount,
                                          targetSignature->parameterTypes,
                                          stringHeapEntries,
                                          stringHeapEntryCount);
}

static void metadata_token_write_target_field_sig(TZrByte *buffer,
                                                  TZrSize *offset,
                                                  SZrCompilerState *cs,
                                                  const SZrMetadataTokenTargetSignature *targetSignature,
                                                  const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                  TZrUInt32 stringHeapEntryCount) {
    metadata_token_write_field_signature(buffer,
                                         offset,
                                         cs,
                                         &targetSignature->valueType,
                                         stringHeapEntries,
                                         stringHeapEntryCount);
}

static void metadata_token_write_target_signature(TZrByte *buffer,
                                                  TZrSize *offset,
                                                  SZrCompilerState *cs,
                                                  const SZrMetadataTokenTargetSignature *targetSignature,
                                                  const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                  TZrUInt32 stringHeapEntryCount) {
    if (targetSignature == ZR_NULL || !targetSignature->hasSignature) {
        return;
    }

    if (targetSignature->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
        metadata_token_write_target_method_sig(buffer,
                                               offset,
                                               cs,
                                               targetSignature,
                                               stringHeapEntries,
                                               stringHeapEntryCount);
        return;
    }

    metadata_token_write_target_field_sig(buffer,
                                          offset,
                                          cs,
                                          targetSignature,
                                          stringHeapEntries,
                                          stringHeapEntryCount);
}

static TZrSize metadata_token_assembly_ref_signature_size(const SZrFunctionModuleEffect *effect) {
    ZR_UNUSED_PARAMETER(effect);
    return 1 + sizeof(TZrUInt32) + sizeof(TZrUInt32) + sizeof(TZrUInt32) + sizeof(TZrUInt32);
}

static TZrSize metadata_token_import_type_ref_signature_size(const SZrFunctionModuleEffect *effect) {
    ZR_UNUSED_PARAMETER(effect);
    return 1 + sizeof(TZrUInt32) + sizeof(TZrUInt32);
}

static void metadata_token_write_assembly_ref_signature(TZrByte *buffer,
                                                        TZrSize *offset,
                                                        const SZrFunctionModuleEffect *effect,
                                                        const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                        TZrUInt32 stringHeapEntryCount) {
    metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    effect != ZR_NULL ? effect->moduleName : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    effect != ZR_NULL ? effect->requestedModuleVersion : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    effect != ZR_NULL ? effect->minModuleVersionInclusive : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    effect != ZR_NULL ? effect->maxModuleVersionExclusive : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
}

static void metadata_token_write_explicit_assembly_ref_signature(
        TZrByte *buffer,
        TZrSize *offset,
        const SZrMetadataExplicitTypeRefModule *module,
        const SZrMetadataStringHeapEntry *stringHeapEntries,
        TZrUInt32 stringHeapEntryCount) {
    metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    module != ZR_NULL ? module->moduleName : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    module != ZR_NULL ? module->requestedModuleVersion : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    module != ZR_NULL ? module->minModuleVersionInclusive : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    module != ZR_NULL ? module->maxModuleVersionExclusive : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
}

static void metadata_token_write_import_type_ref_signature(TZrByte *buffer,
                                                           TZrSize *offset,
                                                           const SZrFunctionModuleEffect *effect,
                                                           const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                           TZrUInt32 stringHeapEntryCount) {
    metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_TYPE_REF);
    metadata_token_write_u32(buffer, offset, ZR_VALUE_TYPE_OBJECT);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    effect != ZR_NULL ? effect->symbolName : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
}

static void metadata_token_write_member_ref_signature(TZrByte *buffer,
                                                      TZrSize *offset,
                                                      SZrCompilerState *cs,
                                                      const SZrFunctionModuleEffect *effect,
                                                      const SZrMetadataTokenTargetSignature *targetSignature,
                                                      const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                      TZrUInt32 stringHeapEntryCount,
                                                      TZrSize *outTargetSignatureOffset,
                                                      TZrSize *outTargetSignatureLength) {
    TZrSize targetStart;

    if (outTargetSignatureOffset != ZR_NULL) {
        *outTargetSignatureOffset = 0;
    }
    if (outTargetSignatureLength != ZR_NULL) {
        *outTargetSignatureLength = 0;
    }

    metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_MEMBER_REF);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    effect != ZR_NULL ? effect->moduleName : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
    metadata_token_write_string_ref(buffer,
                                    offset,
                                    effect != ZR_NULL ? effect->symbolName : ZR_NULL,
                                    stringHeapEntries,
                                    stringHeapEntryCount);
    metadata_token_write_u32(buffer, offset, effect != ZR_NULL ? (TZrUInt32)effect->kind : 0u);
    if (targetSignature == ZR_NULL || !targetSignature->hasSignature) {
        return;
    }

    targetStart = *offset;
    metadata_token_write_target_signature(buffer,
                                          offset,
                                          cs,
                                          targetSignature,
                                          stringHeapEntries,
                                          stringHeapEntryCount);
    if (outTargetSignatureOffset != ZR_NULL) {
        *outTargetSignatureOffset = targetStart;
    }
    if (outTargetSignatureLength != ZR_NULL) {
        *outTargetSignatureLength = *offset - targetStart;
    }
}

static TZrBool metadata_token_effect_is_import_member_ref(const SZrFunctionModuleEffect *effect) {
    if (effect == ZR_NULL || metadata_token_string_length(effect->moduleName) == 0 ||
        metadata_token_string_length(effect->symbolName) == 0) {
        return ZR_FALSE;
    }

    return effect->kind == ZR_MODULE_ENTRY_EFFECT_IMPORT_REF ||
           effect->kind == ZR_MODULE_ENTRY_EFFECT_IMPORT_READ ||
           effect->kind == ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL;
}

static TZrUInt32 metadata_token_count_import_member_ref_effects(const SZrFunctionModuleEffect *effects,
                                                                TZrUInt32 effectCount) {
    TZrUInt32 count = 0;

    if (effects == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < effectCount; index++) {
        if (metadata_token_effect_is_import_member_ref(&effects[index])) {
            count++;
        }
    }

    return count;
}

static TZrUInt32 metadata_token_count_callable_import_member_ref_effects(const SZrFunction *function) {
    TZrUInt32 count = 0;

    if (function == ZR_NULL || function->exportedCallableSummaries == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < function->exportedCallableSummaryLength; index++) {
        count += metadata_token_count_import_member_ref_effects(function->exportedCallableSummaries[index].effects,
                                                                function->exportedCallableSummaries[index].effectCount);
    }

    return count;
}

static TZrBool metadata_token_effect_arrays_are_valid(const SZrFunction *function) {
    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((function->moduleEntryEffectLength > 0 && function->moduleEntryEffects == ZR_NULL) ||
        (function->exportedCallableSummaryLength > 0 && function->exportedCallableSummaries == ZR_NULL)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->exportedCallableSummaryLength; index++) {
        if (function->exportedCallableSummaries[index].effectCount > 0 &&
            function->exportedCallableSummaries[index].effects == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static const SZrFunctionModuleEffect *metadata_token_get_import_member_ref_effect_by_flat_index(
        const SZrFunction *function,
        TZrUInt32 effectIndex) {
    TZrUInt32 cursor;

    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    cursor = effectIndex;
    if (function->moduleEntryEffects != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->moduleEntryEffectLength; index++) {
            if (!metadata_token_effect_is_import_member_ref(&function->moduleEntryEffects[index])) {
                continue;
            }
            if (cursor == 0) {
                return &function->moduleEntryEffects[index];
            }
            cursor--;
        }
    }

    for (TZrUInt32 index = 0; index < function->exportedCallableSummaryLength; index++) {
        const SZrFunctionCallableSummary *summary =
                function->exportedCallableSummaries != ZR_NULL ? &function->exportedCallableSummaries[index] : ZR_NULL;

        if (summary == ZR_NULL || summary->effects == ZR_NULL) {
            continue;
        }
        for (TZrUInt32 effectCursor = 0; effectCursor < summary->effectCount; effectCursor++) {
            if (!metadata_token_effect_is_import_member_ref(&summary->effects[effectCursor])) {
                continue;
            }
            if (cursor == 0) {
                return &summary->effects[effectCursor];
            }
            cursor--;
        }
    }

    return ZR_NULL;
}

static TZrBool metadata_token_effect_module_seen_before(const SZrFunction *function,
                                                        TZrUInt32 effectIndex,
                                                        SZrString *moduleName) {
    if (function == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < effectIndex; index++) {
        const SZrFunctionModuleEffect *effect =
                metadata_token_get_import_member_ref_effect_by_flat_index(function, index);
        if (effect != ZR_NULL &&
            effect->moduleName != ZR_NULL &&
            ZrCore_String_Equal(effect->moduleName, moduleName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool metadata_token_effect_type_seen_before(const SZrFunction *function,
                                                      TZrUInt32 effectIndex,
                                                      SZrString *moduleName,
                                                      SZrString *symbolName) {
    if (function == ZR_NULL || moduleName == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < effectIndex; index++) {
        const SZrFunctionModuleEffect *effect =
                metadata_token_get_import_member_ref_effect_by_flat_index(function, index);
        if (effect != ZR_NULL &&
            effect->moduleName != ZR_NULL &&
            effect->symbolName != ZR_NULL &&
            ZrCore_String_Equal(effect->moduleName, moduleName) &&
            ZrCore_String_Equal(effect->symbolName, symbolName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrUInt32 metadata_token_count_unique_import_modules(const SZrFunction *function,
                                                            TZrUInt32 totalEffectCount) {
    TZrUInt32 count = 0;

    for (TZrUInt32 index = 0; index < totalEffectCount; index++) {
        const SZrFunctionModuleEffect *effect =
                metadata_token_get_import_member_ref_effect_by_flat_index(function, index);
        if (effect == ZR_NULL ||
            effect->moduleName == ZR_NULL ||
            metadata_token_effect_module_seen_before(function, index, effect->moduleName)) {
            continue;
        }
        count++;
    }

    return count;
}

static TZrUInt32 metadata_token_count_unique_import_types(const SZrFunction *function,
                                                          TZrUInt32 totalEffectCount) {
    TZrUInt32 count = 0;

    for (TZrUInt32 index = 0; index < totalEffectCount; index++) {
        const SZrFunctionModuleEffect *effect =
                metadata_token_get_import_member_ref_effect_by_flat_index(function, index);
        if (effect == ZR_NULL ||
            effect->moduleName == ZR_NULL ||
            effect->symbolName == ZR_NULL ||
            metadata_token_effect_type_seen_before(function, index, effect->moduleName, effect->symbolName)) {
            continue;
        }
        count++;
    }

    return count;
}

static TZrUInt32 metadata_token_get_import_module_ref_rid(const SZrFunction *function,
                                                          TZrUInt32 totalEffectCount,
                                                          SZrString *moduleName) {
    TZrUInt32 rid = 1u;

    if (function == ZR_NULL || moduleName == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < totalEffectCount; index++) {
        const SZrFunctionModuleEffect *effect =
                metadata_token_get_import_member_ref_effect_by_flat_index(function, index);
        if (effect == ZR_NULL ||
            effect->moduleName == ZR_NULL ||
            metadata_token_effect_module_seen_before(function, index, effect->moduleName)) {
            continue;
        }
        if (ZrCore_String_Equal(effect->moduleName, moduleName)) {
            return rid;
        }
        rid++;
    }

    return 0;
}

static TZrUInt32 metadata_token_get_import_type_ref_rid(const SZrFunction *function,
                                                        TZrUInt32 totalEffectCount,
                                                        SZrString *moduleName,
                                                        SZrString *symbolName) {
    TZrUInt32 rid = 1u;

    if (function == ZR_NULL || moduleName == ZR_NULL || symbolName == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < totalEffectCount; index++) {
        const SZrFunctionModuleEffect *effect =
                metadata_token_get_import_member_ref_effect_by_flat_index(function, index);
        if (effect == ZR_NULL ||
            effect->moduleName == ZR_NULL ||
            effect->symbolName == ZR_NULL ||
            metadata_token_effect_type_seen_before(function, index, effect->moduleName, effect->symbolName)) {
            continue;
        }
        if (ZrCore_String_Equal(effect->moduleName, moduleName) &&
            ZrCore_String_Equal(effect->symbolName, symbolName)) {
            return rid;
        }
        rid++;
    }

    return 0;
}

static TZrBool metadata_token_explicit_module_list_reserve(SZrMetadataExplicitTypeRefModuleList *list,
                                                           TZrUInt32 requiredCapacity) {
    SZrGlobalState *global;
    TZrUInt32 newCapacity;
    SZrMetadataExplicitTypeRefModule *newModules;

    if (list == ZR_NULL || list->compiler == ZR_NULL || list->compiler->state == ZR_NULL ||
        list->compiler->state->global == ZR_NULL) {
        return ZR_FALSE;
    }
    if (requiredCapacity <= list->capacity) {
        return ZR_TRUE;
    }

    newCapacity = list->capacity > 0u ? list->capacity * 2u : 4u;
    while (newCapacity < requiredCapacity) {
        if (newCapacity > ZR_METADATA_TOKEN_RID_MASK / 2u) {
            return ZR_FALSE;
        }
        newCapacity *= 2u;
    }

    global = list->compiler->state->global;
    newModules = (SZrMetadataExplicitTypeRefModule *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrMetadataExplicitTypeRefModule) * newCapacity,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newModules == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(newModules, 0, sizeof(SZrMetadataExplicitTypeRefModule) * newCapacity);
    if (list->modules != ZR_NULL && list->length > 0u) {
        ZrCore_Memory_RawCopy(newModules,
                              list->modules,
                              sizeof(SZrMetadataExplicitTypeRefModule) * list->length);
        ZrCore_Memory_RawFreeWithType(global,
                                      list->modules,
                                      sizeof(SZrMetadataExplicitTypeRefModule) * list->capacity,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    list->modules = newModules;
    list->capacity = newCapacity;
    return ZR_TRUE;
}

static void metadata_token_explicit_module_list_free(SZrMetadataExplicitTypeRefModuleList *list) {
    if (list == ZR_NULL || list->compiler == ZR_NULL || list->compiler->state == ZR_NULL ||
        list->compiler->state->global == ZR_NULL || list->modules == ZR_NULL || list->capacity == 0u) {
        return;
    }

    ZrCore_Memory_RawFreeWithType(list->compiler->state->global,
                                  list->modules,
                                  sizeof(SZrMetadataExplicitTypeRefModule) * list->capacity,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    list->modules = ZR_NULL;
    list->length = 0;
    list->capacity = 0;
}

static TZrBool metadata_token_explicit_module_seen(const SZrMetadataExplicitTypeRefModuleList *list,
                                                   SZrString *moduleName) {
    if (list == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < list->length; index++) {
        if (list->modules[index].moduleName != ZR_NULL &&
            ZrCore_String_Equal(list->modules[index].moduleName, moduleName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool metadata_token_explicit_module_list_add(SZrMetadataExplicitTypeRefModuleList *list,
                                                       const SZrFunction *function,
                                                       TZrUInt32 totalEffectCount,
                                                       SZrString *moduleName) {
    if (list == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_TRUE;
    }
    if (metadata_token_get_import_module_ref_rid(function, totalEffectCount, moduleName) != 0u ||
        metadata_token_explicit_module_seen(list, moduleName)) {
        return ZR_TRUE;
    }
    if (!metadata_token_explicit_module_list_reserve(list, list->length + 1u)) {
        return ZR_FALSE;
    }

    list->modules[list->length].moduleName = moduleName;
    list->modules[list->length].requestedModuleVersion = ZR_NULL;
    list->modules[list->length].minModuleVersionInclusive = ZR_NULL;
    list->modules[list->length].maxModuleVersionExclusive = ZR_NULL;
    list->length++;
    return ZR_TRUE;
}

static TZrBool metadata_token_collect_explicit_modules_from_type(SZrMetadataExplicitTypeRefModuleList *list,
                                                                 const SZrFunction *function,
                                                                 TZrUInt32 totalEffectCount,
                                                                 const SZrFunctionTypedTypeRef *typeRef) {
    SZrString *moduleName = ZR_NULL;
    SZrString *memberTypeName = ZR_NULL;
    SZrString *genericBaseName = ZR_NULL;
    SZrArray genericArgumentTypeNames;
    TZrBool parsedGeneric;

    if (list == ZR_NULL || typeRef == ZR_NULL) {
        return ZR_TRUE;
    }

    if (typeRef->isArray) {
        SZrFunctionTypedTypeRef element;

        ZrCore_Memory_RawSet(&element, 0, sizeof(element));
        element.baseType = typeRef->elementBaseType;
        element.elementBaseType = ZR_VALUE_TYPE_OBJECT;
        element.typeName = typeRef->elementTypeName;
        return metadata_token_collect_explicit_modules_from_type(list, function, totalEffectCount, &element);
    }

    if (typeRef->typeName == ZR_NULL || list->compiler == ZR_NULL || list->compiler->state == ZR_NULL) {
        return ZR_TRUE;
    }

    if (compiler_metadata_type_ref_split_module_qualified_type(list->compiler,
                                                               typeRef->typeName,
                                                               &moduleName,
                                                               &memberTypeName)) {
        if (!metadata_token_explicit_module_list_add(list, function, totalEffectCount, moduleName)) {
            return ZR_FALSE;
        }
    } else if (compiler_metadata_type_ref_resolve_unqualified_alias(list->compiler,
                                                                    typeRef->typeName,
                                                                    &moduleName,
                                                                    &memberTypeName)) {
        if (!metadata_token_explicit_module_list_add(list, function, totalEffectCount, moduleName)) {
            return ZR_FALSE;
        }
    } else {
        memberTypeName = typeRef->typeName;
    }

    parsedGeneric = try_parse_generic_instance_type_name(list->compiler->state,
                                                         memberTypeName,
                                                         &genericBaseName,
                                                         &genericArgumentTypeNames);
    if (parsedGeneric) {
        for (TZrSize index = 0; index < genericArgumentTypeNames.length; index++) {
            SZrString **argumentTypeNamePtr =
                    (SZrString **)ZrCore_Array_Get(&genericArgumentTypeNames, index);
            SZrFunctionTypedTypeRef argumentTypeRef;

            ZrCore_Memory_RawSet(&argumentTypeRef, 0, sizeof(argumentTypeRef));
            argumentTypeRef.baseType = ZR_VALUE_TYPE_OBJECT;
            argumentTypeRef.elementBaseType = ZR_VALUE_TYPE_OBJECT;
            argumentTypeRef.typeName = argumentTypeNamePtr != ZR_NULL ? *argumentTypeNamePtr : ZR_NULL;
            if (!metadata_token_collect_explicit_modules_from_type(list,
                                                                   function,
                                                                   totalEffectCount,
                                                                   &argumentTypeRef)) {
                ZrCore_Array_Free(list->compiler->state, &genericArgumentTypeNames);
                return ZR_FALSE;
            }
        }
        ZrCore_Array_Free(list->compiler->state, &genericArgumentTypeNames);
    }

    return ZR_TRUE;
}

static TZrBool metadata_token_collect_explicit_type_ref_modules(SZrCompilerState *cs,
                                                                const SZrFunction *function,
                                                                TZrUInt32 totalEffectCount,
                                                                SZrMetadataExplicitTypeRefModuleList *outList) {
    if (outList != ZR_NULL) {
        ZrCore_Memory_RawSet(outList, 0, sizeof(*outList));
        outList->compiler = cs;
    }
    if (cs == ZR_NULL || function == ZR_NULL || outList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->typedLocalBindingLength; index++) {
        if (!metadata_token_collect_explicit_modules_from_type(
                    outList,
                    function,
                    totalEffectCount,
                    function->typedLocalBindings != ZR_NULL ? &function->typedLocalBindings[index].type : ZR_NULL)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrUInt32 metadata_token_resolve_assembly_ref_rid(SZrCompilerState *cs,
                                                         const SZrFunction *function,
                                                         TZrUInt32 totalEffectCount,
                                                         TZrMetadataTypeRefEffectByFlatIndex effectAt,
                                                         SZrString *moduleName,
                                                         void *userData) {
    const SZrMetadataExplicitTypeRefModuleList *explicitModules =
            (const SZrMetadataExplicitTypeRefModuleList *)userData;
    TZrUInt32 importRid;
    TZrUInt32 importAssemblyCount;

    ZR_UNUSED_PARAMETER(cs);
    ZR_UNUSED_PARAMETER(effectAt);

    importRid = metadata_token_get_import_module_ref_rid(function, totalEffectCount, moduleName);
    if (importRid != 0u) {
        return importRid;
    }
    if (explicitModules == ZR_NULL || moduleName == ZR_NULL) {
        return 0u;
    }

    importAssemblyCount = metadata_token_count_unique_import_modules(function, totalEffectCount);
    for (TZrUInt32 index = 0; index < explicitModules->length; index++) {
        if (explicitModules->modules[index].moduleName != ZR_NULL &&
            ZrCore_String_Equal(explicitModules->modules[index].moduleName, moduleName)) {
            return importAssemblyCount + index + 1u;
        }
    }

    return 0u;
}

static TZrBool metadata_token_write_record_pair(SZrMetadataTokenRecord *records,
                                                TZrUInt32 recordCount,
                                                TZrUInt32 *ioRecordIndex,
                                                TZrMetadataToken token,
                                                TZrMetadataToken signatureToken,
                                                TZrMetadataToken ownerToken,
                                                TZrUInt32 ownerIndex,
                                                TZrUInt32 signatureBlobOffset,
                                                TZrUInt32 signatureBlobLength,
                                                TZrUInt64 signatureHash,
                                                TZrMetadataToken targetMetadataToken,
                                                TZrMetadataToken targetSignatureToken,
                                                TZrUInt64 targetSignatureHash,
                                                TZrUInt64 targetModuleSignatureHash,
                                                SZrString *requestedModuleVersion,
                                                SZrString *minModuleVersionInclusive,
                                                SZrString *maxModuleVersionExclusive) {
    TZrUInt32 recordIndex;

    if (records == ZR_NULL || ioRecordIndex == ZR_NULL || *ioRecordIndex + 1u >= recordCount) {
        return ZR_FALSE;
    }

    recordIndex = *ioRecordIndex;
    records[recordIndex].token = token;
    records[recordIndex].relatedToken = signatureToken;
    records[recordIndex].ownerToken = ownerToken;
    records[recordIndex].ownerIndex = ownerIndex;
    records[recordIndex].signatureBlobOffset = signatureBlobOffset;
    records[recordIndex].signatureBlobLength = signatureBlobLength;
    records[recordIndex].signatureHash = signatureHash;
    records[recordIndex].layoutVersion = 0;
    records[recordIndex].reserved0 = 0;
    records[recordIndex].layoutHash = 0;
    records[recordIndex].targetMetadataToken = targetMetadataToken;
    records[recordIndex].targetSignatureToken = targetSignatureToken;
    records[recordIndex].targetSignatureHash = targetSignatureHash;
    records[recordIndex].targetModuleSignatureHash = targetModuleSignatureHash;
    records[recordIndex].requestedModuleVersion = requestedModuleVersion;
    records[recordIndex].minModuleVersionInclusive = minModuleVersionInclusive;
    records[recordIndex].maxModuleVersionExclusive = maxModuleVersionExclusive;
    recordIndex++;

    records[recordIndex].token = signatureToken;
    records[recordIndex].relatedToken = token;
    records[recordIndex].ownerToken = token;
    records[recordIndex].ownerIndex = ownerIndex;
    records[recordIndex].signatureBlobOffset = signatureBlobOffset;
    records[recordIndex].signatureBlobLength = signatureBlobLength;
    records[recordIndex].signatureHash = signatureHash;
    records[recordIndex].layoutVersion = 0;
    records[recordIndex].reserved0 = 0;
    records[recordIndex].layoutHash = 0;
    records[recordIndex].targetMetadataToken = targetMetadataToken;
    records[recordIndex].targetSignatureToken = targetSignatureToken;
    records[recordIndex].targetSignatureHash = targetSignatureHash;
    records[recordIndex].targetModuleSignatureHash = targetModuleSignatureHash;
    records[recordIndex].requestedModuleVersion = requestedModuleVersion;
    records[recordIndex].minModuleVersionInclusive = minModuleVersionInclusive;
    records[recordIndex].maxModuleVersionExclusive = maxModuleVersionExclusive;
    recordIndex++;

    *ioRecordIndex = recordIndex;
    return ZR_TRUE;
}

static void metadata_token_clear_function(SZrCompilerState *cs, SZrFunction *function) {
    SZrGlobalState *global;

    if (cs == ZR_NULL || function == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL) {
        return;
    }

    global = cs->state->global;
    if (function->metadataTokenRecords != ZR_NULL && function->metadataTokenRecordLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->metadataTokenRecords,
                                      sizeof(SZrMetadataTokenRecord) * function->metadataTokenRecordLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->signatureBlobHeap != ZR_NULL && function->signatureBlobHeapLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->signatureBlobHeap,
                                      function->signatureBlobHeapLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->metadataStringHeap != ZR_NULL && function->metadataStringHeapLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->metadataStringHeap,
                                      sizeof(SZrMetadataStringHeapEntry) * function->metadataStringHeapLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->moduleMetadataTokenRecords != ZR_NULL && function->moduleMetadataTokenRecordLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->moduleMetadataTokenRecords,
                                      sizeof(SZrMetadataTokenRecord) * function->moduleMetadataTokenRecordLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    function->metadataTokenRecords = ZR_NULL;
    function->metadataTokenRecordLength = 0;
    function->moduleMetadataTokenRecords = ZR_NULL;
    function->moduleMetadataTokenRecordLength = 0;
    function->signatureBlobHeap = ZR_NULL;
    function->signatureBlobHeapLength = 0;
    function->metadataStringHeap = ZR_NULL;
    function->metadataStringHeapLength = 0;
    function->moduleSignatureHash = 0;
    for (TZrUInt32 index = 0; index < function->typedExportedSymbolLength; index++) {
        function->typedExportedSymbols[index].metadataToken = 0;
        function->typedExportedSymbols[index].signatureToken = 0;
        function->typedExportedSymbols[index].signatureBlobOffset = 0;
        function->typedExportedSymbols[index].signatureBlobLength = 0;
        function->typedExportedSymbols[index].signatureHash = 0;
    }
}

static TZrBool metadata_token_build_string_heap(SZrCompilerState *cs,
                                                SZrFunction *function,
                                                TZrUInt32 totalEffectCount,
                                                SZrMetadataStringHeapBuilder *builder) {
    if (builder == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(builder, 0, sizeof(*builder));
    builder->compiler = cs;
    if (cs == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!metadata_token_string_heap_add(builder, function->moduleVersion)) {
        return ZR_FALSE;
    }
    if (!compiler_metadata_module_record_collect_strings(cs,
                                                         function,
                                                         metadata_token_type_def_string_collector,
                                                         builder)) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0; index < function->typedExportedSymbolLength; index++) {
        if (!metadata_token_string_heap_add(builder, function->typedExportedSymbols[index].name) ||
            !metadata_token_string_heap_collect_symbol(builder, &function->typedExportedSymbols[index])) {
            return ZR_FALSE;
        }
    }
    for (TZrUInt32 index = 0; index < function->typedLocalBindingLength; index++) {
        if (!metadata_token_string_heap_collect_type(builder,
                                                     function->typedLocalBindings != ZR_NULL
                                                             ? &function->typedLocalBindings[index].type
                                                             : ZR_NULL) ||
            !metadata_token_string_heap_collect_external_type_ref_names(
                    builder,
                    function->typedLocalBindings != ZR_NULL ? &function->typedLocalBindings[index].type : ZR_NULL)) {
            return ZR_FALSE;
        }
    }
    if (!compiler_metadata_type_def_collect_strings(cs,
                                                    function,
                                                    metadata_token_type_def_string_collector,
                                                    builder)) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0; index < totalEffectCount; index++) {
        const SZrFunctionModuleEffect *effect =
                metadata_token_get_import_member_ref_effect_by_flat_index(function, index);
        SZrMetadataTokenTargetSignature targetSignature;

        (void)metadata_token_find_target_signature(cs, effect, &targetSignature);
        if (!metadata_token_string_heap_collect_effect(builder, effect, &targetSignature)) {
            return ZR_FALSE;
        }
    }

    metadata_token_sort_string_heap(builder);
    return ZR_TRUE;
}

ZR_PARSER_API TZrBool compiler_build_function_metadata_tokens(SZrCompilerState *cs, SZrFunction *function) {
    SZrGlobalState *global;
    TZrUInt32 exportCount;
    TZrUInt32 effectCount;
    TZrUInt32 callableEffectCount;
    TZrUInt32 totalEffectCount;
    TZrUInt32 assemblyRefCount;
    TZrUInt32 typeRefCount;
    TZrUInt32 ownerTypeRefCount;
    SZrMetadataModuleRecordPlan moduleRecordPlan;
    SZrMetadataTypeDefPlan typeDefPlan;
    SZrMetadataExternalTypeRefPlan externalTypeRefPlan;
    SZrMetadataTypeSpecPlan typeSpecPlan;
    SZrMetadataStringHeapBuilder stringHeapBuilder;
    SZrMetadataExplicitTypeRefModuleList explicitTypeRefModules;
    TZrUInt32 typeDefCount;
    TZrUInt32 typeSpecCount;
    TZrUInt32 moduleRecordCount;
    TZrUInt32 recordCount;
    TZrSize heapLength = 0;
    SZrMetadataTokenRecord *records;
    TZrByte *heap;
    TZrUInt32 recordIndex = 0;
    TZrUInt32 signatureRidCursor;
    TZrUInt32 assemblyRefRidCursor = 1u;
    TZrUInt32 typeRefRidCursor = 1u;
    TZrSize heapOffset = 0;

    if (cs == ZR_NULL || function == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    metadata_token_clear_function(cs, function);
    if (!metadata_token_effect_arrays_are_valid(function)) {
        return ZR_FALSE;
    }
    exportCount = function->typedExportedSymbolLength;
    effectCount =
            metadata_token_count_import_member_ref_effects(function->moduleEntryEffects, function->moduleEntryEffectLength);
    callableEffectCount = metadata_token_count_callable_import_member_ref_effects(function);
    totalEffectCount = effectCount + callableEffectCount;
    if (exportCount == 0 && totalEffectCount == 0 && function->typedLocalBindingLength == 0u) {
        return ZR_TRUE;
    }
    if (!metadata_token_build_string_heap(cs, function, totalEffectCount, &stringHeapBuilder)) {
        metadata_token_string_heap_free(cs, &stringHeapBuilder);
        return ZR_FALSE;
    }
    if (!metadata_token_collect_explicit_type_ref_modules(cs,
                                                          function,
                                                          totalEffectCount,
                                                          &explicitTypeRefModules)) {
        metadata_token_explicit_module_list_free(&explicitTypeRefModules);
        metadata_token_string_heap_free(cs, &stringHeapBuilder);
        return ZR_FALSE;
    }
    assemblyRefCount = metadata_token_count_unique_import_modules(function, totalEffectCount);
    if (explicitTypeRefModules.length > ZR_METADATA_TOKEN_RID_MASK ||
        explicitTypeRefModules.length > ZR_METADATA_TOKEN_RID_MASK - assemblyRefCount) {
        metadata_token_explicit_module_list_free(&explicitTypeRefModules);
        metadata_token_string_heap_free(cs, &stringHeapBuilder);
        return ZR_FALSE;
    }
    assemblyRefCount += explicitTypeRefModules.length;
    ownerTypeRefCount = metadata_token_count_unique_import_types(function, totalEffectCount);
    typeRefCount = ownerTypeRefCount;
    if ((exportCount > 0 && function->typedExportedSymbols == ZR_NULL) ||
        (callableEffectCount > 0 && function->exportedCallableSummaries == ZR_NULL) ||
        !compiler_metadata_module_record_plan(cs, function, &moduleRecordPlan) ||
        !compiler_metadata_type_def_plan(cs, function, &typeDefPlan) ||
        !compiler_metadata_type_ref_plan(cs,
                                         function,
                                         totalEffectCount,
                                         metadata_token_get_import_member_ref_effect_by_flat_index,
                                         metadata_token_find_target_signature,
                                         stringHeapBuilder.entries,
                                         stringHeapBuilder.length,
                                         &externalTypeRefPlan) ||
        !compiler_metadata_type_spec_plan(cs,
                                          function,
                                          stringHeapBuilder.entries,
                                          stringHeapBuilder.length,
                                          &typeSpecPlan) ||
        externalTypeRefPlan.typeRefCount > ZR_METADATA_TOKEN_RID_MASK ||
        externalTypeRefPlan.typeRefCount > ZR_METADATA_TOKEN_RID_MASK - typeRefCount ||
        (typeRefCount += externalTypeRefPlan.typeRefCount) > ZR_METADATA_TOKEN_RID_MASK ||
        moduleRecordPlan.moduleRecordCount > ZR_METADATA_TOKEN_RID_MASK ||
        exportCount > ZR_METADATA_TOKEN_RID_MASK ||
        totalEffectCount > ZR_METADATA_TOKEN_RID_MASK ||
        assemblyRefCount > ZR_METADATA_TOKEN_RID_MASK ||
        typeRefCount > ZR_METADATA_TOKEN_RID_MASK ||
        typeDefPlan.typeDefCount > ZR_METADATA_TOKEN_RID_MASK ||
        typeSpecPlan.typeSpecCount > ZR_METADATA_TOKEN_RID_MASK ||
        moduleRecordPlan.moduleRecordCount > ZR_METADATA_TOKEN_RID_MASK - exportCount ||
        assemblyRefCount > ZR_METADATA_TOKEN_RID_MASK - exportCount ||
        typeRefCount > ZR_METADATA_TOKEN_RID_MASK - exportCount - assemblyRefCount ||
        typeDefPlan.typeDefCount >
                ZR_METADATA_TOKEN_RID_MASK - exportCount - assemblyRefCount - typeRefCount ||
        typeSpecPlan.typeSpecCount >
                ZR_METADATA_TOKEN_RID_MASK - exportCount - assemblyRefCount - typeRefCount -
                        typeDefPlan.typeDefCount ||
        totalEffectCount >
                ZR_METADATA_TOKEN_RID_MASK - exportCount - assemblyRefCount - typeRefCount -
                        typeDefPlan.typeDefCount - typeSpecPlan.typeSpecCount) {
        metadata_token_explicit_module_list_free(&explicitTypeRefModules);
        metadata_token_string_heap_free(cs, &stringHeapBuilder);
        return ZR_FALSE;
    }
    moduleRecordCount = moduleRecordPlan.moduleRecordCount;
    typeDefCount = typeDefPlan.typeDefCount;
    typeSpecCount = typeSpecPlan.typeSpecCount;
    if (moduleRecordCount == 0u && exportCount == 0u && typeDefCount == 0u && typeSpecCount == 0u &&
        assemblyRefCount == 0u && typeRefCount == 0u && totalEffectCount == 0u) {
        metadata_token_explicit_module_list_free(&explicitTypeRefModules);
        metadata_token_string_heap_free(cs, &stringHeapBuilder);
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0; index < exportCount; index++) {
        TZrSize signatureLength = metadata_token_symbol_signature_size(cs, &function->typedExportedSymbols[index]);
        if (signatureLength == 0 || signatureLength > (TZrSize)0xFFFFFFFFu ||
            heapLength > (TZrSize)0xFFFFFFFFu - signatureLength) {
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
        heapLength += signatureLength;
    }
    if (moduleRecordPlan.signatureHeapLength > 0) {
        if (moduleRecordPlan.signatureHeapLength > (TZrSize)0xFFFFFFFFu ||
            heapLength > (TZrSize)0xFFFFFFFFu - moduleRecordPlan.signatureHeapLength) {
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
        heapLength += moduleRecordPlan.signatureHeapLength;
    }
    if (typeSpecPlan.signatureHeapLength > 0) {
        if (typeSpecPlan.signatureHeapLength > (TZrSize)0xFFFFFFFFu ||
            heapLength > (TZrSize)0xFFFFFFFFu - typeSpecPlan.signatureHeapLength) {
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
        heapLength += typeSpecPlan.signatureHeapLength;
    }
    if (typeDefPlan.signatureHeapLength > 0) {
        if (typeDefPlan.signatureHeapLength > (TZrSize)0xFFFFFFFFu ||
            heapLength > (TZrSize)0xFFFFFFFFu - typeDefPlan.signatureHeapLength) {
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
        heapLength += typeDefPlan.signatureHeapLength;
    }
    if (externalTypeRefPlan.signatureHeapLength > 0) {
        if (externalTypeRefPlan.signatureHeapLength > (TZrSize)0xFFFFFFFFu ||
            heapLength > (TZrSize)0xFFFFFFFFu - externalTypeRefPlan.signatureHeapLength) {
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
        heapLength += externalTypeRefPlan.signatureHeapLength;
    }
    for (TZrUInt32 index = 0; index < totalEffectCount; index++) {
        const SZrFunctionModuleEffect *effect =
                metadata_token_get_import_member_ref_effect_by_flat_index(function, index);

        if (effect != ZR_NULL &&
            effect->moduleName != ZR_NULL &&
            !metadata_token_effect_module_seen_before(function, index, effect->moduleName)) {
            TZrSize signatureLength = metadata_token_assembly_ref_signature_size(effect);
            if (signatureLength == 0 || signatureLength > (TZrSize)0xFFFFFFFFu ||
                heapLength > (TZrSize)0xFFFFFFFFu - signatureLength) {
                metadata_token_explicit_module_list_free(&explicitTypeRefModules);
                metadata_token_string_heap_free(cs, &stringHeapBuilder);
                return ZR_FALSE;
            }
            heapLength += signatureLength;
        }
        if (effect != ZR_NULL &&
            effect->moduleName != ZR_NULL &&
            effect->symbolName != ZR_NULL &&
            !metadata_token_effect_type_seen_before(function, index, effect->moduleName, effect->symbolName)) {
            TZrSize signatureLength = metadata_token_import_type_ref_signature_size(effect);
            if (signatureLength == 0 || signatureLength > (TZrSize)0xFFFFFFFFu ||
                heapLength > (TZrSize)0xFFFFFFFFu - signatureLength) {
                metadata_token_explicit_module_list_free(&explicitTypeRefModules);
                metadata_token_string_heap_free(cs, &stringHeapBuilder);
                return ZR_FALSE;
            }
            heapLength += signatureLength;
        }
    }
    for (TZrUInt32 index = 0; index < explicitTypeRefModules.length; index++) {
        TZrSize signatureLength = metadata_token_assembly_ref_signature_size(ZR_NULL);
        if (signatureLength == 0 || signatureLength > (TZrSize)0xFFFFFFFFu ||
            heapLength > (TZrSize)0xFFFFFFFFu - signatureLength) {
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
        heapLength += signatureLength;
    }
    for (TZrUInt32 index = 0; index < totalEffectCount; index++) {
        const SZrFunctionModuleEffect *effect = metadata_token_get_import_member_ref_effect_by_flat_index(function, index);
        SZrMetadataTokenTargetSignature targetSignature;
        TZrSize signatureLength;

        (void)metadata_token_find_target_signature(cs, effect, &targetSignature);
        signatureLength = metadata_token_member_ref_signature_size(cs, effect, &targetSignature);
        if (signatureLength == 0 || signatureLength > (TZrSize)0xFFFFFFFFu ||
            heapLength > (TZrSize)0xFFFFFFFFu - signatureLength) {
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
        heapLength += signatureLength;
    }

    recordCount =
            (moduleRecordCount + exportCount + typeDefCount + typeSpecCount + assemblyRefCount + typeRefCount +
             totalEffectCount) *
            2u;
    global = cs->state->global;
    records = (SZrMetadataTokenRecord *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrMetadataTokenRecord) * recordCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (records == ZR_NULL) {
        metadata_token_explicit_module_list_free(&explicitTypeRefModules);
        metadata_token_string_heap_free(cs, &stringHeapBuilder);
        return ZR_FALSE;
    }
    heap = (TZrByte *)ZrCore_Memory_RawMallocWithType(global, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (heap == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global,
                                      records,
                                      sizeof(SZrMetadataTokenRecord) * recordCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        metadata_token_explicit_module_list_free(&explicitTypeRefModules);
        metadata_token_string_heap_free(cs, &stringHeapBuilder);
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(records, 0, sizeof(SZrMetadataTokenRecord) * recordCount);
    ZrCore_Memory_RawSet(heap, 0, heapLength);
    signatureRidCursor = 1u;
    for (TZrUInt32 index = 0; index < exportCount; index++) {
        TZrSize signatureStart = heapOffset;
        TZrSize expectedLength = metadata_token_symbol_signature_size(cs, &function->typedExportedSymbols[index]);
        TZrMetadataToken memberToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, index + 1u);
        TZrMetadataToken signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, signatureRidCursor++);
        SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];
        TZrUInt64 signatureHash;

        metadata_token_write_symbol_signature(heap,
                                              &heapOffset,
                                              cs,
                                              symbol,
                                              stringHeapBuilder.entries,
                                              stringHeapBuilder.length);
        if (heapOffset - signatureStart != expectedLength) {
            ZrCore_Memory_RawFreeWithType(global,
                                          records,
                                          sizeof(SZrMetadataTokenRecord) * recordCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
        signatureHash = metadata_signature_hash_v1(heap + signatureStart, expectedLength);
        if (signatureHash == 0) {
            ZrCore_Memory_RawFreeWithType(global,
                                          records,
                                          sizeof(SZrMetadataTokenRecord) * recordCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }

        symbol->metadataToken = memberToken;
        symbol->signatureToken = signatureToken;
        symbol->signatureBlobOffset = (TZrUInt32)signatureStart;
        symbol->signatureBlobLength = (TZrUInt32)expectedLength;
        symbol->signatureHash = signatureHash;

        if (!metadata_token_write_record_pair(records,
                                              recordCount,
                                              &recordIndex,
                                              memberToken,
                                              signatureToken,
                                              0,
                                              index,
                                              (TZrUInt32)signatureStart,
                                              (TZrUInt32)expectedLength,
                                              signatureHash,
                                              0,
                                              0,
                                              0,
                                              0,
                                              ZR_NULL,
                                              ZR_NULL,
                                              ZR_NULL)) {
            ZrCore_Memory_RawFreeWithType(global,
                                          records,
                                          sizeof(SZrMetadataTokenRecord) * recordCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
    }
    if (moduleRecordCount > 0 &&
        !compiler_metadata_module_record_emit(cs,
                                              function,
                                              records,
                                              recordCount,
                                              &recordIndex,
                                              heap,
                                              heapLength,
                                              &heapOffset,
                                              &signatureRidCursor,
                                              stringHeapBuilder.entries,
                                              stringHeapBuilder.length)) {
        ZrCore_Memory_RawFreeWithType(global,
                                      records,
                                      sizeof(SZrMetadataTokenRecord) * recordCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        metadata_token_explicit_module_list_free(&explicitTypeRefModules);
        metadata_token_string_heap_free(cs, &stringHeapBuilder);
        return ZR_FALSE;
    }
    if (typeDefCount > 0 &&
        !compiler_metadata_type_def_emit(cs,
                                         function,
                                         records,
                                         recordCount,
                                         &recordIndex,
                                         heap,
                                         heapLength,
                                         &heapOffset,
                                         &signatureRidCursor,
                                         stringHeapBuilder.entries,
                                         stringHeapBuilder.length)) {
        ZrCore_Memory_RawFreeWithType(global,
                                      records,
                                      sizeof(SZrMetadataTokenRecord) * recordCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        metadata_token_explicit_module_list_free(&explicitTypeRefModules);
        metadata_token_string_heap_free(cs, &stringHeapBuilder);
        return ZR_FALSE;
    }
    if (typeSpecCount > 0 &&
        !compiler_metadata_type_spec_emit(cs,
                                          function,
                                          records,
                                          recordCount,
                                          &recordIndex,
                                          heap,
                                          heapLength,
                                          &heapOffset,
                                          &signatureRidCursor,
                                          stringHeapBuilder.entries,
                                          stringHeapBuilder.length)) {
        ZrCore_Memory_RawFreeWithType(global,
                                      records,
                                      sizeof(SZrMetadataTokenRecord) * recordCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        metadata_token_explicit_module_list_free(&explicitTypeRefModules);
        metadata_token_string_heap_free(cs, &stringHeapBuilder);
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0; index < totalEffectCount; index++) {
        const SZrFunctionModuleEffect *effect =
                metadata_token_get_import_member_ref_effect_by_flat_index(function, index);

        if (effect == ZR_NULL ||
            effect->moduleName == ZR_NULL ||
            metadata_token_effect_module_seen_before(function, index, effect->moduleName)) {
            continue;
        }
        {
            TZrSize signatureStart = heapOffset;
            TZrSize expectedLength = metadata_token_assembly_ref_signature_size(effect);
            TZrMetadataToken assemblyToken =
                    ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, assemblyRefRidCursor++);
            TZrMetadataToken signatureToken =
                    ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, signatureRidCursor++);
            TZrUInt64 signatureHash;

            metadata_token_write_assembly_ref_signature(heap,
                                                        &heapOffset,
                                                        effect,
                                                        stringHeapBuilder.entries,
                                                        stringHeapBuilder.length);
            if (heapOffset - signatureStart != expectedLength) {
                ZrCore_Memory_RawFreeWithType(global,
                                              records,
                                              sizeof(SZrMetadataTokenRecord) * recordCount,
                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                metadata_token_explicit_module_list_free(&explicitTypeRefModules);
                metadata_token_string_heap_free(cs, &stringHeapBuilder);
                return ZR_FALSE;
            }
            signatureHash = metadata_signature_hash_v1(heap + signatureStart, expectedLength);
            if (signatureHash == 0 ||
                !metadata_token_write_record_pair(records,
                                                  recordCount,
                                                  &recordIndex,
                                                  assemblyToken,
                                                  signatureToken,
                                                  0,
                                                  index,
                                                  (TZrUInt32)signatureStart,
                                                  (TZrUInt32)expectedLength,
                                                  signatureHash,
                                                  0,
                                                  0,
                                                  0,
                                                  effect != ZR_NULL ? effect->targetModuleSignatureHash : 0,
                                                  effect != ZR_NULL ? effect->requestedModuleVersion : ZR_NULL,
                                                  effect != ZR_NULL ? effect->minModuleVersionInclusive : ZR_NULL,
                                                  effect != ZR_NULL ? effect->maxModuleVersionExclusive : ZR_NULL)) {
                ZrCore_Memory_RawFreeWithType(global,
                                              records,
                                              sizeof(SZrMetadataTokenRecord) * recordCount,
                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                metadata_token_explicit_module_list_free(&explicitTypeRefModules);
                metadata_token_string_heap_free(cs, &stringHeapBuilder);
                return ZR_FALSE;
            }
        }
    }
    for (TZrUInt32 index = 0; index < explicitTypeRefModules.length; index++) {
        const SZrMetadataExplicitTypeRefModule *module = &explicitTypeRefModules.modules[index];
        TZrSize signatureStart = heapOffset;
        TZrSize expectedLength = metadata_token_assembly_ref_signature_size(ZR_NULL);
        TZrMetadataToken assemblyToken =
                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, assemblyRefRidCursor++);
        TZrMetadataToken signatureToken =
                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, signatureRidCursor++);
        TZrUInt64 signatureHash;

        metadata_token_write_explicit_assembly_ref_signature(heap,
                                                             &heapOffset,
                                                             module,
                                                             stringHeapBuilder.entries,
                                                             stringHeapBuilder.length);
        if (heapOffset - signatureStart != expectedLength) {
            ZrCore_Memory_RawFreeWithType(global,
                                          records,
                                          sizeof(SZrMetadataTokenRecord) * recordCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
        signatureHash = metadata_signature_hash_v1(heap + signatureStart, expectedLength);
        if (signatureHash == 0 ||
            !metadata_token_write_record_pair(records,
                                              recordCount,
                                              &recordIndex,
                                              assemblyToken,
                                              signatureToken,
                                              0,
                                              totalEffectCount + index,
                                              (TZrUInt32)signatureStart,
                                              (TZrUInt32)expectedLength,
                                              signatureHash,
                                              0,
                                              0,
                                              0,
                                              0,
                                              module->requestedModuleVersion,
                                              module->minModuleVersionInclusive,
                                              module->maxModuleVersionExclusive)) {
            ZrCore_Memory_RawFreeWithType(global,
                                          records,
                                          sizeof(SZrMetadataTokenRecord) * recordCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
    }
    for (TZrUInt32 index = 0; index < totalEffectCount; index++) {
        const SZrFunctionModuleEffect *effect =
                metadata_token_get_import_member_ref_effect_by_flat_index(function, index);

        if (effect == ZR_NULL ||
            effect->moduleName == ZR_NULL ||
            effect->symbolName == ZR_NULL ||
            metadata_token_effect_type_seen_before(function, index, effect->moduleName, effect->symbolName)) {
            continue;
        }
        {
            TZrSize signatureStart = heapOffset;
            TZrSize expectedLength = metadata_token_import_type_ref_signature_size(effect);
            TZrUInt32 assemblyRefRid =
                    metadata_token_get_import_module_ref_rid(function, totalEffectCount, effect->moduleName);
            TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, typeRefRidCursor++);
            TZrMetadataToken signatureToken =
                    ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, signatureRidCursor++);
            TZrMetadataToken assemblyToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, assemblyRefRid);
            TZrUInt64 signatureHash;

            if (assemblyRefRid == 0) {
                ZrCore_Memory_RawFreeWithType(global,
                                              records,
                                              sizeof(SZrMetadataTokenRecord) * recordCount,
                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                metadata_token_explicit_module_list_free(&explicitTypeRefModules);
                metadata_token_string_heap_free(cs, &stringHeapBuilder);
                return ZR_FALSE;
            }
            metadata_token_write_import_type_ref_signature(heap,
                                                           &heapOffset,
                                                           effect,
                                                           stringHeapBuilder.entries,
                                                           stringHeapBuilder.length);
            if (heapOffset - signatureStart != expectedLength) {
                ZrCore_Memory_RawFreeWithType(global,
                                              records,
                                              sizeof(SZrMetadataTokenRecord) * recordCount,
                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                metadata_token_explicit_module_list_free(&explicitTypeRefModules);
                metadata_token_string_heap_free(cs, &stringHeapBuilder);
                return ZR_FALSE;
            }
            signatureHash = metadata_signature_hash_v1(heap + signatureStart, expectedLength);
            if (signatureHash == 0 ||
                !metadata_token_write_record_pair(records,
                                                  recordCount,
                                                  &recordIndex,
                                                  typeToken,
                                                  signatureToken,
                                                  assemblyToken,
                                                  index,
                                                  (TZrUInt32)signatureStart,
                                                  (TZrUInt32)expectedLength,
                                                  signatureHash,
                                                  0,
                                                  0,
                                                  0,
                                                  0,
                                                  ZR_NULL,
                                                  ZR_NULL,
                                                  ZR_NULL)) {
                ZrCore_Memory_RawFreeWithType(global,
                                              records,
                                              sizeof(SZrMetadataTokenRecord) * recordCount,
                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                metadata_token_explicit_module_list_free(&explicitTypeRefModules);
                metadata_token_string_heap_free(cs, &stringHeapBuilder);
                return ZR_FALSE;
            }
        }
    }
    if (externalTypeRefPlan.typeRefCount > 0 &&
        !compiler_metadata_type_ref_emit(cs,
                                         function,
                                         totalEffectCount,
                                         metadata_token_get_import_member_ref_effect_by_flat_index,
                                         metadata_token_find_target_signature,
                                         metadata_token_resolve_assembly_ref_rid,
                                         &explicitTypeRefModules,
                                         records,
                                         recordCount,
                                         &recordIndex,
                                         heap,
                                         heapLength,
                                         &heapOffset,
                                         &signatureRidCursor,
                                         &typeRefRidCursor,
                                         stringHeapBuilder.entries,
                                         stringHeapBuilder.length)) {
        ZrCore_Memory_RawFreeWithType(global,
                                      records,
                                      sizeof(SZrMetadataTokenRecord) * recordCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        metadata_token_explicit_module_list_free(&explicitTypeRefModules);
        metadata_token_string_heap_free(cs, &stringHeapBuilder);
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0; index < totalEffectCount; index++) {
        const SZrFunctionModuleEffect *effect = metadata_token_get_import_member_ref_effect_by_flat_index(function, index);
        TZrSize signatureStart = heapOffset;
        SZrMetadataTokenTargetSignature targetSignature;
        TZrSize expectedLength;
        TZrSize targetSignatureStart = 0;
        TZrSize targetSignatureLength = 0;
        TZrUInt32 rid = index + 1u;
        TZrUInt32 typeRefRid = metadata_token_get_import_type_ref_rid(function,
                                                                      totalEffectCount,
                                                                      effect != ZR_NULL ? effect->moduleName : ZR_NULL,
                                                                      effect != ZR_NULL ? effect->symbolName : ZR_NULL);
        TZrMetadataToken memberToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, rid);
        TZrMetadataToken signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, signatureRidCursor++);
        TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, typeRefRid);
        TZrUInt64 signatureHash;
        TZrMetadataToken targetMetadataToken;
        TZrMetadataToken targetSignatureToken;
        TZrUInt64 targetSignatureHash;

        if (typeRefRid == 0) {
            ZrCore_Memory_RawFreeWithType(global,
                                          records,
                                          sizeof(SZrMetadataTokenRecord) * recordCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
        (void)metadata_token_find_target_signature(cs, effect, &targetSignature);
        expectedLength = metadata_token_member_ref_signature_size(cs, effect, &targetSignature);
        metadata_token_write_member_ref_signature(heap,
                                                  &heapOffset,
                                                  cs,
                                                  effect,
                                                  &targetSignature,
                                                  stringHeapBuilder.entries,
                                                  stringHeapBuilder.length,
                                                  &targetSignatureStart,
                                                  &targetSignatureLength);
        if (heapOffset - signatureStart != expectedLength) {
            ZrCore_Memory_RawFreeWithType(global,
                                          records,
                                          sizeof(SZrMetadataTokenRecord) * recordCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
        signatureHash = metadata_signature_hash_v1(heap + signatureStart, expectedLength);
        if (signatureHash == 0) {
            ZrCore_Memory_RawFreeWithType(global,
                                          records,
                                          sizeof(SZrMetadataTokenRecord) * recordCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
        targetSignatureHash = effect != ZR_NULL && effect->targetSignatureHash != 0
                                      ? effect->targetSignatureHash
                                      : targetSignature.signatureHash;
        targetMetadataToken = effect != ZR_NULL && effect->targetMetadataToken != 0
                                      ? effect->targetMetadataToken
                                      : targetSignature.metadataToken;
        targetSignatureToken = effect != ZR_NULL && effect->targetSignatureToken != 0
                                       ? effect->targetSignatureToken
                                       : targetSignature.signatureToken;
        if (targetSignatureHash == 0 && targetSignatureLength > 0) {
            targetSignatureHash = metadata_signature_hash_v1(heap + targetSignatureStart, targetSignatureLength);
        }

        if (!metadata_token_write_record_pair(records,
                                              recordCount,
                                              &recordIndex,
                                              memberToken,
                                              signatureToken,
                                              typeToken,
                                              index,
                                              (TZrUInt32)signatureStart,
                                              (TZrUInt32)expectedLength,
                                              signatureHash,
                                              targetMetadataToken,
                                              targetSignatureToken,
                                              targetSignatureHash,
                                              0,
                                              ZR_NULL,
                                              ZR_NULL,
                                              ZR_NULL)) {
            ZrCore_Memory_RawFreeWithType(global,
                                          records,
                                          sizeof(SZrMetadataTokenRecord) * recordCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            ZrCore_Memory_RawFreeWithType(global, heap, heapLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            metadata_token_explicit_module_list_free(&explicitTypeRefModules);
            metadata_token_string_heap_free(cs, &stringHeapBuilder);
            return ZR_FALSE;
        }
    }

    function->metadataTokenRecords = records;
    function->metadataTokenRecordLength = recordCount;
    function->signatureBlobHeap = heap;
    function->signatureBlobHeapLength = (TZrUInt32)heapLength;
    function->metadataStringHeap = stringHeapBuilder.entries;
    function->metadataStringHeapLength = stringHeapBuilder.length;
    metadata_token_explicit_module_list_free(&explicitTypeRefModules);
    stringHeapBuilder.entries = ZR_NULL;
    stringHeapBuilder.length = 0;
    stringHeapBuilder.capacity = 0;
    function->moduleSignatureHash = metadata_token_compute_module_signature_hash(cs,
                                                                                 function,
                                                                                 function->metadataStringHeap,
                                                                                 function->metadataStringHeapLength);
    if (exportCount > 0 && function->moduleSignatureHash == 0u) {
        metadata_token_clear_function(cs, function);
        return ZR_FALSE;
    }
    if (!compiler_build_module_metadata_ref_table(cs, function)) {
        metadata_token_clear_function(cs, function);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
