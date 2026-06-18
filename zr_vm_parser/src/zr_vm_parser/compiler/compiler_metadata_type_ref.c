#include "compiler_metadata_type_ref.h"
#include "compiler_metadata_signature.h"
#include "module_init_analysis.h"
#include "type_inference_internal.h"

#include <string.h>

typedef struct SZrMetadataExternalTypeRefEntry {
    SZrString *moduleName;
    SZrString *baseName;
    TZrMetadataToken ownerAssemblyToken;
    TZrMetadataToken targetMetadataToken;
    TZrMetadataToken targetSignatureToken;
    TZrUInt64 targetSignatureHash;
    TZrUInt64 targetModuleSignatureHash;
    TZrUInt32 layoutVersion;
    TZrUInt64 layoutHash;
    TZrSize signatureLength;
} SZrMetadataExternalTypeRefEntry;

static TZrSize metadata_type_ref_raw_signature_size(void) {
    return 1u + sizeof(TZrUInt32) + sizeof(TZrUInt32);
}

static void metadata_type_ref_write_raw_signature(TZrByte *buffer,
                                                  TZrSize *offset,
                                                  SZrString *baseName,
                                                  const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                  TZrUInt32 stringHeapEntryCount) {
    metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_TYPE_REF);
    metadata_token_write_u32(buffer, offset, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
    metadata_token_write_string_ref(buffer, offset, baseName, stringHeapEntries, stringHeapEntryCount);
}

static const SZrModuleInitTypeDefInfo *metadata_type_ref_find_summary_type_def(
        const SZrParserModuleInitSummary *summary,
        SZrString *baseName) {
    if (summary == ZR_NULL || baseName == ZR_NULL || !summary->typeDefs.isValid) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < summary->typeDefs.length; index++) {
        const SZrModuleInitTypeDefInfo *info =
                (const SZrModuleInitTypeDefInfo *)ZrCore_Array_Get((SZrArray *)&summary->typeDefs, index);
        if (info != ZR_NULL &&
            info->name != ZR_NULL &&
            ZrCore_String_Equal(info->name, baseName)) {
            return info;
        }
    }

    return ZR_NULL;
}

