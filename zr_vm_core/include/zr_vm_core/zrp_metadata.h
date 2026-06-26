#ifndef ZR_VM_CORE_ZRP_METADATA_H
#define ZR_VM_CORE_ZRP_METADATA_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/metadata_token.h"

#define ZR_ZRP_METADATA_MAGIC ((TZrUInt32)0x4D50525Au)
#define ZR_ZRP_METADATA_VERSION ((TZrUInt16)2u)
#define ZR_ZRP_METADATA_SECTION_COUNT 12u
#define ZR_ZRP_METADATA_HEADER_SIZE 208u

typedef enum EZrZrpMetadataSectionKind {
    ZR_ZRP_METADATA_SECTION_TOKEN_RECORDS = 0,
    ZR_ZRP_METADATA_SECTION_TYPE_DEFS = 1,
    ZR_ZRP_METADATA_SECTION_METHOD_DEFS = 2,
    ZR_ZRP_METADATA_SECTION_FIELD_DEFS = 3,
    ZR_ZRP_METADATA_SECTION_GENERIC_PARAMS = 4,
    ZR_ZRP_METADATA_SECTION_GENERIC_PARAM_CONSTRAINTS = 5,
    ZR_ZRP_METADATA_SECTION_TYPE_SPECS = 6,
    ZR_ZRP_METADATA_SECTION_METHOD_SPECS = 7,
    ZR_ZRP_METADATA_SECTION_MODULE_REFS = 8,
    ZR_ZRP_METADATA_SECTION_STRING_POOL = 9,
    ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL = 10,
    ZR_ZRP_METADATA_SECTION_CONSTANT_POOL = 11
} EZrZrpMetadataSectionKind;

typedef struct SZrZrpMetadataSection {
    TZrUInt32 offset;
    TZrUInt32 byteLength;
    TZrUInt32 count;
    TZrUInt32 elementSize;
} SZrZrpMetadataSection;

typedef struct SZrZrpMetadataSectionView {
    const SZrZrpMetadataSection *section;
    const TZrByte *data;
    TZrSize byteLength;
    TZrUInt32 count;
    TZrUInt32 elementSize;
} SZrZrpMetadataSectionView;

typedef struct SZrZrpMetadataPoolSliceView {
    const TZrByte *data;
    TZrSize byteLength;
} SZrZrpMetadataPoolSliceView;

typedef struct SZrZrpMetadataStringView {
    const char *data;
    TZrSize byteLength;
} SZrZrpMetadataStringView;

typedef struct SZrZrpMetadataTypeDefRow {
    TZrMetadataToken token;
    TZrUInt32 nameStringOffset;
    TZrUInt32 namespaceStringOffset;
    TZrUInt32 firstMethodDefIndex;
    TZrUInt32 methodDefCount;
    TZrUInt32 firstFieldDefIndex;
    TZrUInt32 fieldDefCount;
    TZrUInt32 firstGenericParamIndex;
    TZrUInt32 genericParamCount;
    TZrUInt32 flags;
    TZrUInt32 typeLayoutId;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
} SZrZrpMetadataTypeDefRow;

typedef struct SZrZrpMetadataMethodDefRow {
    TZrMetadataToken token;
    TZrMetadataToken ownerTypeToken;
    TZrUInt32 nameStringOffset;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
    TZrUInt32 functionIndex;
    TZrUInt32 firstGenericParamIndex;
    TZrUInt32 genericParamCount;
    TZrUInt32 flags;
} SZrZrpMetadataMethodDefRow;

typedef struct SZrZrpMetadataFieldDefRow {
    TZrMetadataToken token;
    TZrMetadataToken ownerTypeToken;
    TZrUInt32 nameStringOffset;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
    TZrUInt32 byteOffset;
    TZrUInt32 typeLayoutId;
    TZrUInt32 flags;
} SZrZrpMetadataFieldDefRow;

typedef struct SZrZrpMetadataGenericParamRow {
    TZrMetadataToken ownerToken;
    TZrUInt32 nameStringOffset;
    TZrUInt32 parameterIndex;
    TZrUInt32 firstConstraintIndex;
    TZrUInt32 constraintCount;
    TZrUInt32 flags;
} SZrZrpMetadataGenericParamRow;

typedef struct SZrZrpMetadataGenericParamConstraintRow {
    TZrUInt32 genericParamIndex;
    TZrMetadataToken constraintTypeToken;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
} SZrZrpMetadataGenericParamConstraintRow;

