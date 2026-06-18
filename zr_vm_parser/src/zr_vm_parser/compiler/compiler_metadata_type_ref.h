#ifndef ZR_VM_PARSER_COMPILER_METADATA_TYPE_REF_H
#define ZR_VM_PARSER_COMPILER_METADATA_TYPE_REF_H

#include "compiler_internal.h"

typedef struct SZrMetadataExternalTypeRefPlan {
    TZrUInt32 typeRefCount;
    TZrSize signatureHeapLength;
} SZrMetadataExternalTypeRefPlan;

typedef struct SZrMetadataTokenTargetSignature {
    TZrUInt8 symbolKind;
    TZrUInt8 exportKind;
    TZrUInt8 readiness;
    TZrUInt8 reserved0;
    SZrFunctionTypedTypeRef valueType;
    TZrUInt32 parameterCount;
    SZrFunctionTypedTypeRef *parameterTypes;
    TZrMetadataToken metadataToken;
    TZrMetadataToken signatureToken;
    TZrUInt64 signatureHash;
    TZrBool hasSignature;
} SZrMetadataTokenTargetSignature;
typedef TZrBool (*TZrMetadataTypeRefTargetSignatureResolver)(SZrCompilerState *cs,
                                                             const SZrFunctionModuleEffect *effect,
                                                             SZrMetadataTokenTargetSignature *outSignature);
typedef const SZrFunctionModuleEffect *(*TZrMetadataTypeRefEffectByFlatIndex)(const SZrFunction *function,
                                                                              TZrUInt32 effectIndex);
typedef TZrUInt32 (*TZrMetadataTypeRefAssemblyRefRidResolver)(SZrCompilerState *cs,
                                                              const SZrFunction *function,
                                                              TZrUInt32 totalEffectCount,
                                                              TZrMetadataTypeRefEffectByFlatIndex effectAt,
                                                              SZrString *moduleName,
                                                              void *userData);

TZrBool compiler_metadata_type_ref_split_module_qualified_type(SZrCompilerState *cs,
                                                               SZrString *typeName,
                                                               SZrString **outModuleName,
                                                               SZrString **outMemberTypeName);

TZrBool compiler_metadata_type_ref_resolve_unqualified_alias(SZrCompilerState *cs,
                                                             SZrString *typeName,
                                                             SZrString **outModuleName,
                                                             SZrString **outMemberTypeName);

TZrBool compiler_metadata_type_ref_plan(SZrCompilerState *cs,
                                        const SZrFunction *function,
                                        TZrUInt32 totalEffectCount,
                                        TZrMetadataTypeRefEffectByFlatIndex effectAt,
                                        TZrMetadataTypeRefTargetSignatureResolver resolveTarget,
                                        const SZrMetadataStringHeapEntry *stringHeapEntries,
                                        TZrUInt32 stringHeapEntryCount,
                                        SZrMetadataExternalTypeRefPlan *outPlan);

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
                                        TZrUInt32 stringHeapEntryCount);

#endif
