#include "compiler_metadata_signature.h"
#include "type_inference_internal.h"

#include "zr_vm_core/hash.h"

#include <string.h>

static const TZrByte CZrMetadataSignatureHashV1Prefix[] = {
        'z',
        'r',
        '.',
        'm',
        'd',
        '.',
        's',
        'i',
        'g',
        '.',
        'v',
        '1',
        '\0',
};

TZrUInt64 metadata_signature_hash_v1(const TZrByte *signatureBlob, TZrSize signatureBlobLength) {
    if (signatureBlob == ZR_NULL || signatureBlobLength == 0) {
        return 0;
    }

    return ZrCore_Hash_CreateStable64WithPrefix(CZrMetadataSignatureHashV1Prefix,
                                                sizeof(CZrMetadataSignatureHashV1Prefix),
                                                signatureBlob,
                                                signatureBlobLength);
}

static TZrNativeString metadata_token_string_text(struct SZrString *stringValue) {
    return stringValue != ZR_NULL ? ZrCore_String_GetNativeString(stringValue) : ZR_NULL;
}

TZrSize metadata_token_string_length(SZrString *stringValue) {
    TZrNativeString text = metadata_token_string_text(stringValue);
    return text != ZR_NULL ? strlen(text) : 0;
}

static TZrUInt32 metadata_token_string_stable_index(SZrString *value) {
    TZrNativeString text = metadata_token_string_text(value);
    TZrSize length = text != ZR_NULL ? strlen(text) : 0;
    TZrUInt64 hash;

    if (length == 0) {
        return 0;
    }

    hash = ZrCore_Hash_CreateStable64((const TZrByte *)text, length);
    return (TZrUInt32)(hash & 0x7FFFFFFFu) + 1u;
}

TZrUInt32 metadata_token_string_heap_index(const SZrMetadataStringHeapEntry *entries,
                                           TZrUInt32 entryCount,
                                           SZrString *value) {
    TZrUInt32 expectedIndex;

    if (entries == ZR_NULL || entryCount == 0 || metadata_token_string_length(value) == 0) {
        return 0;
    }

    expectedIndex = metadata_token_string_stable_index(value);
    for (TZrUInt32 index = 0; index < entryCount; index++) {
        if (entries[index].stringIndex == expectedIndex &&
            ((entries[index].value == value) ||
             (entries[index].value != ZR_NULL &&
              value != ZR_NULL &&
              ZrCore_String_Equal(entries[index].value, value)))) {
            return expectedIndex;
        }
    }

    return 0;
}

