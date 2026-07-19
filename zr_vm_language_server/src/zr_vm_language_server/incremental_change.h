#ifndef ZR_VM_LANGUAGE_SERVER_INCREMENTAL_CHANGE_H
#define ZR_VM_LANGUAGE_SERVER_INCREMENTAL_CHANGE_H

#include "zr_vm_language_server/incremental_parser.h"

void ZrLanguageServer_IncrementalChange_Reset(
    SZrString *uri,
    SZrFileChangeInfo *outChangeInfo);

void ZrLanguageServer_IncrementalChange_Compute(
    SZrString *uri,
    const TZrChar *oldContent,
    TZrSize oldContentLength,
    const TZrChar *newContent,
    TZrSize newContentLength,
    SZrFileChangeInfo *outChangeInfo);

#endif // ZR_VM_LANGUAGE_SERVER_INCREMENTAL_CHANGE_H
