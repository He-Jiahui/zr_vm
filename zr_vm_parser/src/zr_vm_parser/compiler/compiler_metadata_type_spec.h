#ifndef ZR_VM_PARSER_COMPILER_METADATA_TYPE_SPEC_H
#define ZR_VM_PARSER_COMPILER_METADATA_TYPE_SPEC_H

#include "compiler_internal.h"

typedef struct SZrMetadataTypeSpecPlan {
    TZrUInt32 typeSpecCount;
    TZrSize signatureHeapLength;
} SZrMetadataTypeSpecPlan;

TZrBool compiler_metadata_type_spec_plan(SZrCompilerState *cs,
                                         const SZrFunction *function,
                                         const SZrMetadataStringHeapEntry *stringHeapEntries,
                                         TZrUInt32 stringHeapEntryCount,
                                         SZrMetadataTypeSpecPlan *outPlan);

TZrBool compiler_metadata_type_spec_emit(SZrCompilerState *cs,
                                         const SZrFunction *function,
                                         SZrMetadataTokenRecord *records,
                                         TZrUInt32 recordCount,
                                         TZrUInt32 *ioRecordIndex,
                                         TZrByte *heap,
                                         TZrSize heapLength,
                                         TZrSize *ioHeapOffset,
                                         TZrUInt32 *ioSignatureRidCursor,
                                         const SZrMetadataStringHeapEntry *stringHeapEntries,
                                         TZrUInt32 stringHeapEntryCount);

#endif
