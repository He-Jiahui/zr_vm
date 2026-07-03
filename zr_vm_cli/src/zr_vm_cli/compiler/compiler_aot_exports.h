#ifndef ZR_VM_CLI_COMPILER_AOT_EXPORTS_H
#define ZR_VM_CLI_COMPILER_AOT_EXPORTS_H

#include "compiler/compiler_aot.h"

void ZrCli_Compiler_AotExportDeclarations_Init(SZrCliAotPreserveRoots *roots);
void ZrCli_Compiler_AotExportDeclarations_Free(SZrCliAotPreserveRoots *roots);

TZrBool ZrCli_Compiler_ApplyProjectAotExportDeclarations(const SZrCliProjectContext *project,
                                                         const SZrFunction *function,
                                                         SZrAotWriterOptions *options,
                                                         SZrCliAotPreserveRoots *roots);

#endif
