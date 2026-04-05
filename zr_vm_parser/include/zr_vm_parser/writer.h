//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_WRITER_H
#define ZR_VM_PARSER_WRITER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"

typedef struct SZrAotWriterOptions {
    const TZrChar *moduleName;
    const TZrChar *sourceHash;
    const TZrChar *zroHash;
    TZrUInt32 inputKind;
    const TZrChar *inputHash;
    const TZrByte *embeddedModuleBlob;
    TZrSize embeddedModuleBlobLength;
    TZrBool requireExecutableLowering;
} SZrAotWriterOptions;

typedef struct SZrBinaryWriterOptions {
    const TZrChar *moduleName;
    const TZrChar *moduleHash;
} SZrBinaryWriterOptions;

// 写入二进制文件 (.zro)
// 将编译后的函数写入二进制格式文件，符合 zr_io_conf.h 中定义的 .SOURCE 格式
ZR_PARSER_API TZrBool ZrParser_Writer_WriteBinaryFileWithOptions(SZrState *state,
                                                                 SZrFunction *function,
                                                                 const TZrChar *filename,
                                                                 const SZrBinaryWriterOptions *options);
ZR_PARSER_API TZrBool ZrParser_Writer_WriteBinaryFile(SZrState *state, SZrFunction *function, const TZrChar *filename);

// 将 native helper 函数指针映射为稳定的可序列化 helper id
ZR_PARSER_API TZrUInt64 ZrParser_Writer_GetSerializableNativeHelperId(FZrNativeFunction function);

// 写入明文中间文件 (.zri)
// 将编译后的函数写入可读的明文格式文件
ZR_PARSER_API TZrBool ZrParser_Writer_WriteIntermediateFile(SZrState *state, SZrFunction *function, const TZrChar *filename);

// 写入语法树文件 (.zrs)
// 将解析后的 AST 写入可读的明文格式文件
ZR_PARSER_API TZrBool ZrParser_Writer_WriteSyntaxTreeFile(SZrState *state, SZrAstNode *ast, const TZrChar *filename);

// 从 SemIR 降低为 AOT 后端文本工件。
ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFileWithOptions(SZrState *state,
                                                               SZrFunction *function,
                                                               const TZrChar *filename,
                                                               const SZrAotWriterOptions *options);
ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFile(SZrState *state, SZrFunction *function, const TZrChar *filename);
ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFileWithOptions(SZrState *state,
                                                                  SZrFunction *function,
                                                                  const TZrChar *filename,
                                                                  const SZrAotWriterOptions *options);
ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFile(SZrState *state, SZrFunction *function, const TZrChar *filename);

#endif //ZR_VM_PARSER_WRITER_H

