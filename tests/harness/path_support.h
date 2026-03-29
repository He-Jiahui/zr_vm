#ifndef ZR_VM_TESTS_PATH_SUPPORT_H
#define ZR_VM_TESTS_PATH_SUPPORT_H

#include <stddef.h>

#include "zr_vm_common/zr_common_conf.h"

#define ZR_TESTS_PATH_MAX 1024

TZrBool ZrTests_Path_GetFixture(const TZrChar *group,
                                const TZrChar *fileName,
                                TZrChar *outPath,
                                TZrSize maxLen);

TZrBool ZrTests_Path_GetParserFixture(const TZrChar *fileName, TZrChar *outPath, TZrSize maxLen);

TZrBool ZrTests_Path_GetProjectFile(const TZrChar *projectName,
                                    const TZrChar *relativePath,
                                    TZrChar *outPath,
                                    TZrSize maxLen);

TZrBool ZrTests_Path_GetGoldenArtifact(const TZrChar *subDir,
                                       const TZrChar *baseName,
                                       const TZrChar *extension,
                                       TZrChar *outPath,
                                       TZrSize maxLen);

TZrBool ZrTests_Path_GetGeneratedArtifact(const TZrChar *suiteName,
                                          const TZrChar *subDir,
                                          const TZrChar *baseName,
                                          const TZrChar *extension,
                                          TZrChar *outPath,
                                          TZrSize maxLen);

TZrBool ZrTests_Path_EnsureParentDirectory(const TZrChar *filePath);

TZrBool ZrTests_File_Exists(const TZrChar *path);

TZrChar *ZrTests_ReadTextFile(const TZrChar *path, TZrSize *outLength);

TZrBool ZrTests_ReadFileBytes(const TZrChar *path, TZrBytePtr *outBuffer, TZrSize *outLength);

#endif