static void metadata_token_type_ref_from_name(SZrString *typeName, SZrFunctionTypedTypeRef *outTypeRef) {
    TZrNativeString typeNameText;
    TZrSize typeNameLength;
    EZrValueType primitiveType;

    if (outTypeRef == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(outTypeRef, 0, sizeof(*outTypeRef));
    outTypeRef->baseType = ZR_VALUE_TYPE_OBJECT;
    outTypeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    if (typeName == ZR_NULL) {
        return;
    }

    typeNameText = metadata_token_string_text(typeName);
    typeNameLength = typeNameText != ZR_NULL ? strlen(typeNameText) : 0;
    if (inferred_type_try_map_primitive_name(typeNameText, typeNameLength, &primitiveType)) {
        outTypeRef->baseType = primitiveType;
        return;
    }

    outTypeRef->typeName = typeName;
}

void metadata_token_write_u8(TZrByte *buffer, TZrSize *offset, TZrUInt8 value) {
    buffer[*offset] = value;
    *offset += 1;
}

void metadata_token_write_u32(TZrByte *buffer, TZrSize *offset, TZrUInt32 value) {
    buffer[*offset + 0] = (TZrByte)(value & 0xFFu);
    buffer[*offset + 1] = (TZrByte)((value >> 8) & 0xFFu);
    buffer[*offset + 2] = (TZrByte)((value >> 16) & 0xFFu);
    buffer[*offset + 3] = (TZrByte)((value >> 24) & 0xFFu);
    *offset += 4;
}

static SZrAstNode *metadata_token_find_union_declaration_in_array(SZrAstNodeArray *declarations,
                                                                  SZrString *typeName) {
    if (declarations == ZR_NULL || declarations->nodes == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < declarations->count; index++) {
        SZrAstNode *declaration = declarations->nodes[index];

        if (declaration == ZR_NULL) {
            continue;
        }
        if (declaration->type == ZR_AST_UNION_DECLARATION &&
            declaration->data.unionDeclaration.name != ZR_NULL &&
            declaration->data.unionDeclaration.name->name != ZR_NULL &&
            ZrCore_String_Equal(declaration->data.unionDeclaration.name->name, typeName)) {
            return declaration;
        }
        if (declaration->type == ZR_AST_EXTERN_BLOCK) {
            SZrAstNode *match = metadata_token_find_union_declaration_in_array(
                    declaration->data.externBlock.declarations,
                    typeName);
            if (match != ZR_NULL) {
                return match;
            }
        }
    }

    return ZR_NULL;
}

TZrBool metadata_token_try_resolve_union_signature_type(SZrCompilerState *cs,
                                                        SZrString *typeName,
                                                        SZrString **outBaseName,
                                                        SZrArray *outArgumentTypeNames) {
    SZrArray argumentTypeNames;
    SZrString *baseName = ZR_NULL;
    SZrString *lookupName;
    TZrBool parsedGeneric;

    if (outBaseName != ZR_NULL) {
        *outBaseName = ZR_NULL;
    }
    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->scriptAst == ZR_NULL ||
        cs->scriptAst->type != ZR_AST_SCRIPT || typeName == ZR_NULL ||
        outBaseName == ZR_NULL || outArgumentTypeNames == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&argumentTypeNames);
    parsedGeneric = try_parse_generic_instance_type_name(cs->state, typeName, &baseName, &argumentTypeNames);
    lookupName = parsedGeneric && baseName != ZR_NULL ? baseName : typeName;
    if (metadata_token_find_union_declaration_in_array(cs->scriptAst->data.script.statements, lookupName) == ZR_NULL) {
        ZrCore_Array_Free(cs->state, &argumentTypeNames);
        return ZR_FALSE;
    }

    *outBaseName = lookupName;
    *outArgumentTypeNames = argumentTypeNames;
    return ZR_TRUE;
}

static TZrSize metadata_token_generic_argument_signatures_size(SZrCompilerState *cs, SZrArray *argumentTypeNames) {
    TZrSize size = 0;

    if (argumentTypeNames == ZR_NULL) {
        return 0;
    }

    for (TZrSize index = 0; index < argumentTypeNames->length; index++) {
        SZrString **argumentTypeNamePtr = (SZrString **)ZrCore_Array_Get(argumentTypeNames, index);
        SZrFunctionTypedTypeRef argumentTypeRef;

        metadata_token_type_ref_from_name(argumentTypeNamePtr != ZR_NULL ? *argumentTypeNamePtr : ZR_NULL,
                                          &argumentTypeRef);
        size += metadata_token_type_ref_signature_size(cs, &argumentTypeRef);
    }

    return size;
}

static void metadata_token_write_generic_argument_signatures(TZrByte *buffer,
                                                            TZrSize *offset,
                                                            SZrCompilerState *cs,
                                                            SZrArray *argumentTypeNames,
                                                            const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                            TZrUInt32 stringHeapEntryCount) {
    if (argumentTypeNames == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < argumentTypeNames->length; index++) {
        SZrString **argumentTypeNamePtr = (SZrString **)ZrCore_Array_Get(argumentTypeNames, index);
        SZrFunctionTypedTypeRef argumentTypeRef;

        metadata_token_type_ref_from_name(argumentTypeNamePtr != ZR_NULL ? *argumentTypeNamePtr : ZR_NULL,
                                          &argumentTypeRef);
        metadata_token_write_type_ref_signature(buffer,
                                                offset,
                                                cs,
                                                &argumentTypeRef,
                                                stringHeapEntries,
                                                stringHeapEntryCount);
    }
}

