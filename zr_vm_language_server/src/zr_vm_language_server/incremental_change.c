#include "incremental_change.h"

#include <string.h>

/** @brief 把字节偏移包装为变更区间；精确行列由后续位置映射阶段处理。 */
static SZrFileRange incremental_change_range(
        SZrString *uri,
        TZrSize startOffset,
        TZrSize endOffset) {
    return ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(startOffset, 0, 0),
            ZrParser_FilePosition_Create(endOffset, 0, 0),
            uri);
}

/** @brief 清空上次变更及影响级别，供同内容更新和新文件初始化。 */
void ZrLanguageServer_IncrementalChange_Reset(
        SZrString *uri,
        SZrFileChangeInfo *outChangeInfo) {
    if (outChangeInfo == ZR_NULL) {
        return;
    }

    memset(outChangeInfo, 0, sizeof(*outChangeInfo));
    outChangeInfo->oldRange = incremental_change_range(uri, 0, 0);
    outChangeInfo->newRange = incremental_change_range(uri, 0, 0);
    outChangeInfo->declarationRange = incremental_change_range(uri, 0, 0);
    outChangeInfo->impact = ZR_FILE_CHANGE_IMPACT_NONE;
}

/**
 * @brief 将旧新文本的共同前后缀排除，交给 token 比较及语义分类器继续细分影响。
 * @note 本层仅输出最小字节范围与保守的 MODULE 影响，不推断声明归属。
 */
void ZrLanguageServer_IncrementalChange_Compute(
        SZrString *uri,
        const TZrChar *oldContent,
        TZrSize oldContentLength,
        const TZrChar *newContent,
        TZrSize newContentLength,
        SZrFileChangeInfo *outChangeInfo) {
    TZrSize prefixLength = 0;
    TZrSize suffixLength = 0;

    ZrLanguageServer_IncrementalChange_Reset(uri, outChangeInfo);
    if (oldContent == ZR_NULL || newContent == ZR_NULL || outChangeInfo == ZR_NULL) {
        return;
    }

    while (prefixLength < oldContentLength &&
           prefixLength < newContentLength &&
           oldContent[prefixLength] == newContent[prefixLength]) {
        prefixLength++;
    }
    while (suffixLength < oldContentLength - prefixLength &&
           suffixLength < newContentLength - prefixLength &&
           oldContent[oldContentLength - suffixLength - 1] ==
                   newContent[newContentLength - suffixLength - 1]) {
        suffixLength++;
    }

    outChangeInfo->oldRange = incremental_change_range(
            uri,
            prefixLength,
            oldContentLength - suffixLength);
    outChangeInfo->newRange = incremental_change_range(
            uri,
            prefixLength,
            newContentLength - suffixLength);
    outChangeInfo->impact = ZR_FILE_CHANGE_IMPACT_MODULE;
}
