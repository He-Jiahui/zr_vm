#ifndef ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_FRAME_H
#define ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_FRAME_H

#include "backend_aot_exec_ir.h"

/** @brief 校验函数帧 ABI，并复制参数及槽布局供旧 AOT emitter 使用。
 *  @return 成功时布局含调用方持有的数组；失败时不得消费布局。 */
TZrBool backend_aot_exec_ir_build_frame_layout(
        SZrState *state,
        const SZrFunction *function,
        SZrAotExecIrFrameLayout *outFrameLayout);
/** @brief 释放 build_frame_layout 分配的数组并清空布局。 */
void backend_aot_exec_ir_release_frame_layout(
        SZrState *state,
        SZrAotExecIrFrameLayout *frameLayout);

#endif
