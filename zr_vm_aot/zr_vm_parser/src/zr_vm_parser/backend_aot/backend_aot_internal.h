#ifndef ZR_VM_PARSER_BACKEND_AOT_INTERNAL_H
#define ZR_VM_PARSER_BACKEND_AOT_INTERNAL_H

#include "backend_aot_callable_provenance.h"
#include "backend_aot_exec_ir.h"
#include "backend_aot_function_table.h"
#include "zr_vm_parser/writer.h"

#define ZR_AOT_COUNT_NONE 0U
#define ZR_AOT_FUNCTION_TREE_ROOT_INDEX 0U
#define ZR_AOT_INVALID_FUNCTION_INDEX ((TZrUInt32)-1)
#define ZR_AOT_LLVM_RESUME_FALLTHROUGH ((TZrUInt32)0xFFFFFFFFu)

/** @brief C emitter 对执行指令可能产生的异常和控制流效果分类。 */
typedef enum EZrAotEmitterStepFlag {
    ZR_AOT_EMITTER_STEP_FLAG_NONE = 0,
    ZR_AOT_EMITTER_STEP_FLAG_MAY_THROW = 1u << 0,
    ZR_AOT_EMITTER_STEP_FLAG_CONTROL_FLOW = 1u << 1,
    ZR_AOT_EMITTER_STEP_FLAG_CALL = 1u << 2,
    ZR_AOT_EMITTER_STEP_FLAG_RETURN = 1u << 3
} EZrAotEmitterStepFlag;

/** @brief 将模块指令投影写成供诊断的清单。 */
void backend_aot_write_instruction_listing(FILE *file,
                                           const TZrChar *prefix,
                                           const SZrAotExecIrModule *module);
/** @brief 在 writer 选项、候选值与兜底文本之间选择有效字符串。 */
const TZrChar *backend_aot_option_text(const SZrAotWriterOptions *options,
                                       const TZrChar *candidate,
                                       const TZrChar *fallback);
/** @brief 检查函数指令是否落在现有 AOT emitter 支持的执行子集。 */
TZrBool backend_aot_function_is_executable_subset(const SZrFunction *function);
/** @brief 报告函数表中首个不能由目标 emitter 处理的指令。 */
TZrBool backend_aot_report_first_unsupported_instruction(const TZrChar *backendName,
                                                         const TZrChar *moduleName,
                                                         const SZrAotFunctionTable *table);
/** @brief 读取 writer 输入种类供产物元数据记录。 */
TZrUInt32 backend_aot_option_input_kind(const SZrAotWriterOptions *options);
/** @brief 依据输入种类选择源码或 ZRO 哈希文本。 */
const TZrChar *backend_aot_option_input_hash(const SZrAotWriterOptions *options,
                                             const TZrChar *sourceHash,
                                             const TZrChar *zroHash);
/** @brief 查询是否要求完全 AOT，禁止运行时回退。 */
TZrBool backend_aot_option_require_full_aot(const SZrAotWriterOptions *options);
/** @brief 查询生成产物时是否按可达性移除函数。 */
TZrBool backend_aot_option_enable_code_stripping(const SZrAotWriterOptions *options);
/** @brief 查询是否在产物中隐藏生成符号。 */
TZrBool backend_aot_option_strip_generated_symbols(const SZrAotWriterOptions *options);
/** @brief 查询反射元数据保留级别。 */
TZrUInt8 backend_aot_option_reflection_metadata_level(const SZrAotWriterOptions *options);
/** @brief 查询是否抑制运行时回退告警。 */
TZrBool backend_aot_option_suppress_runtime_fallback_warnings(const SZrAotWriterOptions *options);
/** @brief 查询按种类抑制运行时回退告警的位掩码。 */
TZrUInt32 backend_aot_option_runtime_fallback_warning_suppression_mask(const SZrAotWriterOptions *options);
/** @brief 查询是否抑制类型/所有权注解告警。 */
TZrBool backend_aot_option_suppress_annotation_warnings(const SZrAotWriterOptions *options);
/** @brief 提取 C emitter 对一条指令的异常、调用及控制流效果位。 */
ZR_PARSER_API TZrUInt32 backend_aot_c_step_flags_for_instruction(const SZrFunction *function,
                                                                 const TZrInstruction *instruction);
/** @brief 从预热缓存查目标调用点的参数数量。 */
TZrUInt32 backend_aot_get_callsite_cache_argument_count(const SZrFunction *function,
                                                        TZrUInt32 cacheIndex,
                                                        EZrFunctionCallSiteCacheKind expectedKind);

#endif
