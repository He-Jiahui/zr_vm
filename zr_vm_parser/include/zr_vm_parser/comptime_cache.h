#ifndef ZR_VM_PARSER_COMPTIME_CACHE_H
#define ZR_VM_PARSER_COMPTIME_CACHE_H

#include "zr_vm_parser/compiler.h"

typedef struct SZrParserSourceComptimeCache {
    const TZrByte *inputSnapshot;
    TZrSize inputSnapshotSize;
    TZrByte *outputSnapshot;
    TZrSize outputSnapshotSize;
    TZrBool inputSnapshotAccepted;
    TZrUInt64 hitCount;
    TZrUInt64 missCount;
} SZrParserSourceComptimeCache;

ZR_PARSER_API struct SZrFunction *ZrParser_Source_CompileWithComptimeCache(
        struct SZrState *state,
        const TZrChar *source,
        TZrSize sourceLength,
        struct SZrString *sourceName,
        SZrParserSourceComptimeCache *cache);

ZR_PARSER_API TZrBool ZrParser_ComptimeCache_ExportSnapshot(
        const SZrCompilerState *compiler,
        TZrByte **outBytes,
        TZrSize *outSize);

ZR_PARSER_API TZrBool ZrParser_ComptimeCache_ImportSnapshot(
        SZrCompilerState *compiler,
        const TZrByte *bytes,
        TZrSize size);

ZR_PARSER_API void ZrParser_ComptimeCache_FreeSnapshot(
        SZrState *state,
        TZrByte *bytes,
        TZrSize size);

#endif
