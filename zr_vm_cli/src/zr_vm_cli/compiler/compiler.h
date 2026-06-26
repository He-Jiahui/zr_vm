#ifndef ZR_VM_CLI_COMPILER_H
#define ZR_VM_CLI_COMPILER_H

#include "command/command.h"
#include "project/project.h"
#include "zr_vm_parser/writer.h"

typedef struct SZrCliCompileSummary {
    TZrSize compiledCount;
    TZrSize skippedCount;
    TZrSize removedCount;
    TZrBool packedAssembly;
    TZrChar zrmPath[ZR_LIBRARY_MAX_PATH_LENGTH];
} SZrCliCompileSummary;

TZrBool ZrCli_Compiler_CompileProjectWithSummary(const SZrCliCommand *command, SZrCliCompileSummary *summary);
TZrBool ZrCli_Compiler_CompileProjectWithSummaryAndBootstrap(const SZrCliCommand *command,
                                                             SZrCliCompileSummary *summary,
                                                             FZrCliProjectGlobalBootstrap bootstrap,
                                                             TZrPtr userData);
TZrBool ZrCli_Compiler_ApplyProjectAotWriterOptions(const SZrCliProjectContext *project,
                                                    SZrAotWriterOptions *options);
int ZrCli_Compiler_CompileProject(const SZrCliCommand *command);

#endif
