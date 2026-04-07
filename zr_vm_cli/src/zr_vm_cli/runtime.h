#ifndef ZR_VM_CLI_RUNTIME_H
#define ZR_VM_CLI_RUNTIME_H

#include "zr_vm_common.h"
#include "zr_vm_cli/command.h"

int ZrCli_Runtime_RunProject(const SZrCliCommand *command);
int ZrCli_Runtime_RunInline(const SZrCliCommand *command);
TZrBool ZrCli_Runtime_InjectProcessArguments(struct SZrState *state,
                                             const TZrChar *entryIdentifier,
                                             const TZrChar *const *programArgs,
                                             TZrSize programArgCount);

#endif
