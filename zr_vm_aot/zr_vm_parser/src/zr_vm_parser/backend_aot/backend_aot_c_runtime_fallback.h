#ifndef ZR_VM_PARSER_BACKEND_AOT_C_RUNTIME_FALLBACK_H
#define ZR_VM_PARSER_BACKEND_AOT_C_RUNTIME_FALLBACK_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"
#include "backend_aot_function_table.h"

/** @brief 拒绝保留函数中仍需动态调用、值访问、迭代或反射退路的 full-AOT 输入。 */
TZrBool backend_aot_c_validate_full_aot_runtime_closure(SZrState *state,
                                                        const SZrAotFunctionTable *functionTable,
                                                        const SZrAotExecIrModule *module);
/** @brief 统计未被 reason mask 抑制的运行时退路警告。 */
TZrUInt32 backend_aot_c_count_runtime_fallback_warnings(SZrState *state,
                                                        const SZrAotFunctionTable *functionTable,
                                                        const SZrAotExecIrModule *module,
                                                        TZrUInt32 suppressedReasonMask);
/** @brief 统计被 reason mask 抑制的运行时退路警告。 */
TZrUInt32 backend_aot_c_count_suppressed_runtime_fallback_warnings(SZrState *state,
                                                                   const SZrAotFunctionTable *functionTable,
                                                                   const SZrAotExecIrModule *module,
                                                                   TZrUInt32 suppressedReasonMask);
/** @brief 汇总未抑制警告的原因位，供生成文件摘要使用。 */
TZrUInt32 backend_aot_c_runtime_fallback_warning_reason_mask(SZrState *state,
                                                             const SZrAotFunctionTable *functionTable,
                                                             const SZrAotExecIrModule *module,
                                                             TZrUInt32 suppressedReasonMask);
/** @brief 汇总已抑制警告的原因位，供生成文件摘要使用。 */
TZrUInt32 backend_aot_c_suppressed_runtime_fallback_warning_reason_mask(SZrState *state,
                                                                        const SZrAotFunctionTable *functionTable,
                                                                        const SZrAotExecIrModule *module,
                                                                        TZrUInt32 suppressedReasonMask);
/** @brief 为未抑制退路写出含源范围和原因的逐条裁剪诊断。 */
void backend_aot_write_c_trim_warnings(FILE *file,
                                       SZrState *state,
                                       const SZrAotFunctionTable *functionTable,
                                       const SZrAotExecIrModule *module,
                                       TZrUInt32 suppressedReasonMask);

#endif
