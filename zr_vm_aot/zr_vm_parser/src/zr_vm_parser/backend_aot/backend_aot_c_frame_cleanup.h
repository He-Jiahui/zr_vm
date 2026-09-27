#ifndef ZR_VM_PARSER_BACKEND_AOT_C_FRAME_CLEANUP_H
#define ZR_VM_PARSER_BACKEND_AOT_C_FRAME_CLEANUP_H

#include <stdio.h>

#include "backend_aot_internal.h"

/** @brief 判断函数是否存在需要 DropInline 的非别名 inline frame 槽。 */
TZrBool backend_aot_c_frame_cleanup_would_emit_for_function(SZrState *state,
                                                            const SZrAotExecIrFunction *functionIr);
/** @brief 为已压入的静态 GC root frame 写出退出时的成对 pop。 */
void backend_aot_write_c_frame_root_cleanup(FILE *file);
/** @brief 逆序写出 inline 槽析构；构造异常路径优先交给运行时按初始化位图展开。 */
void backend_aot_write_c_frame_cleanup(FILE *file,
                                       SZrState *state,
                                       const SZrAotExecIrFunction *functionIr);

#endif
