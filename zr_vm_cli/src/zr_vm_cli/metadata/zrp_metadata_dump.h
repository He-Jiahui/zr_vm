#ifndef ZR_VM_CLI_METADATA_ZRP_METADATA_DUMP_H
#define ZR_VM_CLI_METADATA_ZRP_METADATA_DUMP_H

#include <stdio.h>

#include "zr_vm_cli/conf.h"

/** @brief 将当前格式的原始二进制元数据头和各节容量写为稳定的文本摘要。
 * 调用方持有 output 与 buffer；只接受完整且通过核心校验的元数据字节。
 * 失败时可向 errorBuffer 写诊断；写流失败可能留下部分输出。
 */
TZrBool ZrCli_ZrpMetadataDump_WriteSummary(FILE *output,
                                           const TZrByte *buffer,
                                           TZrSize bufferLength,
                                           TZrChar *errorBuffer,
                                           TZrSize errorBufferSize);

/** @brief 比较两份当前格式元数据的节大小与记录数，供 CLI 观察裁剪前后变化。
 * 只报告非负的 bytesRemoved/countRemoved，不比较节载荷；两份输入均由调用方持有。
 * 任一元数据头无效则失败；写流失败可能留下部分输出。
 */
TZrBool ZrCli_ZrpMetadataDump_WriteDiffSummary(FILE *output,
                                               const TZrByte *beforeBuffer,
                                               TZrSize beforeBufferLength,
                                               const TZrByte *afterBuffer,
                                               TZrSize afterBufferLength,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize);

/** @brief 输出磁盘头的实际版本和当前版本期望，供 CLI 诊断兼容性。
 * 至少需要 16 字节前缀；格式不受支持时仍写出 status=unsupported 并返回失败。
 * status=ok 还要求当前格式的完整元数据头通过核心校验。
 */
TZrBool ZrCli_ZrpMetadataDump_WriteVersionCheck(FILE *output,
                                                const TZrByte *buffer,
                                                TZrSize bufferLength,
                                                TZrChar *errorBuffer,
                                                TZrSize errorBufferSize);

/** @brief CLI 摘要模式的文件入口；读取文件后交给 WriteSummary，返回进程退出码。
 * 路径应指向原始二进制元数据，而非项目清单；有效路径下空流参数使用标准流。
 */
int ZrCli_ZrpMetadataDump_RunPath(const TZrChar *path, FILE *output, FILE *errorOutput);

/** @brief CLI 差异模式的文件入口；两份文件均读取成功后才生成比较结果。
 * 输入须为原始二进制元数据；返回 0 表示成功，非零表示读取、校验或写入失败。
 */
int ZrCli_ZrpMetadataDump_RunDiffPath(const TZrChar *beforePath,
                                      const TZrChar *afterPath,
                                      FILE *output,
                                      FILE *errorOutput);

/** @brief CLI 版本检查模式的文件入口；不支持的头仍输出诊断并返回非零退出码。
 * 输入须为原始二进制元数据；有效路径下空流参数使用标准流。
 */
int ZrCli_ZrpMetadataDump_RunVersionCheckPath(const TZrChar *path, FILE *output, FILE *errorOutput);

#endif // ZR_VM_CLI_METADATA_ZRP_METADATA_DUMP_H
