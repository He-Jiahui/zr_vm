#ifndef ZR_VM_LANGUAGE_SERVER_LSP_WORKSPACE_H
#define ZR_VM_LANGUAGE_SERVER_LSP_WORKSPACE_H

#include "zr_vm_language_server/lsp_interface.h"

typedef struct SZrLspWorkspace SZrLspWorkspace;

ZR_LANGUAGE_SERVER_API SZrLspWorkspace *ZrLanguageServer_LspWorkspace_New(SZrState *state);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspWorkspace_Free(SZrState *state, SZrLspWorkspace *workspace);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspWorkspace_Reset(SZrState *state, SZrLspContext *context);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspWorkspace_AddFolder(SZrState *state,
                                                                        SZrLspContext *context,
                                                                        SZrString *uri);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspWorkspace_RemoveFolder(SZrState *state,
                                                                           SZrLspContext *context,
                                                                           SZrString *uri);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspWorkspace_CanProcessFileEvent(SZrLspContext *context,
                                                                                  SZrString *uri);

#endif
