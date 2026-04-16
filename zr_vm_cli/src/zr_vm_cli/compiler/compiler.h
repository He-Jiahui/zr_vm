#ifndef ZR_VM_CLI_COMPILER_H
#define ZR_VM_CLI_COMPILER_H

#include "command/command.h"
#include "project/project.h"

typedef struct SZrCliCompileSummary {
    TZrSize compiledCount;
    TZrSize skippedCount;
    TZrSize removedCount;
} SZrCliCompileSummary;

TZrBool ZrCli_Compiler_CompileProjectWithSummary(const SZrCliCommand *command, SZrCliCompileSummary *summary);
TZrBool ZrCli_Compiler_CompileProjectWithSummaryAndBootstrap(const SZrCliCommand *command,
                                                             SZrCliCompileSummary *summary,
                                                             FZrCliProjectGlobalBootstrap bootstrap,
                                                             TZrPtr userData);
int ZrCli_Compiler_CompileProject(const SZrCliCommand *command);

#endif