TZrSize metadata_token_type_ref_signature_size(SZrCompilerState *cs, const SZrFunctionTypedTypeRef *typeRef) {
    TZrSize typeNameLength;

    if (typeRef == ZR_NULL) {
        return 1 + sizeof(TZrUInt32);
    }

    if (typeRef->isNullable) {
        SZrFunctionTypedTypeRef nested = *typeRef;
        nested.isNullable = ZR_FALSE;
        return 1 + metadata_token_type_ref_signature_size(cs, &nested);
    }

    if (typeRef->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE) {
        SZrFunctionTypedTypeRef nested = *typeRef;
        nested.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
        return 1 + sizeof(TZrUInt32) + metadata_token_type_ref_signature_size(cs, &nested);
    }

    if (typeRef->isArray) {
        SZrFunctionTypedTypeRef element;

        ZrCore_Memory_RawSet(&element, 0, sizeof(element));
        element.baseType = typeRef->elementBaseType;
        element.elementBaseType = ZR_VALUE_TYPE_OBJECT;
        element.typeName = typeRef->elementTypeName;
        return 1 + sizeof(TZrUInt32) + metadata_token_type_ref_signature_size(cs, &element);
    }

    typeNameLength = metadata_token_string_length(typeRef->typeName);
    if (typeNameLength > 0) {
        SZrString *unionBaseName = ZR_NULL;
        SZrArray unionArgumentTypeNames;
        SZrString *genericBaseName = ZR_NULL;
        SZrArray genericArgumentTypeNames;

        if (metadata_token_try_resolve_union_signature_type(cs,
                                                            typeRef->typeName,
                                                            &unionBaseName,
                                                            &unionArgumentTypeNames)) {
            TZrSize size = 1 + sizeof(TZrUInt32) + sizeof(TZrUInt32) + sizeof(TZrUInt32);

            for (TZrSize index = 0; index < unionArgumentTypeNames.length; index++) {
                SZrString **argumentTypeNamePtr =
                        (SZrString **)ZrCore_Array_Get(&unionArgumentTypeNames, index);
                SZrFunctionTypedTypeRef argumentTypeRef;

                metadata_token_type_ref_from_name(
                        argumentTypeNamePtr != ZR_NULL ? *argumentTypeNamePtr : ZR_NULL,
                        &argumentTypeRef);
                size += metadata_token_type_ref_signature_size(cs, &argumentTypeRef);
            }
            ZrCore_Array_Free(cs->state, &unionArgumentTypeNames);
            return size;
        }
        if (cs != ZR_NULL && cs->state != ZR_NULL &&
            try_parse_generic_instance_type_name(cs->state,
                                                 typeRef->typeName,
                                                 &genericBaseName,
                                                 &genericArgumentTypeNames)) {
            TZrSize openTypeLength = 1 + sizeof(TZrUInt32) + sizeof(TZrUInt32);
            TZrSize argumentLength =
                    metadata_token_generic_argument_signatures_size(cs, &genericArgumentTypeNames);

            ZrCore_Array_Free(cs->state, &genericArgumentTypeNames);
            return 1 + openTypeLength + sizeof(TZrUInt32) + argumentLength;
        }

        return 1 + sizeof(TZrUInt32) + sizeof(TZrUInt32);
    }

    return 1 + sizeof(TZrUInt32);
}

