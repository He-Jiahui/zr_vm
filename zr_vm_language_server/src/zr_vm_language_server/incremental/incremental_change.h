#ifndef ZR_VM_LANGUAGE_SERVER_INCREMENTAL_CHANGE_H
#define ZR_VM_LANGUAGE_SERVER_INCREMENTAL_CHANGE_H

#include "zr_vm_language_server/incremental_parser.h"

/** @brief 将文件变更状态恢复为无影响，供初始化或相同文本更新。 */
void ZrLanguageServer_IncrementalChange_Reset(
    SZrString *uri,
    SZrFileChangeInfo *outChangeInfo);

/** @brief 提供字节级最小变化范围；后续解析与语义层决定是否可局部复用。 */
void ZrLanguageServer_IncrementalChange_Compute(
    SZrString *uri,
    const TZrChar *oldContent,
    TZrSize oldContentLength,
    const TZrChar *newContent,
    TZrSize newContentLength,
    SZrFileChangeInfo *outChangeInfo);

#endif // ZR_VM_LANGUAGE_SERVER_INCREMENTAL_CHANGE_H
