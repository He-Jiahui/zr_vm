#ifndef ZR_VM_CLI_COMMAND_H
#define ZR_VM_CLI_COMMAND_H

#include <stdio.h>

#include "zr_vm_cli/conf.h"

typedef enum EZrCliMode {
    ZR_CLI_MODE_HELP = 0,
    ZR_CLI_MODE_REPL = 1,
    ZR_CLI_MODE_RUN_PROJECT = 2,
    ZR_CLI_MODE_COMPILE_PROJECT = 3
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
    const TZrChar *debugAddress;
    TZrBool runAfterCompile;
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

void ZrCli_Command_WriteHelp(FILE *stream, const TZrChar *programName);

#endif
