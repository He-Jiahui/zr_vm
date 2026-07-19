#include "incremental_change.h"

#include <string.h>

static SZrFileRange incremental_change_range(
        SZrString *uri,
        TZrSize startOffset,
        TZrSize endOffset) {
    return ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(startOffset, 0, 0),
            ZrParser_FilePosition_Create(endOffset, 0, 0),
            uri);
}

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