static TZrBool metadata_type_ref_entry_seen(const SZrMetadataExternalTypeRefEntry *entries,
                                            TZrUInt32 entryCount,
                                            const SZrModuleInitTypeDefInfo *typeDef,
                                            TZrUInt64 moduleSignatureHash) {
    if (entries == ZR_NULL || typeDef == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < entryCount; index++) {
        if (entries[index].targetMetadataToken == typeDef->metadataToken &&
            entries[index].targetSignatureToken == typeDef->signatureToken &&
            entries[index].targetSignatureHash == typeDef->signatureHash &&
            entries[index].targetModuleSignatureHash == moduleSignatureHash) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrUInt32 metadata_type_ref_add_capacity(TZrUInt32 left, TZrUInt32 right) {
    if (left >= ZR_METADATA_TOKEN_RID_MASK || right >= ZR_METADATA_TOKEN_RID_MASK ||
        right > ZR_METADATA_TOKEN_RID_MASK - left) {
        return ZR_METADATA_TOKEN_RID_MASK;
    }

    return left + right;
}

static TZrUInt32 metadata_type_ref_type_capacity(SZrCompilerState *cs, const SZrFunctionTypedTypeRef *typeRef) {
    TZrUInt32 capacity = 0u;

    if (typeRef == ZR_NULL) {
        return 0u;
    }

    if (typeRef->isArray) {
        SZrFunctionTypedTypeRef element;

        ZrCore_Memory_RawSet(&element, 0, sizeof(element));
        element.baseType = typeRef->elementBaseType;
        element.elementBaseType = ZR_VALUE_TYPE_OBJECT;
        element.typeName = typeRef->elementTypeName;
        return metadata_type_ref_type_capacity(cs, &element);
    }

    if (typeRef->typeName == ZR_NULL) {
        return 0u;
    }

    capacity = 1u;
    if (cs != ZR_NULL && cs->state != ZR_NULL) {
        SZrString *baseName = ZR_NULL;
        SZrArray argumentTypeNames;

        if (try_parse_generic_instance_type_name(cs->state, typeRef->typeName, &baseName, &argumentTypeNames)) {
            for (TZrSize index = 0; index < argumentTypeNames.length; index++) {
                SZrString **argumentNamePtr = (SZrString **)ZrCore_Array_Get(&argumentTypeNames, index);
                SZrFunctionTypedTypeRef argumentTypeRef;

                ZrCore_Memory_RawSet(&argumentTypeRef, 0, sizeof(argumentTypeRef));
                argumentTypeRef.baseType = ZR_VALUE_TYPE_OBJECT;
                argumentTypeRef.elementBaseType = ZR_VALUE_TYPE_OBJECT;
                argumentTypeRef.typeName = argumentNamePtr != ZR_NULL ? *argumentNamePtr : ZR_NULL;
                capacity = metadata_type_ref_add_capacity(capacity,
                                                          metadata_type_ref_type_capacity(cs, &argumentTypeRef));
            }
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
        }
    }

    return capacity;
}

static TZrNativeString metadata_type_ref_find_top_level_last_dot(SZrString *typeName) {
    TZrNativeString text;
    TZrNativeString lastDot = ZR_NULL;
    TZrUInt32 genericDepth = 0u;

    if (typeName == ZR_NULL) {
        return ZR_NULL;
    }

    text = ZrCore_String_GetNativeString(typeName);
    if (text == ZR_NULL || text[0] == '\0') {
        return ZR_NULL;
    }

    for (TZrNativeString cursor = text; *cursor != '\0'; cursor++) {
        if (*cursor == '<') {
            genericDepth++;
        } else if (*cursor == '>') {
            if (genericDepth > 0u) {
                genericDepth--;
            }
        } else if (*cursor == '.' && genericDepth == 0u) {
            lastDot = cursor;
        }
    }

    return lastDot;
}

TZrBool compiler_metadata_type_ref_split_module_qualified_type(SZrCompilerState *cs,
                                                               SZrString *typeName,
                                                               SZrString **outModuleName,
                                                               SZrString **outMemberTypeName) {
    TZrNativeString text;
    TZrNativeString lastDot;
    TZrSize textLength;
    TZrSize moduleLength;

    if (outModuleName != ZR_NULL) {
        *outModuleName = ZR_NULL;
    }
    if (outMemberTypeName != ZR_NULL) {
        *outMemberTypeName = ZR_NULL;
    }
    if (cs == ZR_NULL || cs->state == ZR_NULL || typeName == ZR_NULL ||
        outModuleName == ZR_NULL || outMemberTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    text = ZrCore_String_GetNativeString(typeName);
    textLength = ZrCore_String_GetByteLength(typeName);
    lastDot = metadata_type_ref_find_top_level_last_dot(typeName);
    if (text == ZR_NULL || lastDot == ZR_NULL || lastDot == text || *(lastDot + 1) == '\0') {
        return ZR_FALSE;
    }

    moduleLength = (TZrSize)(lastDot - text);
    if (moduleLength >= textLength) {
        return ZR_FALSE;
    }

    *outModuleName = ZrCore_String_Create(cs->state, text, moduleLength);
    *outMemberTypeName = ZrCore_String_Create(cs->state, lastDot + 1, textLength - moduleLength - 1u);
    return *outModuleName != ZR_NULL && *outMemberTypeName != ZR_NULL;
}

static SZrTypeBinding *metadata_type_ref_find_type_alias(SZrCompilerState *cs, SZrString *aliasName) {
    if (cs == ZR_NULL || aliasName == ZR_NULL || !cs->typeValueAliases.isValid) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < cs->typeValueAliases.length; index++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&cs->typeValueAliases, index);
        if (binding != ZR_NULL &&
            binding->name != ZR_NULL &&
            binding->type.typeName != ZR_NULL &&
            ZrCore_String_Equal(binding->name, aliasName) &&
            !ZrCore_String_Equal(binding->type.typeName, aliasName)) {
            return binding;
        }
    }

    return ZR_NULL;
}

static SZrString *metadata_type_ref_build_generic_alias_member_name(SZrCompilerState *cs,
                                                                    SZrString *resolvedBaseName,
                                                                    SZrArray *argumentTypeNames) {
    TZrNativeString baseText;
    TZrSize totalLength;
    TZrSize offset;
    TZrChar *buffer;
    SZrString *result;

    if (cs == ZR_NULL || cs->state == ZR_NULL || resolvedBaseName == ZR_NULL || argumentTypeNames == ZR_NULL) {
        return ZR_NULL;
    }

    baseText = ZrCore_String_GetNativeString(resolvedBaseName);
    if (baseText == ZR_NULL || baseText[0] == '\0') {
        return ZR_NULL;
    }

    totalLength = strlen(baseText) + 2u;
    for (TZrSize index = 0; index < argumentTypeNames->length; index++) {
        SZrString **argumentNamePtr = (SZrString **)ZrCore_Array_Get(argumentTypeNames, index);
        TZrNativeString argumentText =
                argumentNamePtr != ZR_NULL && *argumentNamePtr != ZR_NULL
                        ? ZrCore_String_GetNativeString(*argumentNamePtr)
                        : ZR_NULL;
        if (argumentText == ZR_NULL || argumentText[0] == '\0') {
            return ZR_NULL;
        }
        totalLength += strlen(argumentText);
        if (index + 1u < argumentTypeNames->length) {
            totalLength++;
        }
    }

    buffer = (TZrChar *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                       totalLength + 1u,
                                                       ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    offset = 0;
    memcpy(buffer + offset, baseText, strlen(baseText));
    offset += strlen(baseText);
    buffer[offset++] = '<';
    for (TZrSize index = 0; index < argumentTypeNames->length; index++) {
        SZrString **argumentNamePtr = (SZrString **)ZrCore_Array_Get(argumentTypeNames, index);
        TZrNativeString argumentText = ZrCore_String_GetNativeString(*argumentNamePtr);
        TZrSize argumentLength = strlen(argumentText);

        memcpy(buffer + offset, argumentText, argumentLength);
        offset += argumentLength;
        if (index + 1u < argumentTypeNames->length) {
            buffer[offset++] = ',';
        }
    }
    buffer[offset++] = '>';
    buffer[offset] = '\0';

    result = ZrCore_String_Create(cs->state, buffer, offset);
    ZrCore_Memory_RawFreeWithType(cs->state->global,
                                  buffer,
                                  totalLength + 1u,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return result;
}

TZrBool compiler_metadata_type_ref_resolve_unqualified_alias(SZrCompilerState *cs,
                                                             SZrString *typeName,
                                                             SZrString **outModuleName,
                                                             SZrString **outMemberTypeName) {
    SZrString *baseName = ZR_NULL;
    SZrString *aliasModuleName = ZR_NULL;
    SZrString *aliasMemberBaseName = ZR_NULL;
    SZrArray argumentTypeNames;
    SZrTypeBinding *binding;
    TZrBool parsedGeneric = ZR_FALSE;
    TZrBool result = ZR_FALSE;

    if (outModuleName != ZR_NULL) {
        *outModuleName = ZR_NULL;
    }
    if (outMemberTypeName != ZR_NULL) {
        *outMemberTypeName = ZR_NULL;
    }
    if (cs == ZR_NULL || cs->state == ZR_NULL || typeName == ZR_NULL ||
        outModuleName == ZR_NULL || outMemberTypeName == ZR_NULL) {
        return ZR_FALSE;
    }
    if (metadata_type_ref_find_top_level_last_dot(typeName) != ZR_NULL) {
        return ZR_FALSE;
    }

    if (try_parse_generic_instance_type_name(cs->state, typeName, &baseName, &argumentTypeNames)) {
        parsedGeneric = ZR_TRUE;
    } else {
        baseName = typeName;
    }

    binding = metadata_type_ref_find_type_alias(cs, baseName);
    if (binding != ZR_NULL &&
        compiler_metadata_type_ref_split_module_qualified_type(cs,
                                                               binding->type.typeName,
                                                               &aliasModuleName,
                                                               &aliasMemberBaseName)) {
        if (parsedGeneric) {
            SZrString *aliasMemberName =
                    metadata_type_ref_build_generic_alias_member_name(cs, aliasMemberBaseName, &argumentTypeNames);
            if (aliasMemberName != ZR_NULL) {
                *outModuleName = aliasModuleName;
                *outMemberTypeName = aliasMemberName;
                result = ZR_TRUE;
            }
        } else {
            *outModuleName = aliasModuleName;
            *outMemberTypeName = aliasMemberBaseName;
            result = ZR_TRUE;
        }
    }

    if (parsedGeneric) {
        ZrCore_Array_Free(cs->state, &argumentTypeNames);
    }
    return result;
}

static TZrBool metadata_type_ref_append_for_module_type(SZrCompilerState *cs,
                                                        SZrString *moduleName,
                                                        const SZrFunctionTypedTypeRef *typeRef,
                                                        SZrMetadataExternalTypeRefEntry *entries,
                                                        TZrUInt32 entryCapacity,
                                                        TZrUInt32 *ioEntryCount) {
    const SZrParserModuleInitSummary *summary;
    const SZrModuleInitTypeDefInfo *typeDef;
    SZrString *activeModuleName;
    SZrString *activeTypeName;
    SZrString *qualifiedModuleName = ZR_NULL;
    SZrString *qualifiedMemberTypeName = ZR_NULL;
    SZrString *baseName = ZR_NULL;
    SZrArray argumentTypeNames;
    TZrBool hasGenericArguments = ZR_FALSE;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL ||
        entries == ZR_NULL || ioEntryCount == ZR_NULL) {
        return ZR_FALSE;
    }
    if (typeRef == ZR_NULL) {
        return ZR_TRUE;
    }

    if (typeRef->isArray) {
        SZrFunctionTypedTypeRef element;

        ZrCore_Memory_RawSet(&element, 0, sizeof(element));
        element.baseType = typeRef->elementBaseType;
        element.elementBaseType = ZR_VALUE_TYPE_OBJECT;
        element.typeName = typeRef->elementTypeName;
        return metadata_type_ref_append_for_module_type(cs,
                                                        moduleName,
                                                        &element,
                                                        entries,
                                                        entryCapacity,
                                                        ioEntryCount);
    }

    if (typeRef->typeName == ZR_NULL) {
        return ZR_TRUE;
    }

    activeModuleName = moduleName;
    activeTypeName = typeRef->typeName;
    if (compiler_metadata_type_ref_split_module_qualified_type(cs,
                                                               typeRef->typeName,
                                                               &qualifiedModuleName,
                                                               &qualifiedMemberTypeName)) {
        activeModuleName = qualifiedModuleName;
        activeTypeName = qualifiedMemberTypeName;
    } else if (moduleName == ZR_NULL &&
               compiler_metadata_type_ref_resolve_unqualified_alias(cs,
                                                                    typeRef->typeName,
                                                                    &qualifiedModuleName,
                                                                    &qualifiedMemberTypeName)) {
        activeModuleName = qualifiedModuleName;
        activeTypeName = qualifiedMemberTypeName;
    }
    if (try_parse_generic_instance_type_name(cs->state, activeTypeName, &baseName, &argumentTypeNames)) {
        hasGenericArguments = ZR_TRUE;
    } else {
        baseName = activeTypeName;
    }

    if (activeModuleName == ZR_NULL) {
        if (hasGenericArguments) {
            for (TZrSize index = 0; index < argumentTypeNames.length; index++) {
                SZrString **argumentNamePtr = (SZrString **)ZrCore_Array_Get(&argumentTypeNames, index);
                SZrFunctionTypedTypeRef argumentTypeRef;

                ZrCore_Memory_RawSet(&argumentTypeRef, 0, sizeof(argumentTypeRef));
                argumentTypeRef.baseType = ZR_VALUE_TYPE_OBJECT;
                argumentTypeRef.elementBaseType = ZR_VALUE_TYPE_OBJECT;
                argumentTypeRef.typeName = argumentNamePtr != ZR_NULL ? *argumentNamePtr : ZR_NULL;
                if (!metadata_type_ref_append_for_module_type(cs,
                                                              ZR_NULL,
                                                              &argumentTypeRef,
                                                              entries,
                                                              entryCapacity,
                                                              ioEntryCount)) {
                    ZrCore_Array_Free(cs->state, &argumentTypeNames);
                    return ZR_FALSE;
                }
            }
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
        }
        return ZR_TRUE;
    }

    summary = ZrParser_ModuleInitAnalysis_FindSummary(cs->state->global, activeModuleName);
    if (summary == ZR_NULL) {
        if (!ZrParser_ModuleInitAnalysis_EnsureSummary(cs, activeModuleName)) {
            if (hasGenericArguments) {
                ZrCore_Array_Free(cs->state, &argumentTypeNames);
            }
            return ZR_TRUE;
        }
        summary = ZrParser_ModuleInitAnalysis_FindSummary(cs->state->global, activeModuleName);
    }
    if (summary == ZR_NULL || summary->state == ZR_PARSER_MODULE_INIT_SUMMARY_FAILED) {
        if (hasGenericArguments) {
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
        }
        return ZR_TRUE;
    }

    typeDef = metadata_type_ref_find_summary_type_def(summary, baseName);
    if (typeDef != ZR_NULL &&
        typeDef->metadataToken != 0u &&
        typeDef->signatureToken != 0u &&
        typeDef->signatureHash != 0u &&
        !metadata_type_ref_entry_seen(entries, *ioEntryCount, typeDef, summary->moduleSignatureHash)) {
        if (*ioEntryCount >= entryCapacity) {
            if (hasGenericArguments) {
                ZrCore_Array_Free(cs->state, &argumentTypeNames);
            }
            return ZR_FALSE;
        }

        entries[*ioEntryCount].moduleName = activeModuleName;
        entries[*ioEntryCount].baseName = baseName;
        entries[*ioEntryCount].targetMetadataToken = typeDef->metadataToken;
        entries[*ioEntryCount].targetSignatureToken = typeDef->signatureToken;
        entries[*ioEntryCount].targetSignatureHash = typeDef->signatureHash;
        entries[*ioEntryCount].targetModuleSignatureHash = summary->moduleSignatureHash;
        entries[*ioEntryCount].layoutVersion = typeDef->layoutVersion;
        entries[*ioEntryCount].layoutHash = typeDef->layoutHash;
        entries[*ioEntryCount].signatureLength = metadata_type_ref_raw_signature_size();

        (*ioEntryCount)++;
    }

    if (hasGenericArguments) {
        for (TZrSize index = 0; index < argumentTypeNames.length; index++) {
            SZrString **argumentNamePtr = (SZrString **)ZrCore_Array_Get(&argumentTypeNames, index);
            SZrFunctionTypedTypeRef argumentTypeRef;

            ZrCore_Memory_RawSet(&argumentTypeRef, 0, sizeof(argumentTypeRef));
                argumentTypeRef.baseType = ZR_VALUE_TYPE_OBJECT;
                argumentTypeRef.elementBaseType = ZR_VALUE_TYPE_OBJECT;
                argumentTypeRef.typeName = argumentNamePtr != ZR_NULL ? *argumentNamePtr : ZR_NULL;
            if (!metadata_type_ref_append_for_module_type(cs,
                                                          activeModuleName,
                                                          &argumentTypeRef,
                                                          entries,
                                                          entryCapacity,
                                                          ioEntryCount)) {
                ZrCore_Array_Free(cs->state, &argumentTypeNames);
                return ZR_FALSE;
            }
        }
        ZrCore_Array_Free(cs->state, &argumentTypeNames);
    }

    return ZR_TRUE;
}

static TZrBool metadata_type_ref_append_for_type(SZrCompilerState *cs,
                                                 const SZrFunctionModuleEffect *effect,
                                                 const SZrFunctionTypedTypeRef *typeRef,
                                                 SZrMetadataExternalTypeRefEntry *entries,
                                                 TZrUInt32 entryCapacity,
                                                 TZrUInt32 *ioEntryCount) {
    if (effect == ZR_NULL) {
        return ZR_TRUE;
    }

    return metadata_type_ref_append_for_module_type(cs,
                                                    effect->moduleName,
                                                    typeRef,
                                                    entries,
                                                    entryCapacity,
                                                    ioEntryCount);
}

static TZrUInt32 metadata_type_ref_target_type_capacity(SZrCompilerState *cs,
                                                        const SZrFunction *function,
                                                        TZrUInt32 totalEffectCount,
                                                        TZrMetadataTypeRefEffectByFlatIndex effectAt,
                                                        TZrMetadataTypeRefTargetSignatureResolver resolveTarget) {
    TZrUInt32 capacity = 0u;

    if (cs == ZR_NULL || function == ZR_NULL || effectAt == ZR_NULL || resolveTarget == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0; index < totalEffectCount; index++) {
        const SZrFunctionModuleEffect *effect = effectAt(function, index);
        SZrMetadataTokenTargetSignature targetSignature;

        if (effect == ZR_NULL || !resolveTarget(cs, effect, &targetSignature) || !targetSignature.hasSignature) {
            continue;
        }
        capacity = metadata_type_ref_add_capacity(
                capacity,
                metadata_type_ref_type_capacity(cs, &targetSignature.valueType));
        for (TZrUInt32 paramIndex = 0; paramIndex < targetSignature.parameterCount; paramIndex++) {
            capacity = metadata_type_ref_add_capacity(
                    capacity,
                    metadata_type_ref_type_capacity(cs,
                                                    targetSignature.parameterTypes != ZR_NULL
                                                            ? &targetSignature.parameterTypes[paramIndex]
                                                            : ZR_NULL));
        }
    }

    return capacity;
}

static TZrUInt32 metadata_type_ref_explicit_type_capacity(SZrCompilerState *cs,
                                                          const SZrFunction *function) {
    TZrUInt32 capacity = 0u;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0; index < function->typedLocalBindingLength; index++) {
        capacity = metadata_type_ref_add_capacity(
                capacity,
                metadata_type_ref_type_capacity(cs, &function->typedLocalBindings[index].type));
    }

    return capacity;
}

static TZrUInt32 metadata_type_ref_get_assembly_ref_rid(const SZrFunction *function,
                                                        TZrUInt32 totalEffectCount,
                                                        TZrMetadataTypeRefEffectByFlatIndex effectAt,
                                                        SZrString *moduleName) {
    TZrUInt32 rid = 1u;

    if (function == ZR_NULL || effectAt == ZR_NULL || moduleName == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 index = 0; index < totalEffectCount; index++) {
        const SZrFunctionModuleEffect *effect = effectAt(function, index);
        TZrBool seenBefore = ZR_FALSE;

        if (effect == ZR_NULL || effect->moduleName == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 prior = 0; prior < index; prior++) {
            const SZrFunctionModuleEffect *priorEffect = effectAt(function, prior);
            if (priorEffect != ZR_NULL &&
                priorEffect->moduleName != ZR_NULL &&
                ZrCore_String_Equal(priorEffect->moduleName, effect->moduleName)) {
                seenBefore = ZR_TRUE;
                break;
            }
        }
        if (seenBefore) {
            continue;
        }
        if (ZrCore_String_Equal(effect->moduleName, moduleName)) {
            return rid;
        }
        rid++;
    }

    return 0u;
}

static TZrBool metadata_type_ref_collect_entries(SZrCompilerState *cs,
                                                 const SZrFunction *function,
                                                 TZrUInt32 totalEffectCount,
                                                 TZrMetadataTypeRefEffectByFlatIndex effectAt,
                                                 TZrMetadataTypeRefTargetSignatureResolver resolveTarget,
                                                 SZrMetadataExternalTypeRefEntry *entries,
                                                 TZrUInt32 entryCapacity,
                                                 TZrUInt32 *outEntryCount) {
    TZrUInt32 entryCount = 0u;

    if (outEntryCount != ZR_NULL) {
        *outEntryCount = 0u;
    }
    if (cs == ZR_NULL || function == ZR_NULL || effectAt == ZR_NULL || resolveTarget == ZR_NULL ||
        entries == ZR_NULL || outEntryCount == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < totalEffectCount; index++) {
        const SZrFunctionModuleEffect *effect = effectAt(function, index);
        SZrMetadataTokenTargetSignature targetSignature;

        if (effect == ZR_NULL || !resolveTarget(cs, effect, &targetSignature) || !targetSignature.hasSignature) {
            continue;
        }

        if (!metadata_type_ref_append_for_type(cs,
                                               effect,
                                               &targetSignature.valueType,
                                               entries,
                                               entryCapacity,
                                               &entryCount)) {
            return ZR_FALSE;
        }
        for (TZrUInt32 paramIndex = 0; paramIndex < targetSignature.parameterCount; paramIndex++) {
            if (!metadata_type_ref_append_for_type(
                        cs,
                        effect,
                        targetSignature.parameterTypes != ZR_NULL ? &targetSignature.parameterTypes[paramIndex] : ZR_NULL,
                        entries,
                        entryCapacity,
                        &entryCount)) {
                return ZR_FALSE;
            }
        }
    }

    if (function->typedLocalBindings != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->typedLocalBindingLength; index++) {
            if (!metadata_type_ref_append_for_module_type(cs,
                                                          ZR_NULL,
                                                          &function->typedLocalBindings[index].type,
                                                          entries,
                                                          entryCapacity,
                                                          &entryCount)) {
                return ZR_FALSE;
            }
        }
    }

    *outEntryCount = entryCount;
    return ZR_TRUE;
}

