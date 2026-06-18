#ifndef ZR_VM_PARSER_COMPILER_METADATA_MODULE_RECORD_H
#define ZR_VM_PARSER_COMPILER_METADATA_MODULE_RECORD_H

#include "compiler_internal.h"
#include "compiler_metadata_type_def.h"

typedef struct SZrMetadataModuleRecordPlan {
    TZrUInt32 moduleRecordCount;
    TZrSize signatureHeapLength;
} SZrMetadataModuleRecordPlan;

TZrBool compiler_metadata_module_record_collect_strings(SZrCompilerState *cs,
                                                        const SZrFunction *function,
                                                        TZrMetadataTypeDefStringCollector collector,
                                                        void *userData);

TZrBool compiler_metadata_module_record_plan(SZrCompilerState *cs,
                                             const SZrFunction *function,
                                             SZrMetadataModuleRecordPlan *outPlan);

TZrBool compiler_metadata_module_record_emit(SZrCompilerState *cs,
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
