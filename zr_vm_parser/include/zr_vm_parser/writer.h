//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_WRITER_H
#define ZR_VM_PARSER_WRITER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"

// 写入二进制文件 (.zro)
// 将编译后的函数写入二进制格式文件，符合 zr_io_conf.h 中定义的 .SOURCE 格式
ZR_PARSER_API TZrBool ZrParser_Writer_WriteBinaryFile(SZrState *state, SZrFunction *function, const TZrChar *filename);

// 写入明文中间文件 (.zri)
// 将编译后的函数写入可读的明文格式文件
ZR_PARSER_API TZrBool ZrParser_Writer_WriteIntermediateFile(SZrState *state, SZrFunction *function, const TZrChar *filename);

// 写入语法树文件 (.zrs)
// 将解析后的 AST 写入可读的明文格式文件
ZR_PARSER_API TZrBool ZrParser_Writer_WriteSyntaxTreeFile(SZrState *state, SZrAstNode *ast, const TZrChar *filename);

#endif //ZR_VM_PARSER_WRITER_H

