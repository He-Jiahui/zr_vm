#ifndef ZR_VM_LANGUAGE_SERVER_INCREMENTAL_SYNTAX_REPARSE_H
#define ZR_VM_LANGUAGE_SERVER_INCREMENTAL_SYNTAX_REPARSE_H

#include "zr_vm_language_server/incremental_parser.h"

TZrBool ZrLanguageServer_IncrementalSyntaxReparse_TryDeclaration(
        SZrState *state,
        SZrString *uri,
        const TZrChar *oldContent,
        TZrSize oldContentLength,
        const TZrChar *newContent,
        TZrSize newContentLength,
        SZrAstNode *root,
        SZrFileChangeInfo *changeInfo);

#endif // ZR_VM_LANGUAGE_SERVER_INCREMENTAL_SYNTAX_REPARSE_H
