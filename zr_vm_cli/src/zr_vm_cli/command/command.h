#ifndef ZR_VM_CLI_COMMAND_H
#define ZR_VM_CLI_COMMAND_H

#include "zr_vm_cli/conf.h"

struct SZrState;

typedef enum EZrCliMode {
    ZR_CLI_MODE_HELP = 0,
    ZR_CLI_MODE_VERSION = 1,
    ZR_CLI_MODE_REPL = 2,
    ZR_CLI_MODE_RUN_PROJECT = 3,
    ZR_CLI_MODE_COMPILE_PROJECT = 4,
    ZR_CLI_MODE_RUN_INLINE = 5,
    ZR_CLI_MODE_RUN_PROJECT_MODULE = 6
} EZrCliMode;

typedef enum EZrCliExecutionMode {
    ZR_CLI_EXECUTION_MODE_INTERP = 0,
    ZR_CLI_EXECUTION_MODE_BINARY = 1,
    ZR_CLI_EXECUTION_MODE_AOT_C = 2,
    ZR_CLI_EXECUTION_MODE_AOT_LLVM = 3
} EZrCliExecutionMode;

typedef struct SZrCliCommand {
    EZrCliMode mode;
    EZrCliExecutionMode executionMode;
    const TZrChar *projectPath;
    const TZrChar *inlineCode;
    const TZrChar *inlineModeAlias;
    const TZrChar *moduleName;
    const TZrChar *const *programArgs;
    TZrSize programArgCount;
    const TZrChar *debugAddress;
    TZrBool runAfterCompile;
    TZrBool interactiveAfterRun;
    TZrBool emitIntermediate;
    TZrBool incremental;
    TZrBool emitAotC;
    TZrBool emitAotLlvm;
    TZrBool requireAotPath;
    TZrBool emitExecutedVia;
    TZrBool debugEnabled;
    TZrBool debugWait;
    TZrBool debugPrintEndpoint;
} SZrCliCommand;

TZrBool ZrCli_Command_Parse(int argc,
                            char **argv,
                            SZrCliCommand *outCommand,
                            TZrChar *errorBuffer,
                            TZrSize errorBufferSize);

void ZrCli_Command_LogHelp(struct SZrState *state, TZrBool writeToStdErr, const TZrChar *programName);
void ZrCli_Command_LogVersion(struct SZrState *state, TZrBool writeToStdErr);

#endif
