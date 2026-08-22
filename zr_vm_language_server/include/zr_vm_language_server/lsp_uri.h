//
// Canonical LSP URI and native-path boundary helpers.
//

#ifndef ZR_VM_LANGUAGE_SERVER_LSP_URI_H
#define ZR_VM_LANGUAGE_SERVER_LSP_URI_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_core/string.h"

/*
 * Native file access accepts only absolute file: URIs.  Virtual document
 * schemes deliberately stay outside this boundary.
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspUri_FileToNativePath(SZrString *uri,
                                                                         TZrChar *buffer,
                                                                         TZrSize bufferSize);

/* Converts an absolute native path to its canonical UTF-8 file URI. */
ZR_LANGUAGE_SERVER_API SZrString *ZrLanguageServer_LspUri_FromNativePath(SZrState *state,
                                                                           const TZrChar *nativePath);

/* Compares file URIs through normalized native paths; virtual URIs match only byte-for-byte. */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspUri_Equivalent(SZrString *left,
                                                                    SZrString *right);

#endif // ZR_VM_LANGUAGE_SERVER_LSP_URI_H
