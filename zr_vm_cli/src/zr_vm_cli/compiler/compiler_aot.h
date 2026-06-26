#ifndef ZR_VM_CLI_COMPILER_AOT_H
#define ZR_VM_CLI_COMPILER_AOT_H

#include "project/project.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_parser/writer.h"

typedef struct SZrCliAotPreserveRoots {
    TZrUInt32 *indices;
    TZrUInt32 count;
    TZrUInt32 capacity;
    SZrAotManifestGenericRoot *genericRoots;
    TZrUInt32 genericRootCount;
    TZrUInt32 genericRootCapacity;
} SZrCliAotPreserveRoots;

void ZrCli_Compiler_AotPreserveRoots_Init(SZrCliAotPreserveRoots *roots);
void ZrCli_Compiler_AotPreserveRoots_Free(SZrCliAotPreserveRoots *roots);

TZrBool ZrCli_Compiler_ApplyProjectAotPreserveRules(const SZrCliProjectContext *project,
                                                    SZrState *state,
                                                    SZrFunction *function,
                                                    const TZrChar *moduleName,
                                                    SZrAotWriterOptions *options,
                                                    SZrCliAotPreserveRoots *roots);

TZrBool ZrCli_Compiler_WriteAotCFileForModule(const SZrCliProjectContext *project,
                                              SZrState *state,
                                              SZrFunction *function,
                                              const TZrChar *moduleName,
                                              const TZrChar *sourceHash,
                                              const TZrChar *zroHash,
                                              const TZrChar *zroPath,
                                              const TZrChar *aotCPath);

#endif
