#ifndef ZR_VM_CLI_PROJECT_H
#define ZR_VM_CLI_PROJECT_H

#include "zr_vm_core/global.h"
#include "zr_vm_core/io.h"
#include "zr_vm_cli/conf.h"
#include "zr_vm_library/conf.h"

typedef struct SZrCliProjectContext {
    TZrChar projectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar projectRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sourceRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar binaryRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar entryModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar manifestPath[ZR_LIBRARY_MAX_PATH_LENGTH];
} SZrCliProjectContext;

typedef struct SZrCliStringList {
    TZrChar **items;
    TZrSize count;
    TZrSize capacity;
} SZrCliStringList;

typedef struct SZrCliManifestEntry {
    TZrChar moduleName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sourceHash[ZR_CLI_SOURCE_HASH_HEX_LENGTH];
    TZrChar zroPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar zriPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrCliStringList imports;
} SZrCliManifestEntry;

typedef struct SZrCliIncrementalManifest {
    TZrUInt32 version;
    SZrCliManifestEntry *entries;
    TZrSize count;
    TZrSize capacity;
} SZrCliIncrementalManifest;

SZrGlobalState *ZrCli_Project_CreateBareGlobal(void);
SZrGlobalState *ZrCli_Project_CreateProjectGlobal(const TZrChar *projectPath);
TZrBool ZrCli_Project_RegisterStandardModules(SZrGlobalState *global);
TZrBool ZrCli_ProjectContext_FromGlobal(SZrCliProjectContext *context,
                                        SZrGlobalState *global,
                                        const TZrChar *projectPath);

TZrBool ZrCli_Project_NormalizeModuleName(const TZrChar *modulePath, TZrChar *buffer, TZrSize bufferSize);
TZrBool ZrCli_Project_ResolveSourcePath(const SZrCliProjectContext *context,
                                        const TZrChar *moduleName,
                                        TZrChar *buffer,
                                        TZrSize bufferSize);
TZrBool ZrCli_Project_ResolveBinaryPath(const SZrCliProjectContext *context,
                                        const TZrChar *moduleName,
                                        TZrChar *buffer,
                                        TZrSize bufferSize);
TZrBool ZrCli_Project_ResolveIntermediatePath(const SZrCliProjectContext *context,
                                              const TZrChar *moduleName,
                                              TZrChar *buffer,
                                              TZrSize bufferSize);
TZrBool ZrCli_Project_OpenFileIo(SZrState *state, const TZrChar *path, TZrBool isBinary, SZrIo *io);

TZrBool ZrCli_Project_EnsureParentDirectory(const TZrChar *filePath);
TZrBool ZrCli_Project_RemoveFileIfExists(const TZrChar *filePath);
TZrBool ZrCli_Project_ReadTextFile(const TZrChar *path, TZrChar **outBuffer, TZrSize *outLength);
TZrUInt64 ZrCli_Project_StableHashBytes(const TZrByte *bytes, TZrSize length);
void ZrCli_Project_HashToHex(TZrUInt64 hash, TZrChar *buffer, TZrSize bufferSize);

void ZrCli_Project_StringList_Init(SZrCliStringList *list);
void ZrCli_Project_StringList_Free(SZrCliStringList *list);
TZrBool ZrCli_Project_StringList_AppendUnique(SZrCliStringList *list, const TZrChar *value);
TZrBool ZrCli_Project_StringList_Equals(const SZrCliStringList *left, const SZrCliStringList *right);
TZrBool ZrCli_Project_StringList_Copy(SZrCliStringList *destination, const SZrCliStringList *source);

void ZrCli_Project_Manifest_Init(SZrCliIncrementalManifest *manifest);
void ZrCli_Project_Manifest_Free(SZrCliIncrementalManifest *manifest);
TZrBool ZrCli_Project_LoadManifest(const SZrCliProjectContext *context, SZrCliIncrementalManifest *manifest);
TZrBool ZrCli_Project_SaveManifest(const SZrCliProjectContext *context, const SZrCliIncrementalManifest *manifest);
SZrCliManifestEntry *ZrCli_Project_FindManifestEntry(SZrCliIncrementalManifest *manifest, const TZrChar *moduleName);
const SZrCliManifestEntry *ZrCli_Project_FindManifestEntryConst(const SZrCliIncrementalManifest *manifest,
                                                                const TZrChar *moduleName);

#endif
