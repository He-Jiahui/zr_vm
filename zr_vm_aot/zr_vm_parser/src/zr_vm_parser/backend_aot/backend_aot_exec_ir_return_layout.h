#ifndef ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_RETURN_LAYOUT_H
#define ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_RETURN_LAYOUT_H

#include "backend_aot_exec_ir.h"

/** @brief 从 RETURN_TYPED 与帧槽证明直接内联返回的类型布局。
 *  @note 无法唯一证明时保留 unknown；无效的类型或布局组合返回失败。 */
TZrBool backend_aot_exec_ir_project_direct_inline_return_layout(
        SZrState *state,
        const SZrFunction *function,
        SZrAotExecIrFunction *outFunction);

#endif
