#ifndef ZR_VM_PARSER_COMPILER_METADATA_MODULE_HASH_H
#define ZR_VM_PARSER_COMPILER_METADATA_MODULE_HASH_H

#include "compiler_internal.h"

TZrUInt64 metadata_token_compute_module_signature_hash(
        SZrCompilerState *cs,
        const SZrFunction *function,
        const SZrMetadataStringHeapEntry *stringHeapEntries,
        TZrUInt32 stringHeapEntryCount);

#endif // ZR_VM_PARSER_COMPILER_METADATA_MODULE_HASH_H
