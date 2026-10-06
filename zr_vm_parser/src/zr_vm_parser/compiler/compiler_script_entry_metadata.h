#ifndef ZR_VM_PARSER_COMPILER_SCRIPT_ENTRY_METADATA_H
#define ZR_VM_PARSER_COMPILER_SCRIPT_ENTRY_METADATA_H

#include "compiler_internal.h"

/* Per-build plan only; no retained IR or callable ABI fields are introduced. */
typedef struct SZrScriptEntryMetadataPlan {
    TZrBool present;
    TZrSize signatureHeapLength;
} SZrScriptEntryMetadataPlan;

TZrBool compiler_script_entry_metadata_prepare(SZrCompilerState *cs, SZrFunction *function);
TZrBool compiler_script_entry_metadata_plan(SZrCompilerState *cs, const SZrFunction *function,
                                           SZrScriptEntryMetadataPlan *plan);
/* Emit into caller-owned temporary buffers. Failure invalidates this build. */
TZrBool compiler_script_entry_metadata_emit(SZrCompilerState *cs, const SZrFunction *function,
        const SZrScriptEntryMetadataPlan *plan, SZrMetadataTokenRecord *records,
        TZrUInt32 recordCount, TZrUInt32 *recordIndex, TZrByte *heap, TZrSize heapLength,
        TZrSize *heapOffset, TZrUInt32 memberRid, TZrUInt32 *signatureRid,
        const SZrMetadataStringHeapEntry *strings, TZrUInt32 stringCount);
TZrUInt64 compiler_script_entry_metadata_hash(const SZrCompilerState *cs, const SZrFunction *function);

#endif
