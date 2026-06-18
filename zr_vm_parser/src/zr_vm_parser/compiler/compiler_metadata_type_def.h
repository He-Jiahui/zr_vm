#ifndef ZR_VM_PARSER_COMPILER_METADATA_TYPE_DEF_H
#define ZR_VM_PARSER_COMPILER_METADATA_TYPE_DEF_H

#include "compiler_internal.h"

typedef struct SZrMetadataTypeDefPlan {
    TZrUInt32 typeDefCount;
    TZrSize signatureHeapLength;
} SZrMetadataTypeDefPlan;

typedef TZrBool (*TZrMetadataTypeDefStringCollector)(SZrCompilerState *cs,
                                                     SZrString *value,
                                                     void *userData);

TZrBool compiler_metadata_type_def_collect_strings(SZrCompilerState *cs,
                                                   const SZrFunction *function,
                                                   TZrMetadataTypeDefStringCollector collector,
                                                   void *userData);

TZrBool compiler_metadata_type_def_plan(SZrCompilerState *cs,
                                        const SZrFunction *function,
                                        SZrMetadataTypeDefPlan *outPlan);

TZrBool compiler_metadata_type_def_emit(SZrCompilerState *cs,
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
