#include "backend_aot_c_type_layouts.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/string.h"

#include <string.h>

static TZrBool backend_aot_c_type_layout_string_equals(SZrString *lhs, SZrString *rhs) {
    if (lhs == rhs) {
        return ZR_TRUE;
    }
    if (lhs == ZR_NULL || rhs == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrCore_String_Equal(lhs, rhs);
}

static TZrBool backend_aot_c_type_layout_merge_unique_type_name(SZrString **outTypeName,
                                                                SZrString *candidateTypeName) {
    if (outTypeName == ZR_NULL || candidateTypeName == ZR_NULL) {
        return ZR_TRUE;
    }
    if (*outTypeName == ZR_NULL) {
        *outTypeName = candidateTypeName;
        return ZR_TRUE;
    }
    return backend_aot_c_type_layout_string_equals(*outTypeName, candidateTypeName);
}

static TZrBool backend_aot_c_type_layout_type_ref_names_layout_id(
        const SZrFunctionTypedTypeRef *typeRef,
        TZrUInt32 typeLayoutId,
        SZrString **outTypeName) {
    if (typeRef == ZR_NULL ||
        typeRef->typeName == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
        typeRef->staticCType != ZR_STATIC_C_TYPE_STRUCT ||
        typeRef->staticCTypeId != typeLayoutId) {
        return ZR_TRUE;
    }

    return backend_aot_c_type_layout_merge_unique_type_name(outTypeName, typeRef->typeName);
}

static TZrBool backend_aot_c_type_layout_binding_names_layout_id(
        const SZrFunction *function,
        const SZrFunctionTypedLocalBinding *binding,
        TZrUInt32 typeLayoutId,
        SZrString **outTypeName) {
    if (function == ZR_NULL || binding == ZR_NULL || typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_TRUE;
    }

    if (!backend_aot_c_type_layout_type_ref_names_layout_id(&binding->type, typeLayoutId, outTypeName)) {
        return ZR_FALSE;
    }

    if (binding->type.typeName == ZR_NULL || function->frameSlotLayouts == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrUInt32 slotIndex = 0u; slotIndex < function->frameSlotLayoutLength; slotIndex++) {
        const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[slotIndex];

        if (slotLayout->stackSlot == binding->stackSlot &&
            slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
            slotLayout->typeLayoutId == typeLayoutId) {
            return backend_aot_c_type_layout_merge_unique_type_name(outTypeName, binding->type.typeName);
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_type_layout_function_names_layout_id(const SZrFunction *function,
                                                                  TZrUInt32 typeLayoutId,
                                                                  SZrString **outTypeName) {
    if (function == ZR_NULL || typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_TRUE;
    }

    for (TZrUInt32 bindingIndex = 0u; bindingIndex < function->typedLocalBindingLength; bindingIndex++) {
        const SZrFunctionTypedLocalBinding *binding = function->typedLocalBindings != ZR_NULL
                                                              ? &function->typedLocalBindings[bindingIndex]
                                                              : ZR_NULL;
        if (!backend_aot_c_type_layout_binding_names_layout_id(function, binding, typeLayoutId, outTypeName)) {
            return ZR_FALSE;
        }
    }

    for (TZrUInt32 symbolIndex = 0u; symbolIndex < function->typedExportedSymbolLength; symbolIndex++) {
        const SZrFunctionTypedExportSymbol *symbol = function->typedExportedSymbols != ZR_NULL
                                                             ? &function->typedExportedSymbols[symbolIndex]
                                                             : ZR_NULL;
        if (symbol == ZR_NULL) {
            continue;
        }
        if (!backend_aot_c_type_layout_type_ref_names_layout_id(&symbol->valueType, typeLayoutId, outTypeName)) {
            return ZR_FALSE;
        }
        for (TZrUInt32 parameterIndex = 0u; parameterIndex < symbol->parameterCount; parameterIndex++) {
            const SZrFunctionTypedTypeRef *parameterType =
                    symbol->parameterTypes != ZR_NULL ? &symbol->parameterTypes[parameterIndex] : ZR_NULL;
            if (!backend_aot_c_type_layout_type_ref_names_layout_id(parameterType, typeLayoutId, outTypeName)) {
                return ZR_FALSE;
            }
        }
    }

    if (!backend_aot_c_type_layout_type_ref_names_layout_id(&function->callableReturnType,
                                                            typeLayoutId,
                                                            outTypeName)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 typeIndex = 0u; typeIndex < function->semIrTypeTableLength; typeIndex++) {
        const SZrFunctionTypedTypeRef *typeRef =
                function->semIrTypeTable != ZR_NULL ? &function->semIrTypeTable[typeIndex] : ZR_NULL;
        if (!backend_aot_c_type_layout_type_ref_names_layout_id(typeRef, typeLayoutId, outTypeName)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static SZrString *backend_aot_c_type_layout_type_name_from_table(const SZrAotFunctionTable *table,
                                                                 TZrUInt32 typeLayoutId) {
    SZrString *typeName = ZR_NULL;

    if (table == ZR_NULL || table->entries == ZR_NULL || typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_NULL;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        if (!backend_aot_c_type_layout_function_names_layout_id(table->entries[entryIndex].function,
                                                                typeLayoutId,
                                                                &typeName)) {
            return ZR_NULL;
        }
    }

    return typeName;
}

static TZrBool backend_aot_c_type_layout_read_u32(const TZrByte *buffer,
                                                  TZrUInt32 bufferLength,
                                                  TZrUInt32 offset,
                                                  TZrUInt32 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    *outValue = 0u;
    if (buffer == ZR_NULL || offset > bufferLength || bufferLength - offset < sizeof(TZrUInt32)) {
        return ZR_FALSE;
    }

    *outValue = (TZrUInt32)buffer[offset + 0u] |
                ((TZrUInt32)buffer[offset + 1u] << 8u) |
                ((TZrUInt32)buffer[offset + 2u] << 16u) |
                ((TZrUInt32)buffer[offset + 3u] << 24u);
    return ZR_TRUE;
}

static TZrBool backend_aot_c_type_layout_read_u8(const TZrByte *buffer,
                                                 TZrUInt32 bufferLength,
                                                 TZrUInt32 offset,
                                                 TZrUInt8 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    *outValue = 0u;
    if (buffer == ZR_NULL || offset >= bufferLength) {
        return ZR_FALSE;
    }

    *outValue = buffer[offset];
    return ZR_TRUE;
}

static SZrString *backend_aot_c_type_layout_metadata_string_heap_lookup(const SZrFunction *function,
                                                                        TZrUInt32 stringIndex) {
    if (function == ZR_NULL || function->metadataStringHeap == ZR_NULL || stringIndex == 0u) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0u; index < function->metadataStringHeapLength; index++) {
        const SZrMetadataStringHeapEntry *entry = &function->metadataStringHeap[index];

        if (entry->stringIndex == stringIndex) {
            return entry->value;
        }
    }

    return ZR_NULL;
}

static TZrBool backend_aot_c_type_layout_is_space(TZrChar ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static const TZrChar *backend_aot_c_type_layout_skip_spaces(const TZrChar *text) {
    if (text == ZR_NULL) {
        return ZR_NULL;
    }
    while (*text != '\0' && backend_aot_c_type_layout_is_space(*text)) {
        text++;
    }
    return text;
}

static void backend_aot_c_type_layout_trim_segment(const TZrChar **ioStart,
                                                   const TZrChar **ioEnd) {
    const TZrChar *start;
    const TZrChar *end;

    if (ioStart == ZR_NULL || ioEnd == ZR_NULL || *ioStart == ZR_NULL || *ioEnd == ZR_NULL) {
        return;
    }

    start = *ioStart;
    end = *ioEnd;
    while (start < end && backend_aot_c_type_layout_is_space(*start)) {
        start++;
    }
    while (end > start && backend_aot_c_type_layout_is_space(*(end - 1))) {
        end--;
    }

    *ioStart = start;
    *ioEnd = end;
}

static TZrBool backend_aot_c_type_layout_segment_equals_native(const TZrChar *start,
                                                               const TZrChar *end,
                                                               TZrNativeString native,
                                                               TZrSize nativeLength) {
    TZrSize segmentLength;

    if (start == ZR_NULL || end == ZR_NULL || native == ZR_NULL || end < start) {
        return ZR_FALSE;
    }
    backend_aot_c_type_layout_trim_segment(&start, &end);
    segmentLength = (TZrSize)(end - start);
    return (TZrBool)(segmentLength == nativeLength &&
                     (segmentLength == 0u || memcmp(start, native, segmentLength) == 0));
}

static TZrBool backend_aot_c_type_layout_segment_equals_string(const TZrChar *start,
                                                               const TZrChar *end,
                                                               SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }
    return backend_aot_c_type_layout_segment_equals_native(start,
                                                           end,
                                                           ZrCore_String_GetNativeString(value),
                                                           ZrCore_String_GetByteLength(value));
}

static TZrBool backend_aot_c_type_layout_segment_equals_literal(const TZrChar *start,
                                                                const TZrChar *end,
                                                                const TZrChar *literal) {
    return literal != ZR_NULL &&
           backend_aot_c_type_layout_segment_equals_native(start, end, (TZrNativeString)literal, strlen(literal));
}

static TZrBool backend_aot_c_type_layout_consume_type_segment(const TZrChar **ioText,
                                                              const TZrChar **outStart,
                                                              const TZrChar **outEnd) {
    const TZrChar *cursor;
    const TZrChar *start;
    TZrUInt32 genericDepth = 0u;

    if (ioText == ZR_NULL || *ioText == ZR_NULL || outStart == ZR_NULL || outEnd == ZR_NULL) {
        return ZR_FALSE;
    }

    cursor = backend_aot_c_type_layout_skip_spaces(*ioText);
    start = cursor;
    while (*cursor != '\0') {
        if (*cursor == '<') {
            genericDepth++;
        } else if (*cursor == '>') {
            if (genericDepth == 0u) {
                break;
            }
            genericDepth--;
        } else if (*cursor == ',' && genericDepth == 0u) {
            break;
        }
        cursor++;
    }

    if (genericDepth != 0u) {
        return ZR_FALSE;
    }

    *outStart = start;
    *outEnd = cursor;
    *ioText = cursor;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_type_layout_primitive_segment_matches(EZrValueType valueType,
                                                                   const TZrChar *start,
                                                                   const TZrChar *end) {
    switch (valueType) {
        case ZR_VALUE_TYPE_NULL:
            return backend_aot_c_type_layout_segment_equals_literal(start, end, "null");
        case ZR_VALUE_TYPE_BOOL:
            return backend_aot_c_type_layout_segment_equals_literal(start, end, "bool");
        case ZR_VALUE_TYPE_INT8:
            return backend_aot_c_type_layout_segment_equals_literal(start, end, "int8");
        case ZR_VALUE_TYPE_INT16:
            return backend_aot_c_type_layout_segment_equals_literal(start, end, "int16");
        case ZR_VALUE_TYPE_INT32:
            return backend_aot_c_type_layout_segment_equals_literal(start, end, "int32");
        case ZR_VALUE_TYPE_INT64:
            return backend_aot_c_type_layout_segment_equals_literal(start, end, "int") ||
                   backend_aot_c_type_layout_segment_equals_literal(start, end, "int64");
        case ZR_VALUE_TYPE_UINT8:
            return backend_aot_c_type_layout_segment_equals_literal(start, end, "uint8");
        case ZR_VALUE_TYPE_UINT16:
            return backend_aot_c_type_layout_segment_equals_literal(start, end, "uint16");
        case ZR_VALUE_TYPE_UINT32:
            return backend_aot_c_type_layout_segment_equals_literal(start, end, "uint32");
        case ZR_VALUE_TYPE_UINT64:
            return backend_aot_c_type_layout_segment_equals_literal(start, end, "uint") ||
                   backend_aot_c_type_layout_segment_equals_literal(start, end, "uint64");
        case ZR_VALUE_TYPE_FLOAT:
            return backend_aot_c_type_layout_segment_equals_literal(start, end, "float") ||
                   backend_aot_c_type_layout_segment_equals_literal(start, end, "float32");
        case ZR_VALUE_TYPE_DOUBLE:
            return backend_aot_c_type_layout_segment_equals_literal(start, end, "double") ||
                   backend_aot_c_type_layout_segment_equals_literal(start, end, "float64");
        case ZR_VALUE_TYPE_STRING:
            return backend_aot_c_type_layout_segment_equals_literal(start, end, "string");
        case ZR_VALUE_TYPE_OBJECT:
            return backend_aot_c_type_layout_segment_equals_literal(start, end, "object");
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_type_layout_read_signature_string(const SZrFunction *function,
                                                               const TZrByte *signature,
                                                               TZrUInt32 signatureLength,
                                                               TZrUInt32 *ioOffset,
                                                               SZrString **outName) {
    TZrUInt32 unusedBaseType;
    TZrUInt32 stringIndex;

    if (ioOffset == ZR_NULL || outName == ZR_NULL) {
        return ZR_FALSE;
    }
    *outName = ZR_NULL;

    if (!backend_aot_c_type_layout_read_u32(signature, signatureLength, *ioOffset, &unusedBaseType)) {
        return ZR_FALSE;
    }
    *ioOffset += (TZrUInt32)sizeof(TZrUInt32);
    if (!backend_aot_c_type_layout_read_u32(signature, signatureLength, *ioOffset, &stringIndex)) {
        return ZR_FALSE;
    }
    *ioOffset += (TZrUInt32)sizeof(TZrUInt32);
    *outName = backend_aot_c_type_layout_metadata_string_heap_lookup(function, stringIndex);
    return *outName != ZR_NULL;
}

static TZrBool backend_aot_c_type_layout_signature_matches_text(const SZrFunction *function,
                                                                const TZrByte *signature,
                                                                TZrUInt32 signatureLength,
                                                                TZrUInt32 *ioOffset,
                                                                const TZrChar **ioText);

static TZrBool backend_aot_c_type_layout_named_generic_signature_matches_text(
        const SZrFunction *function,
        const TZrByte *signature,
        TZrUInt32 signatureLength,
        TZrUInt32 *ioOffset,
        SZrString *baseName,
        TZrUInt32 argumentCount,
        const TZrChar **ioText) {
    const TZrChar *cursor;
    const TZrChar *baseStart;
    const TZrChar *baseEnd;

    if (function == ZR_NULL ||
        signature == ZR_NULL ||
        ioOffset == ZR_NULL ||
        baseName == ZR_NULL ||
        ioText == ZR_NULL ||
        *ioText == ZR_NULL) {
        return ZR_FALSE;
    }

    cursor = backend_aot_c_type_layout_skip_spaces(*ioText);
    baseStart = cursor;
    while (*cursor != '\0' && *cursor != '<' && *cursor != ',' && *cursor != '>') {
        cursor++;
    }
    baseEnd = cursor;
    if (*cursor != '<' || !backend_aot_c_type_layout_segment_equals_string(baseStart, baseEnd, baseName)) {
        return ZR_FALSE;
    }
    cursor++;

    for (TZrUInt32 argumentIndex = 0u; argumentIndex < argumentCount; argumentIndex++) {
        cursor = backend_aot_c_type_layout_skip_spaces(cursor);
        if (!backend_aot_c_type_layout_signature_matches_text(function,
                                                              signature,
                                                              signatureLength,
                                                              ioOffset,
                                                              &cursor)) {
            return ZR_FALSE;
        }
        cursor = backend_aot_c_type_layout_skip_spaces(cursor);
        if (argumentIndex + 1u < argumentCount) {
            if (*cursor != ',') {
                return ZR_FALSE;
            }
            cursor++;
        } else if (*cursor != '>') {
            return ZR_FALSE;
        } else {
            cursor++;
        }
    }

    *ioText = cursor;
    return ZR_TRUE;
}

static TZrBool backend_aot_c_type_layout_signature_matches_text(const SZrFunction *function,
                                                                const TZrByte *signature,
                                                                TZrUInt32 signatureLength,
                                                                TZrUInt32 *ioOffset,
                                                                const TZrChar **ioText) {
    TZrUInt8 node;
    TZrUInt32 value;
    const TZrChar *segmentStart;
    const TZrChar *segmentEnd;

    if (function == ZR_NULL ||
        signature == ZR_NULL ||
        ioOffset == ZR_NULL ||
        ioText == ZR_NULL ||
        *ioText == ZR_NULL ||
        !backend_aot_c_type_layout_read_u8(signature, signatureLength, *ioOffset, &node)) {
        return ZR_FALSE;
    }
    *ioOffset += 1u;

    switch ((EZrMetadataSignatureNode)node) {
        case ZR_METADATA_SIGNATURE_NODE_PRIMITIVE:
            if (!backend_aot_c_type_layout_read_u32(signature, signatureLength, *ioOffset, &value)) {
                return ZR_FALSE;
            }
            *ioOffset += (TZrUInt32)sizeof(TZrUInt32);
            return backend_aot_c_type_layout_consume_type_segment(ioText, &segmentStart, &segmentEnd) &&
                   backend_aot_c_type_layout_primitive_segment_matches((EZrValueType)value,
                                                                       segmentStart,
                                                                       segmentEnd);
        case ZR_METADATA_SIGNATURE_NODE_TYPE_REF:
        case ZR_METADATA_SIGNATURE_NODE_TYPE_DEF: {
            SZrString *name = ZR_NULL;

            if (!backend_aot_c_type_layout_read_signature_string(function,
                                                                 signature,
                                                                 signatureLength,
                                                                 ioOffset,
                                                                 &name)) {
                return ZR_FALSE;
            }
            return backend_aot_c_type_layout_consume_type_segment(ioText, &segmentStart, &segmentEnd) &&
                   backend_aot_c_type_layout_segment_equals_string(segmentStart, segmentEnd, name);
        }
        case ZR_METADATA_SIGNATURE_NODE_GENERIC_INST: {
            TZrUInt8 openNode;
            SZrString *baseName = ZR_NULL;

            if (!backend_aot_c_type_layout_read_u8(signature, signatureLength, *ioOffset, &openNode)) {
                return ZR_FALSE;
            }
            *ioOffset += 1u;
            if ((EZrMetadataSignatureNode)openNode != ZR_METADATA_SIGNATURE_NODE_TYPE_REF &&
                (EZrMetadataSignatureNode)openNode != ZR_METADATA_SIGNATURE_NODE_TYPE_DEF) {
                return ZR_FALSE;
            }
            if (!backend_aot_c_type_layout_read_signature_string(function,
                                                                 signature,
                                                                 signatureLength,
                                                                 ioOffset,
                                                                 &baseName) ||
                !backend_aot_c_type_layout_read_u32(signature, signatureLength, *ioOffset, &value)) {
                return ZR_FALSE;
            }
            *ioOffset += (TZrUInt32)sizeof(TZrUInt32);
            return backend_aot_c_type_layout_named_generic_signature_matches_text(function,
                                                                                  signature,
                                                                                  signatureLength,
                                                                                  ioOffset,
                                                                                  baseName,
                                                                                  value,
                                                                                  ioText);
        }
        case ZR_METADATA_SIGNATURE_NODE_UNION: {
            SZrString *baseName = ZR_NULL;

            if (!backend_aot_c_type_layout_read_signature_string(function,
                                                                 signature,
                                                                 signatureLength,
                                                                 ioOffset,
                                                                 &baseName) ||
                !backend_aot_c_type_layout_read_u32(signature, signatureLength, *ioOffset, &value)) {
                return ZR_FALSE;
            }
            *ioOffset += (TZrUInt32)sizeof(TZrUInt32);
            if (value == 0u) {
                return backend_aot_c_type_layout_consume_type_segment(ioText, &segmentStart, &segmentEnd) &&
                       backend_aot_c_type_layout_segment_equals_string(segmentStart, segmentEnd, baseName);
            }
            return backend_aot_c_type_layout_named_generic_signature_matches_text(function,
                                                                                  signature,
                                                                                  signatureLength,
                                                                                  ioOffset,
                                                                                  baseName,
                                                                                  value,
                                                                                  ioText);
        }
        case ZR_METADATA_SIGNATURE_NODE_ARRAY:
            if (!backend_aot_c_type_layout_read_u32(signature, signatureLength, *ioOffset, &value)) {
                return ZR_FALSE;
            }
            *ioOffset += (TZrUInt32)sizeof(TZrUInt32);
            return backend_aot_c_type_layout_signature_matches_text(function,
                                                                    signature,
                                                                    signatureLength,
                                                                    ioOffset,
                                                                    ioText);
        case ZR_METADATA_SIGNATURE_NODE_NULLABLE:
            return backend_aot_c_type_layout_signature_matches_text(function,
                                                                    signature,
                                                                    signatureLength,
                                                                    ioOffset,
                                                                    ioText);
        case ZR_METADATA_SIGNATURE_NODE_OWNERSHIP:
            if (!backend_aot_c_type_layout_read_u32(signature, signatureLength, *ioOffset, &value)) {
                return ZR_FALSE;
            }
            *ioOffset += (TZrUInt32)sizeof(TZrUInt32);
            return backend_aot_c_type_layout_signature_matches_text(function,
                                                                    signature,
                                                                    signatureLength,
                                                                    ioOffset,
                                                                    ioText);
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_type_layout_type_spec_record_matches_name(const SZrFunction *function,
                                                                       const SZrMetadataTokenRecord *record,
                                                                       SZrString *typeName) {
    const TZrByte *signature;
    const TZrChar *typeNameText;
    TZrUInt32 signatureOffset = 0u;

    if (function == ZR_NULL ||
        record == ZR_NULL ||
        typeName == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_TYPE_SPEC ||
        record->signatureBlobLength == 0u ||
        record->signatureBlobOffset > function->signatureBlobHeapLength ||
        record->signatureBlobLength > function->signatureBlobHeapLength - record->signatureBlobOffset) {
        return ZR_FALSE;
    }

    signature = function->signatureBlobHeap + record->signatureBlobOffset;
    typeNameText = ZrCore_String_GetNativeString(typeName);
    if (typeNameText == ZR_NULL ||
        !backend_aot_c_type_layout_signature_matches_text(function,
                                                          signature,
                                                          record->signatureBlobLength,
                                                          &signatureOffset,
                                                          &typeNameText)) {
        return ZR_FALSE;
    }

    typeNameText = backend_aot_c_type_layout_skip_spaces(typeNameText);
    return (TZrBool)(signatureOffset == record->signatureBlobLength &&
                     typeNameText != ZR_NULL &&
                     *typeNameText == '\0');
}

static SZrString *backend_aot_c_type_layout_type_def_record_name(const SZrFunction *function,
                                                                 const SZrMetadataTokenRecord *record) {
    const TZrByte *signature;
    TZrUInt32 nameStringIndex;
    TZrUInt32 signatureOffset;

    if (function == ZR_NULL ||
        record == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_TYPE_DEF ||
        record->signatureBlobLength < 1u + sizeof(TZrUInt32) ||
        record->signatureBlobOffset > function->signatureBlobHeapLength ||
        record->signatureBlobLength > function->signatureBlobHeapLength - record->signatureBlobOffset) {
        return ZR_NULL;
    }

    signature = function->signatureBlobHeap + record->signatureBlobOffset;
    if (signature[0] != (TZrByte)ZR_METADATA_SIGNATURE_NODE_TYPE_DEF) {
        return ZR_NULL;
    }

    signatureOffset = 1u;
    if (!backend_aot_c_type_layout_read_u32(signature,
                                            record->signatureBlobLength,
                                            signatureOffset,
                                            &nameStringIndex)) {
        return ZR_NULL;
    }

    return backend_aot_c_type_layout_metadata_string_heap_lookup(function, nameStringIndex);
}

static TZrMetadataToken backend_aot_c_type_layout_type_def_token_for_name(const SZrFunction *function,
                                                                          SZrString *typeName) {
    TZrMetadataToken token = 0u;

    if (function == ZR_NULL ||
        typeName == ZR_NULL ||
        function->metadataTokenRecords == ZR_NULL ||
        function->metadataTokenRecordLength == 0u) {
        return 0u;
    }

    for (TZrUInt32 recordIndex = 0u; recordIndex < function->metadataTokenRecordLength; recordIndex++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[recordIndex];
        SZrString *recordName = backend_aot_c_type_layout_type_def_record_name(function, record);

        if (!backend_aot_c_type_layout_string_equals(recordName, typeName)) {
            continue;
        }
        if (token != 0u && token != record->token) {
            return 0u;
        }
        token = record->token;
    }

    return token;
}

static TZrMetadataToken backend_aot_c_type_layout_type_def_token_from_table(const SZrAotFunctionTable *table,
                                                                            SZrString *typeName) {
    TZrMetadataToken token = 0u;

    if (table == ZR_NULL || table->entries == ZR_NULL || typeName == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        TZrMetadataToken candidate =
                backend_aot_c_type_layout_type_def_token_for_name(table->entries[entryIndex].function, typeName);

        if (candidate == 0u) {
            continue;
        }
        if (token != 0u && token != candidate) {
            return 0u;
        }
        token = candidate;
    }

    return token;
}

static TZrMetadataToken backend_aot_c_type_layout_type_spec_token_for_name(const SZrFunction *function,
                                                                           SZrString *typeName) {
    TZrMetadataToken token = 0u;

    if (function == ZR_NULL ||
        typeName == ZR_NULL ||
        function->metadataTokenRecords == ZR_NULL ||
        function->metadataTokenRecordLength == 0u) {
        return 0u;
    }

    for (TZrUInt32 recordIndex = 0u; recordIndex < function->metadataTokenRecordLength; recordIndex++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[recordIndex];

        if (!backend_aot_c_type_layout_type_spec_record_matches_name(function, record, typeName)) {
            continue;
        }
        if (token != 0u && token != record->token) {
            return 0u;
        }
        token = record->token;
    }

    return token;
}

static TZrMetadataToken backend_aot_c_type_layout_type_spec_token_from_table(const SZrAotFunctionTable *table,
                                                                             SZrString *typeName) {
    TZrMetadataToken token = 0u;

    if (table == ZR_NULL || table->entries == ZR_NULL || typeName == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        TZrMetadataToken candidate =
                backend_aot_c_type_layout_type_spec_token_for_name(table->entries[entryIndex].function, typeName);

        if (candidate == 0u) {
            continue;
        }
        if (token != 0u && token != candidate) {
            return 0u;
        }
        token = candidate;
    }

    return token;
}

static TZrMetadataToken backend_aot_c_type_layout_token_from_table(SZrState *state,
                                                                   const SZrAotFunctionTable *table,
                                                                   TZrUInt32 typeLayoutId) {
    SZrString *typeName;
    TZrMetadataToken token;

    if (backend_aot_c_type_layout_resolve_from_table(state, table, typeLayoutId) == ZR_NULL ||
        table == ZR_NULL ||
        table->entries == ZR_NULL) {
        return 0u;
    }

    typeName = backend_aot_c_type_layout_type_name_from_table(table, typeLayoutId);
    if (typeName == ZR_NULL) {
        return 0u;
    }

    token = backend_aot_c_type_layout_type_def_token_from_table(table, typeName);
    if (token != 0u) {
        return token;
    }
    return backend_aot_c_type_layout_type_spec_token_from_table(table, typeName);
}

void backend_aot_write_c_type_layout_token_table(FILE *file,
                                                 SZrState *state,
                                                 const SZrAotFunctionTable *table,
                                                 TZrUInt32 typeLayoutIndexSpace) {
    if (file == ZR_NULL || state == ZR_NULL || table == ZR_NULL || typeLayoutIndexSpace == 0u) {
        return;
    }

    fprintf(file,
            "/* AOT metadata token table indexed by cTypeId/typeLayoutId. */\n"
            "static const TZrUInt32 zr_aot_type_layout_tokens[] = {\n");
    for (TZrUInt32 typeLayoutId = 0u; typeLayoutId < typeLayoutIndexSpace; typeLayoutId++) {
        TZrMetadataToken token = backend_aot_c_type_layout_token_from_table(state, table, typeLayoutId);

        if (token != 0u) {
            fprintf(file, "    0x%08xu,\n", (unsigned)token);
        } else {
            fprintf(file, "    0u,\n");
        }
    }
    fprintf(file, "};\n\n");
}