void metadata_token_write_type_ref_signature(TZrByte *buffer,
                                             TZrSize *offset,
                                             SZrCompilerState *cs,
                                             const SZrFunctionTypedTypeRef *typeRef,
                                             const SZrMetadataStringHeapEntry *stringHeapEntries,
                                             TZrUInt32 stringHeapEntryCount) {
    TZrNativeString typeNameText;
    TZrSize typeNameLength;

    if (typeRef == ZR_NULL) {
        metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_PRIMITIVE);
        metadata_token_write_u32(buffer, offset, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
        return;
    }

    if (typeRef->isNullable) {
        SZrFunctionTypedTypeRef nested = *typeRef;
        nested.isNullable = ZR_FALSE;
        metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_NULLABLE);
        metadata_token_write_type_ref_signature(buffer,
                                                offset,
                                                cs,
                                                &nested,
                                                stringHeapEntries,
                                                stringHeapEntryCount);
        return;
    }

    if (typeRef->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE) {
        SZrFunctionTypedTypeRef nested = *typeRef;
        nested.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
        metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_OWNERSHIP);
        metadata_token_write_u32(buffer, offset, typeRef->ownershipQualifier);
        metadata_token_write_type_ref_signature(buffer,
                                                offset,
                                                cs,
                                                &nested,
                                                stringHeapEntries,
                                                stringHeapEntryCount);
        return;
    }

    if (typeRef->isArray) {
        SZrFunctionTypedTypeRef element;

        ZrCore_Memory_RawSet(&element, 0, sizeof(element));
        element.baseType = typeRef->elementBaseType;
        element.elementBaseType = ZR_VALUE_TYPE_OBJECT;
        element.typeName = typeRef->elementTypeName;
        metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_ARRAY);
        metadata_token_write_u32(buffer, offset, 1u);
        metadata_token_write_type_ref_signature(buffer,
                                                offset,
                                                cs,
                                                &element,
                                                stringHeapEntries,
                                                stringHeapEntryCount);
        return;
    }

    typeNameText = metadata_token_string_text(typeRef->typeName);
    typeNameLength = typeNameText != ZR_NULL ? strlen(typeNameText) : 0;
    if (typeNameLength > 0) {
        SZrString *unionBaseName = ZR_NULL;
        SZrArray unionArgumentTypeNames;
        SZrString *genericBaseName = ZR_NULL;
        SZrArray genericArgumentTypeNames;

        if (metadata_token_try_resolve_union_signature_type(cs,
                                                            typeRef->typeName,
                                                            &unionBaseName,
                                                            &unionArgumentTypeNames)) {
            metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_UNION);
            metadata_token_write_u32(buffer, offset, (TZrUInt32)typeRef->baseType);
            metadata_token_write_string_ref(buffer,
                                            offset,
                                            unionBaseName,
                                            stringHeapEntries,
                                            stringHeapEntryCount);
            metadata_token_write_u32(buffer, offset, (TZrUInt32)unionArgumentTypeNames.length);

            for (TZrSize index = 0; index < unionArgumentTypeNames.length; index++) {
                SZrString **argumentTypeNamePtr =
                        (SZrString **)ZrCore_Array_Get(&unionArgumentTypeNames, index);
                SZrFunctionTypedTypeRef argumentTypeRef;

                metadata_token_type_ref_from_name(
                        argumentTypeNamePtr != ZR_NULL ? *argumentTypeNamePtr : ZR_NULL,
                        &argumentTypeRef);
                metadata_token_write_type_ref_signature(buffer,
                                                        offset,
                                                        cs,
                                                        &argumentTypeRef,
                                                        stringHeapEntries,
                                                        stringHeapEntryCount);
            }
            ZrCore_Array_Free(cs->state, &unionArgumentTypeNames);
            return;
        }
        if (cs != ZR_NULL && cs->state != ZR_NULL &&
            try_parse_generic_instance_type_name(cs->state,
                                                 typeRef->typeName,
                                                 &genericBaseName,
                                                 &genericArgumentTypeNames)) {
            metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_GENERIC_INST);
            metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_TYPE_REF);
            metadata_token_write_u32(buffer, offset, (TZrUInt32)typeRef->baseType);
            metadata_token_write_string_ref(buffer,
                                            offset,
                                            genericBaseName,
                                            stringHeapEntries,
                                            stringHeapEntryCount);
            metadata_token_write_u32(buffer, offset, (TZrUInt32)genericArgumentTypeNames.length);
            metadata_token_write_generic_argument_signatures(buffer,
                                                            offset,
                                                            cs,
                                                            &genericArgumentTypeNames,
                                                            stringHeapEntries,
                                                            stringHeapEntryCount);
            ZrCore_Array_Free(cs->state, &genericArgumentTypeNames);
            return;
        }

        metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_TYPE_REF);
        metadata_token_write_u32(buffer, offset, (TZrUInt32)typeRef->baseType);
        metadata_token_write_string_ref(buffer,
                                        offset,
                                        typeRef->typeName,
                                        stringHeapEntries,
                                        stringHeapEntryCount);
        return;
    }

    metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_PRIMITIVE);
    metadata_token_write_u32(buffer, offset, (TZrUInt32)typeRef->baseType);
}

TZrSize metadata_token_method_signature_size(SZrCompilerState *cs,
                                             const SZrFunctionTypedTypeRef *returnType,
                                             TZrUInt32 parameterCount,
                                             const SZrFunctionTypedTypeRef *parameterTypes) {
    TZrSize size;

    size = 1 + 1 + 1 + sizeof(TZrUInt32) +
           metadata_token_type_ref_signature_size(cs, returnType) +
           sizeof(TZrUInt32);
    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        const SZrFunctionTypedTypeRef *parameterType =
                parameterTypes != ZR_NULL ? &parameterTypes[index] : ZR_NULL;
        size += 1 + metadata_token_type_ref_signature_size(cs, parameterType);
    }
    return size;
}

