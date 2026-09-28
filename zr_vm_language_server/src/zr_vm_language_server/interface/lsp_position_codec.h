#ifndef ZR_VM_LANGUAGE_SERVER_LSP_POSITION_CODEC_H
#define ZR_VM_LANGUAGE_SERVER_LSP_POSITION_CODEC_H

#include "zr_vm_language_server/lsp_interface.h"

/** 客户端的零基 UTF-16 位置转换为当前 UTF-8 快照偏移；越界位置收敛到可用边界。 */
TZrSize ZrLanguageServer_LspPositionCodec_Utf16PositionToByteOffset(const TZrChar *content,
                                                                    TZrSize contentLength,
                                                                    SZrLspPosition position);
/** 解析器字节偏移投影回 LSP UTF-16 位置；结果用于诊断、导航与语义 token。 */
SZrLspPosition ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position(const TZrChar *content,
                                                                           TZrSize contentLength,
                                                                           TZrSize offset);
/** 为解析器查询生成一基行及字节列；输入偏移须对应同一内容快照。 */
SZrFilePosition ZrLanguageServer_LspPositionCodec_ByteOffsetToFilePosition(const TZrChar *content,
                                                                           TZrSize contentLength,
                                                                           TZrSize offset);

#endif
