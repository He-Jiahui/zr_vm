#ifndef ZR_VM_CLI_RUNTIME_H
#define ZR_VM_CLI_RUNTIME_H

#include "zr_vm_common.h"
#include "command/command.h"
#include "project/project.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/value.h"

typedef struct SZrCliRunCapture {
    SZrGlobalState *global;
    SZrTypeValue result;
    TZrChar executedVia[32];
} SZrCliRunCapture;

typedef struct SZrCliPreparedProjectRuntime {
    SZrGlobalState *global;
    SZrCliProjectContext project;
    TZrChar effectiveEntryModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar entryIdentifier[ZR_LIBRARY_MAX_PATH_LENGTH];
} SZrCliPreparedProjectRuntime;

int ZrCli_Runtime_RunProject(const SZrCliCommand *command);
int ZrCli_Runtime_RunInline(const SZrCliCommand *command);
TZrBool ZrCli_Runtime_PrepareProjectExecution(const SZrCliCommand *command,
                                             SZrCliPreparedProjectRuntime *outPrepared);
TZrBool ZrCli_Runtime_PrepareProjectExecutionWithBootstrap(const SZrCliCommand *command,
                                                           SZrCliPreparedProjectRuntime *outPrepared,
                                                           FZrCliProjectGlobalBootstrap bootstrap,
                                                           TZrPtr userData);
void ZrCli_Runtime_PreparedProject_Free(SZrCliPreparedProjectRuntime *prepared);
TZrBool ZrCli_Runtime_RunPreparedProjectCapture(SZrCliPreparedProjectRuntime *prepared,
                                                const SZrCliCommand *command,
                                                SZrCliRunCapture *outCapture);
TZrBool ZrCli_Runtime_RunProjectCapture(const SZrCliCommand *command, SZrCliRunCapture *outCapture);
void ZrCli_Runtime_RunCapture_Free(SZrCliRunCapture *capture);
TZrBool ZrCli_Runtime_InjectProcessArguments(struct SZrState *state,
                                             const TZrChar *entryIdentifier,
                                             const TZrChar *const *programArgs,
                                             TZrSize programArgCount);

#endif