void metadata_token_write_method_signature(TZrByte *buffer,
                                           TZrSize *offset,
                                           SZrCompilerState *cs,
                                           const SZrFunctionTypedTypeRef *returnType,
                                           TZrUInt32 parameterCount,
                                           const SZrFunctionTypedTypeRef *parameterTypes,
                                           const SZrMetadataStringHeapEntry *stringHeapEntries,
                                           TZrUInt32 stringHeapEntryCount) {
    metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_METHOD_SIG);
    metadata_token_write_u8(buffer, offset, 1u);
    metadata_token_write_u8(buffer, offset, 0u);
    metadata_token_write_u32(buffer, offset, 0u);
    metadata_token_write_type_ref_signature(buffer,
                                            offset,
                                            cs,
                                            returnType,
                                            stringHeapEntries,
                                            stringHeapEntryCount);
    metadata_token_write_u32(buffer, offset, parameterCount);
    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        const SZrFunctionTypedTypeRef *parameterType =
                parameterTypes != ZR_NULL ? &parameterTypes[index] : ZR_NULL;
        metadata_token_write_u8(buffer, offset, 0u);
        metadata_token_write_type_ref_signature(buffer,
                                                offset,
                                                cs,
                                                parameterType,
                                                stringHeapEntries,
                                                stringHeapEntryCount);
    }
}

TZrSize metadata_token_field_signature_size(SZrCompilerState *cs,
                                            const SZrFunctionTypedTypeRef *valueType) {
    return 1 + 1 + metadata_token_type_ref_signature_size(cs, valueType);
}

void metadata_token_write_field_signature(TZrByte *buffer,
                                          TZrSize *offset,
                                          SZrCompilerState *cs,
                                          const SZrFunctionTypedTypeRef *valueType,
                                          const SZrMetadataStringHeapEntry *stringHeapEntries,
                                          TZrUInt32 stringHeapEntryCount) {
    metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_FIELD_SIG);
    metadata_token_write_u8(buffer, offset, 1u);
    metadata_token_write_type_ref_signature(buffer,
                                            offset,
                                            cs,
                                            valueType,
                                            stringHeapEntries,
                                            stringHeapEntryCount);
}

TZrSize metadata_token_symbol_signature_size(SZrCompilerState *cs, const SZrFunctionTypedExportSymbol *symbol) {
    if (symbol == ZR_NULL) {
        return 0;
    }

    if (symbol->symbolKind != ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
        return metadata_token_field_signature_size(cs, &symbol->valueType);
    }

    return metadata_token_method_signature_size(cs,
                                                &symbol->valueType,
                                                symbol->parameterCount,
                                                symbol->parameterTypes);
}

void metadata_token_write_symbol_signature(TZrByte *buffer,
                                           TZrSize *offset,
                                           SZrCompilerState *cs,
                                           const SZrFunctionTypedExportSymbol *symbol,
                                           const SZrMetadataStringHeapEntry *stringHeapEntries,
                                           TZrUInt32 stringHeapEntryCount) {
    if (symbol == ZR_NULL) {
        return;
    }

    if (symbol->symbolKind != ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
        metadata_token_write_field_signature(buffer,
                                             offset,
                                             cs,
                                             &symbol->valueType,
                                             stringHeapEntries,
                                             stringHeapEntryCount);
        return;
    }

    metadata_token_write_method_signature(buffer,
                                          offset,
                                          cs,
                                          &symbol->valueType,
                                          symbol->parameterCount,
                                          symbol->parameterTypes,
                                          stringHeapEntries,
                                          stringHeapEntryCount);
}

void metadata_token_write_string_ref(TZrByte *buffer,
                                     TZrSize *offset,
                                     SZrString *value,
                                     const SZrMetadataStringHeapEntry *stringHeapEntries,
                                     TZrUInt32 stringHeapEntryCount) {
    TZrUInt32 stringIndex = metadata_token_string_heap_index(stringHeapEntries, stringHeapEntryCount, value);

    metadata_token_write_u32(buffer, offset, stringIndex);
}