TZrBool compiler_metadata_type_ref_plan(SZrCompilerState *cs,
                                        const SZrFunction *function,
                                        TZrUInt32 totalEffectCount,
                                        TZrMetadataTypeRefEffectByFlatIndex effectAt,
                                        TZrMetadataTypeRefTargetSignatureResolver resolveTarget,
                                        const SZrMetadataStringHeapEntry *stringHeapEntries,
                                        TZrUInt32 stringHeapEntryCount,
                                        SZrMetadataExternalTypeRefPlan *outPlan) {
    SZrMetadataExternalTypeRefEntry *entries;
    TZrUInt32 entryCount = 0u;

    ZR_UNUSED_PARAMETER(stringHeapEntries);
    ZR_UNUSED_PARAMETER(stringHeapEntryCount);

    if (outPlan != ZR_NULL) {
        ZrCore_Memory_RawSet(outPlan, 0, sizeof(*outPlan));
    }
    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL ||
        outPlan == ZR_NULL) {
        return ZR_FALSE;
    }

    {
        TZrUInt32 entryCapacity = metadata_type_ref_add_capacity(
                metadata_type_ref_target_type_capacity(cs,
                                                       function,
                                                       totalEffectCount,
                                                       effectAt,
                                                       resolveTarget),
                metadata_type_ref_explicit_type_capacity(cs, function));
        if (entryCapacity == 0u) {
            return ZR_TRUE;
        }

        entries = (SZrMetadataExternalTypeRefEntry *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrMetadataExternalTypeRefEntry) * entryCapacity,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (entries == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(entries, 0, sizeof(SZrMetadataExternalTypeRefEntry) * entryCapacity);

        if (!metadata_type_ref_collect_entries(cs,
                                               function,
                                               totalEffectCount,
                                               effectAt,
                                               resolveTarget,
                                               entries,
                                               entryCapacity,
                                               &entryCount)) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          entries,
                                          sizeof(SZrMetadataExternalTypeRefEntry) * entryCapacity,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }

        for (TZrUInt32 index = 0; index < entryCount; index++) {
            if (entries[index].signatureLength > (TZrSize)0xFFFFFFFFu ||
                outPlan->signatureHeapLength > (TZrSize)0xFFFFFFFFu - entries[index].signatureLength) {
                ZrCore_Memory_RawFreeWithType(cs->state->global,
                                              entries,
                                              sizeof(SZrMetadataExternalTypeRefEntry) * entryCapacity,
                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                return ZR_FALSE;
            }
            outPlan->signatureHeapLength += entries[index].signatureLength;
        }
        outPlan->typeRefCount = entryCount;

        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      entries,
                                      sizeof(SZrMetadataExternalTypeRefEntry) * entryCapacity,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    return ZR_TRUE;
}

static TZrBool metadata_type_ref_write_record_pair(SZrMetadataTokenRecord *records,
                                                   TZrUInt32 recordCount,
                                                   TZrUInt32 *ioRecordIndex,
                                                   const SZrMetadataExternalTypeRefEntry *entry,
                                                   TZrMetadataToken token,
                                                   TZrMetadataToken signatureToken,
                                                   TZrUInt32 signatureBlobOffset,
                                                   TZrUInt32 signatureBlobLength,
                                                   TZrUInt64 signatureHash) {
    TZrUInt32 recordIndex;

    if (records == ZR_NULL || ioRecordIndex == ZR_NULL || entry == ZR_NULL ||
        *ioRecordIndex + 1u >= recordCount) {
        return ZR_FALSE;
    }

    recordIndex = *ioRecordIndex;
    records[recordIndex].token = token;
    records[recordIndex].relatedToken = signatureToken;
    records[recordIndex].ownerToken = entry->ownerAssemblyToken;
    records[recordIndex].signatureBlobOffset = signatureBlobOffset;
    records[recordIndex].signatureBlobLength = signatureBlobLength;
    records[recordIndex].signatureHash = signatureHash;
    records[recordIndex].layoutVersion = entry->layoutVersion;
    records[recordIndex].layoutHash = entry->layoutHash;
    records[recordIndex].targetMetadataToken = entry->targetMetadataToken;
    records[recordIndex].targetSignatureToken = entry->targetSignatureToken;
    records[recordIndex].targetSignatureHash = entry->targetSignatureHash;
    records[recordIndex].targetModuleSignatureHash = entry->targetModuleSignatureHash;
    recordIndex++;

    records[recordIndex] = records[recordIndex - 1u];
    records[recordIndex].token = signatureToken;
    records[recordIndex].relatedToken = token;
    records[recordIndex].ownerToken = token;
    recordIndex++;

    *ioRecordIndex = recordIndex;
    return ZR_TRUE;
}

TZrBool compiler_metadata_type_ref_emit(SZrCompilerState *cs,
                                        const SZrFunction *function,
                                        TZrUInt32 totalEffectCount,
                                        TZrMetadataTypeRefEffectByFlatIndex effectAt,
                                        TZrMetadataTypeRefTargetSignatureResolver resolveTarget,
                                        TZrMetadataTypeRefAssemblyRefRidResolver resolveAssemblyRefRid,
                                        void *resolveAssemblyRefRidUserData,
                                        SZrMetadataTokenRecord *records,
                                        TZrUInt32 recordCount,
                                        TZrUInt32 *ioRecordIndex,
                                        TZrByte *heap,
                                        TZrSize heapLength,
                                        TZrSize *ioHeapOffset,
                                        TZrUInt32 *ioSignatureRidCursor,
                                        TZrUInt32 *ioTypeRefRidCursor,
                                        const SZrMetadataStringHeapEntry *stringHeapEntries,
                                        TZrUInt32 stringHeapEntryCount) {
    SZrMetadataExternalTypeRefEntry *entries;
    TZrUInt32 entryCount = 0u;
    TZrUInt32 entryCapacity;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL ||
        heap == ZR_NULL || ioHeapOffset == ZR_NULL || ioSignatureRidCursor == ZR_NULL ||
        ioTypeRefRidCursor == ZR_NULL) {
        return ZR_FALSE;
    }

    {
        entryCapacity = metadata_type_ref_add_capacity(metadata_type_ref_target_type_capacity(cs,
                                                                                              function,
                                                                                              totalEffectCount,
                                                                                              effectAt,
                                                                                              resolveTarget),
                                                       metadata_type_ref_explicit_type_capacity(cs, function));
        if (entryCapacity == 0u) {
            return ZR_TRUE;
        }

        entries = (SZrMetadataExternalTypeRefEntry *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrMetadataExternalTypeRefEntry) * entryCapacity,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (entries == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(entries, 0, sizeof(SZrMetadataExternalTypeRefEntry) * entryCapacity);

        if (!metadata_type_ref_collect_entries(cs,
                                               function,
                                               totalEffectCount,
                                               effectAt,
                                               resolveTarget,
                                               entries,
                                               entryCapacity,
                                               &entryCount)) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          entries,
                                          sizeof(SZrMetadataExternalTypeRefEntry) * entryCapacity,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 index = 0; index < entryCount; index++) {
        TZrSize signatureStart = *ioHeapOffset;
        TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, (*ioTypeRefRidCursor)++);
        TZrMetadataToken signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, (*ioSignatureRidCursor)++);
        TZrUInt64 signatureHash;
        TZrUInt32 assemblyRid;

        if (signatureStart > heapLength ||
            entries[index].signatureLength > heapLength - signatureStart ||
            signatureStart > (TZrSize)0xFFFFFFFFu) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          entries,
                                          sizeof(SZrMetadataExternalTypeRefEntry) * entryCapacity,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }

        assemblyRid = resolveAssemblyRefRid != ZR_NULL
                              ? resolveAssemblyRefRid(cs,
                                                      function,
                                                      totalEffectCount,
                                                      effectAt,
                                                      entries[index].moduleName,
                                                      resolveAssemblyRefRidUserData)
                              : metadata_type_ref_get_assembly_ref_rid(function,
                                                                       totalEffectCount,
                                                                       effectAt,
                                                                       entries[index].moduleName);
        if (assemblyRid == 0u) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          entries,
                                          sizeof(SZrMetadataExternalTypeRefEntry) * entryCapacity,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }
        entries[index].ownerAssemblyToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, assemblyRid);
        metadata_type_ref_write_raw_signature(heap,
                                              ioHeapOffset,
                                              entries[index].baseName,
                                              stringHeapEntries,
                                              stringHeapEntryCount);
        if (*ioHeapOffset - signatureStart != entries[index].signatureLength) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          entries,
                                          sizeof(SZrMetadataExternalTypeRefEntry) * entryCapacity,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }

        signatureHash = metadata_signature_hash_v1(heap + signatureStart, entries[index].signatureLength);
        if (signatureHash == 0u ||
            !metadata_type_ref_write_record_pair(records,
                                                 recordCount,
                                                 ioRecordIndex,
                                                 &entries[index],
                                                 typeToken,
                                                 signatureToken,
                                                 (TZrUInt32)signatureStart,
                                                 (TZrUInt32)entries[index].signatureLength,
                                                 signatureHash)) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          entries,
                                          sizeof(SZrMetadataExternalTypeRefEntry) * entryCapacity,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }
    }

    ZrCore_Memory_RawFreeWithType(cs->state->global,
                                  entries,
                                  sizeof(SZrMetadataExternalTypeRefEntry) * entryCapacity,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return ZR_TRUE;
}
