#ifndef ZR_VM_LANGUAGE_SERVER_LSP_POSITION_CODEC_H
#define ZR_VM_LANGUAGE_SERVER_LSP_POSITION_CODEC_H

#include "zr_vm_language_server/lsp_interface.h"

TZrSize ZrLanguageServer_LspPositionCodec_Utf16PositionToByteOffset(const TZrChar *content,
                                                                    TZrSize contentLength,
                                                                    SZrLspPosition position);
SZrLspPosition ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position(const TZrChar *content,
                                                                           TZrSize contentLength,
                                                                           TZrSize offset);
SZrFilePosition ZrLanguageServer_LspPositionCodec_ByteOffsetToFilePosition(const TZrChar *content,
                                                                           TZrSize contentLength,
                                                                           TZrSize offset);

#endif
