#ifndef ZR_VM_TESTS_PATH_SUPPORT_H
#define ZR_VM_TESTS_PATH_SUPPORT_H

#include <stddef.h>

#include "zr_vm_common/zr_common_conf.h"

#define ZR_TESTS_PATH_MAX 1024

/** @brief 在测试源码树定位 group 下的 fixture，写入调用方提供的路径缓冲区。 */
TZrBool ZrTests_Path_GetFixture(const TZrChar *group,
                                const TZrChar *fileName,
                                TZrChar *outPath,
                                TZrSize maxLen);

/** @brief 定位 parser fixture；路径过长或参数无效时返回 false。 */
TZrBool ZrTests_Path_GetParserFixture(const TZrChar *fileName, TZrChar *outPath, TZrSize maxLen);

/** @brief 定位测试项目内的相对文件，不创建文件或目录。 */
TZrBool ZrTests_Path_GetProjectFile(const TZrChar *projectName,
                                    const TZrChar *relativePath,
                                    TZrChar *outPath,
                                    TZrSize maxLen);

/** @brief 从测试源码树定位仓库文档，供引用契约测试读取。 */
TZrBool ZrTests_Path_GetRepoDoc(const TZrChar *relativePath, TZrChar *outPath, TZrSize maxLen);

/** @brief 定位只读 golden 工件，不创建目录。 */
TZrBool ZrTests_Path_GetGoldenArtifact(const TZrChar *subDir,
                                       const TZrChar *baseName,
                                       const TZrChar *extension,
                                       TZrChar *outPath,
                                       TZrSize maxLen);

/** @brief 定位生成工件并创建父目录；返回 true 表示路径和父目录均已准备。 */
TZrBool ZrTests_Path_GetGeneratedArtifact(const TZrChar *suiteName,
                                          const TZrChar *subDir,
                                          const TZrChar *baseName,
                                          const TZrChar *extension,
                                          TZrChar *outPath,
                                          TZrSize maxLen);

/** @brief 创建 filePath 的各级父目录；无父目录时视为成功。 */
TZrBool ZrTests_Path_EnsureParentDirectory(const TZrChar *filePath);

/** @brief 尝试以只读方式打开路径；结果只表示当前进程可打开。 */
TZrBool ZrTests_File_Exists(const TZrChar *path);

/** @brief 读入以 NUL 结尾的文本；成功时调用方用 free 释放返回缓冲区。 */
TZrChar *ZrTests_ReadTextFile(const TZrChar *path, TZrSize *outLength);

/** @brief 读入文件字节并额外写入 NUL；成功时调用方用 free 释放缓冲区。 */
TZrBool ZrTests_ReadFileBytes(const TZrChar *path, TZrBytePtr *outBuffer, TZrSize *outLength);

#endif