typedef struct SZrZrpMetadataTypeSpecRow {
    TZrMetadataToken token;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
    TZrUInt32 typeLayoutId;
    TZrUInt64 signatureHash;
} SZrZrpMetadataTypeSpecRow;

typedef struct SZrZrpMetadataMethodSpecRow {
    TZrMetadataToken token;
    TZrMetadataToken methodToken;
    TZrUInt32 instantiationBlobOffset;
    TZrUInt32 instantiationBlobLength;
    TZrUInt64 instantiationHash;
} SZrZrpMetadataMethodSpecRow;

typedef struct SZrZrpMetadataModuleRefRow {
    TZrMetadataToken token;
    TZrUInt32 nameStringOffset;
    TZrUInt32 versionStringOffset;
    TZrUInt32 flags;
    TZrUInt64 moduleSignatureHash;
} SZrZrpMetadataModuleRefRow;

typedef struct SZrZrpMetadataHeader {
    TZrUInt32 magic;
    TZrUInt16 version;
    TZrUInt16 headerSize;
    TZrUInt32 flags;
    TZrUInt32 sectionCount;
    SZrZrpMetadataSection tokenRecords;
    SZrZrpMetadataSection typeDefs;
    SZrZrpMetadataSection methodDefs;
    SZrZrpMetadataSection fieldDefs;
    SZrZrpMetadataSection genericParams;
    SZrZrpMetadataSection genericParamConstraints;
    SZrZrpMetadataSection typeSpecs;
    SZrZrpMetadataSection methodSpecs;
    SZrZrpMetadataSection moduleRefs;
    SZrZrpMetadataSection stringPool;
    SZrZrpMetadataSection signatureBlobPool;
    SZrZrpMetadataSection constantPool;
} SZrZrpMetadataHeader;

ZR_CORE_API void ZrCore_ZrpMetadata_InitHeader(SZrZrpMetadataHeader *header);

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_ValidateHeader(const SZrZrpMetadataHeader *header, TZrSize bufferLength);

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_WriteHeader(TZrByte *buffer,
                                                   TZrSize bufferLength,
                                                   const SZrZrpMetadataHeader *header);

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_ReadHeader(const TZrByte *buffer,
                                                  TZrSize bufferLength,
                                                  SZrZrpMetadataHeader *outHeader);

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_GetSectionView(const TZrByte *buffer,
                                                      TZrSize bufferLength,
                                                      const SZrZrpMetadataHeader *header,
                                                      EZrZrpMetadataSectionKind sectionKind,
                                                      SZrZrpMetadataSectionView *outView);

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_GetPoolSlice(const TZrByte *buffer,
                                                    TZrSize bufferLength,
                                                    const SZrZrpMetadataHeader *header,
                                                    EZrZrpMetadataSectionKind poolKind,
                                                    TZrUInt32 offset,
                                                    TZrUInt32 byteLength,
                                                    SZrZrpMetadataPoolSliceView *outSlice);

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_GetString(const TZrByte *buffer,
                                                 TZrSize bufferLength,
                                                 const SZrZrpMetadataHeader *header,
                                                 TZrUInt32 stringOffset,
                                                 SZrZrpMetadataStringView *outString);

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_ValidateSignatureBlob(const TZrByte *signatureBlob,
                                                             TZrSize signatureBlobLength);

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_WritePoolPayload(TZrByte *buffer,
                                                        TZrSize bufferLength,
                                                        const SZrZrpMetadataHeader *header,
                                                        EZrZrpMetadataSectionKind poolKind,
                                                        const TZrByte *payload,
                                                        TZrUInt32 payloadLength);

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_WriteDefinitionTablePayload(TZrByte *buffer,
                                                                   TZrSize bufferLength,
                                                                   const SZrZrpMetadataHeader *header,
                                                                   EZrZrpMetadataSectionKind tableKind,
                                                                   const void *rows,
                                                                   TZrUInt32 rowCount,
                                                                   TZrUInt32 elementSize);

ZR_CORE_API TZrBool ZrCore_ZrpMetadata_ValidateDefinitionTables(const TZrByte *buffer,
                                                                TZrSize bufferLength,
                                                                const SZrZrpMetadataHeader *header);

#endif // ZR_VM_CORE_ZRP_METADATA_H
