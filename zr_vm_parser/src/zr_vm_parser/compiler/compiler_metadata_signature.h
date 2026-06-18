#ifndef ZR_VM_PARSER_COMPILER_METADATA_SIGNATURE_H
#define ZR_VM_PARSER_COMPILER_METADATA_SIGNATURE_H

#include "compiler_internal.h"

TZrUInt64 metadata_signature_hash_v1(const TZrByte *signatureBlob, TZrSize signatureBlobLength);

TZrSize metadata_token_string_length(SZrString *stringValue);

TZrBool metadata_token_try_resolve_union_signature_type(SZrCompilerState *cs,
                                                        SZrString *typeName,
                                                        SZrString **outBaseName,
                                                        SZrArray *outArgumentTypeNames);

TZrUInt32 metadata_token_string_heap_index(const SZrMetadataStringHeapEntry *entries,
                                           TZrUInt32 entryCount,
                                           SZrString *value);

void metadata_token_write_u8(TZrByte *buffer, TZrSize *offset, TZrUInt8 value);

void metadata_token_write_u32(TZrByte *buffer, TZrSize *offset, TZrUInt32 value);

TZrSize metadata_token_type_ref_signature_size(SZrCompilerState *cs, const SZrFunctionTypedTypeRef *typeRef);

void metadata_token_write_type_ref_signature(TZrByte *buffer,
                                             TZrSize *offset,
                                             SZrCompilerState *cs,
                                             const SZrFunctionTypedTypeRef *typeRef,
                                             const SZrMetadataStringHeapEntry *stringHeapEntries,
                                             TZrUInt32 stringHeapEntryCount);

TZrSize metadata_token_symbol_signature_size(SZrCompilerState *cs, const SZrFunctionTypedExportSymbol *symbol);

void metadata_token_write_symbol_signature(TZrByte *buffer,
                                           TZrSize *offset,
                                           SZrCompilerState *cs,
                                           const SZrFunctionTypedExportSymbol *symbol,
                                           const SZrMetadataStringHeapEntry *stringHeapEntries,
                                           TZrUInt32 stringHeapEntryCount);

TZrSize metadata_token_method_signature_size(SZrCompilerState *cs,
                                             const SZrFunctionTypedTypeRef *returnType,
                                             TZrUInt32 parameterCount,
                                             const SZrFunctionTypedTypeRef *parameterTypes);

void metadata_token_write_method_signature(TZrByte *buffer,
                                           TZrSize *offset,
                                           SZrCompilerState *cs,
                                           const SZrFunctionTypedTypeRef *returnType,
                                           TZrUInt32 parameterCount,
                                           const SZrFunctionTypedTypeRef *parameterTypes,
                                           const SZrMetadataStringHeapEntry *stringHeapEntries,
                                           TZrUInt32 stringHeapEntryCount);

TZrSize metadata_token_field_signature_size(SZrCompilerState *cs,
                                            const SZrFunctionTypedTypeRef *valueType);

void metadata_token_write_field_signature(TZrByte *buffer,
                                          TZrSize *offset,
                                          SZrCompilerState *cs,
                                          const SZrFunctionTypedTypeRef *valueType,
                                          const SZrMetadataStringHeapEntry *stringHeapEntries,
                                          TZrUInt32 stringHeapEntryCount);

void metadata_token_write_string_ref(TZrByte *buffer,
                                     TZrSize *offset,
                                     SZrString *value,
                                     const SZrMetadataStringHeapEntry *stringHeapEntries,
                                     TZrUInt32 stringHeapEntryCount);

#endif
