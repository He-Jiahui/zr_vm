#ifndef ZR_VM_CLI_EXPLAIN_OPTIMIZE_COMMAND_H
#define ZR_VM_CLI_EXPLAIN_OPTIMIZE_COMMAND_H

#include <stdio.h>

#include "zr_vm_core/optimization_remark.h"
#include "zr_vm_cli/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SZrCliExplainOptimizeOptions {
    TZrUInt64 moduleHash;
    TZrUInt64 irHash;
    TZrUInt64 sourceVersion;
    TZrUInt32 sourceId;
    TZrUInt32 reasonMask;
    TZrUInt32 statusMask;
    TZrUInt32 backendMask;
    TZrUInt32 evidenceMask;
    TZrUInt32 pageOffset;
    TZrUInt32 pageLimit;
    TZrBool json;
    TZrBool hasModuleHash;
    TZrBool hasIrHash;
    TZrBool hasSourceVersion;
    TZrBool hasSourceId;
    TZrBool hasSourceRange;
    TZrBool hasPass;
    TZrBool hasModule;
    TZrBool reserved;
    SZrOptimizationRemarkSourceRange sourceRange;
    char pass[ZR_OPTIMIZATION_REMARK_PASS_NAME_MAX];
    char module[ZR_OPTIMIZATION_REMARK_MODULE_NAME_MAX];
} SZrCliExplainOptimizeOptions;

ZR_CLI_API void ZrCli_ExplainOptimizeOptions_Init(
        SZrCliExplainOptimizeOptions *options);

/* Parse the arguments after the `explain optimize` command.  The parser is
 * deliberately independent from project/artifact loading: callers provide a
 * remark store to RunStore once compilation has produced one. */
ZR_CLI_API TZrBool ZrCli_ExplainOptimizeOptions_Parse(
        int argc,
        const TZrChar *const *argv,
        SZrCliExplainOptimizeOptions *options,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);

/* Project canonical remarks to the CLI text or stable JSON representation. */
ZR_CLI_API int ZrCli_ExplainOptimize_RunStore(
        const SZrOptimizationRemarkStore *store,
        const SZrCliExplainOptimizeOptions *options,
        FILE *output,
        FILE *errorOutput);

/* Alias used by command dispatchers that already have a parsed query. */
#define ZrCli_ExplainOptimize_Run ZrCli_ExplainOptimize_RunStore
#define ZrCli_ExplainOptimize_Parse ZrCli_ExplainOptimizeOptions_Parse

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CLI_EXPLAIN_OPTIMIZE_COMMAND_H */
