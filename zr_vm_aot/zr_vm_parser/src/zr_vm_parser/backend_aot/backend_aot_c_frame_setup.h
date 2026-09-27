#ifndef ZR_VM_PARSER_BACKEND_AOT_C_FRAME_SETUP_H
#define ZR_VM_PARSER_BACKEND_AOT_C_FRAME_SETUP_H

#include <stdio.h>

#include "zr_vm_common/zr_common_conf.h"
#include "backend_aot_exec_ir.h"

/** @brief 生成调用帧准备代码；三个 include 标志决定描述符、导出上下文和 GC 根。
 * @note CheckStackAndGc 可能搬迁栈，发射代码会用锚点重新取得基址。
 */
void backend_aot_write_c_frame_setup(FILE *file,
                                     const SZrAotExecIrFrameLayout *frameLayout,
                                     TZrUInt32 functionIndex,
                                     TZrBool includeExportContext,
                                     TZrBool includeFrameDescriptor,
                                     TZrBool includeGcRootFrame);

#endif
