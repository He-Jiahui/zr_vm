#ifndef ZR_VM_LIB_DEBUG_EVAL_INTERNAL_H
#define ZR_VM_LIB_DEBUG_EVAL_INTERNAL_H

#include "debug_internal.h"

/** 兼容表达式解析器的单次调用状态；cursor 指向调用方持有的表达式，缓冲区也由调用方持有。
 * skip_evaluation 仅跳过运行时取值，短路分支仍需完整解析并检查被禁止的语法。
 */
typedef struct ZrDebugEvalParser {
    ZrDebugAgent *agent;
    TZrUInt32 frame_id;
    const TZrChar *cursor;
    TZrChar *error_buffer;
    TZrSize error_buffer_size;
    TZrChar *reference_buffer;
    TZrSize reference_buffer_size;
    TZrBool skip_evaluation;
} ZrDebugEvalParser;

/** 运算符右侧的递归下降入口，由诊断包装器在失败时补足错误信息。 */
typedef TZrBool (*FZrDebugEvalParseValue)(ZrDebugEvalParser *parser, SZrTypeValue *outValue);

void zr_debug_eval_set_error(ZrDebugEvalParser *parser, const TZrChar *message);
const TZrChar *zr_debug_eval_value_type_name(const SZrTypeValue *value);
void zr_debug_eval_set_numeric_operand_error(ZrDebugEvalParser *parser,
                                             const TZrChar *op,
                                             const SZrTypeValue *left,
                                             const SZrTypeValue *right);
void zr_debug_eval_set_division_by_zero_error(ZrDebugEvalParser *parser, const TZrChar *op);
void zr_debug_eval_set_missing_member_name_error(ZrDebugEvalParser *parser);
void zr_debug_eval_set_missing_index_close_error(ZrDebugEvalParser *parser);
void zr_debug_eval_set_missing_group_close_error(ZrDebugEvalParser *parser);
void zr_debug_eval_set_missing_conditional_separator_error(ZrDebugEvalParser *parser);
void zr_debug_eval_set_missing_conditional_consequent_error(ZrDebugEvalParser *parser);
void zr_debug_eval_set_missing_conditional_alternate_error(ZrDebugEvalParser *parser);
void zr_debug_eval_set_unterminated_string_error(ZrDebugEvalParser *parser);
void zr_debug_eval_set_unsupported_string_escape_error(ZrDebugEvalParser *parser, TZrChar escapeChar);
void zr_debug_eval_set_invalid_numeric_literal_error(ZrDebugEvalParser *parser);
void zr_debug_eval_set_function_call_error(ZrDebugEvalParser *parser);
void zr_debug_eval_set_assignment_error(ZrDebugEvalParser *parser);
TZrBool zr_debug_eval_cursor_starts_assignment(const ZrDebugEvalParser *parser);
void zr_debug_eval_set_modulo_operand_error(ZrDebugEvalParser *parser,
                                            const SZrTypeValue *left,
                                            const SZrTypeValue *right,
                                            TZrBool leftIsInteger,
                                            TZrBool rightIsInteger,
                                            TZrBool rightIsZero);
void zr_debug_eval_refine_right_operand_error(ZrDebugEvalParser *parser, const TZrChar *op);
TZrBool zr_debug_eval_parse_right_operand(ZrDebugEvalParser *parser,
                                          const TZrChar *op,
                                          FZrDebugEvalParseValue parseValue,
                                          SZrTypeValue *outValue);
TZrBool zr_debug_eval_parse_right_operand_with_skip(ZrDebugEvalParser *parser,
                                                    const TZrChar *op,
                                                    FZrDebugEvalParseValue parseValue,
                                                    SZrTypeValue *outValue,
                                                    TZrBool skipEvaluation);

#endif

/**
 * @struct ZrDebugEvalParser
 *
 * ## Summary
 * 兼容表达式求值的一次同步调用状态。正式求值未处理且入口允许兼容回退时，由求值入口在栈上构造；不是跨暂停事件保留的快照。
 *
 * ## Fields
 * - agent：本次借用的调试代理；求值期间代理及其 VM 必须有效；本结构不增加 GC root。
 * - frame_id：向上层暂停帧解析传递的编号；此层不独立验证暂停状态或帧世代。
 * - cursor：调用方持有的 NUL 结尾表达式当前位置；解析会推进，不拥有或改写表达式文本。
 * - error_buffer：调用方错误输出区；允许 NULL，文字写入受 error_buffer_size 约束。
 * - error_buffer_size：错误输出区字节容量；零容量 setter 不写入，但部分失败读取未核容量或 NUL，见 DED-BUG-001；不能将截断文字当完整错误类别。
 * - reference_buffer：调用方引用展示区；兼容取值过程可写入，不属于错误区。
 * - reference_buffer_size：引用展示区字节容量；与 reference_buffer 配对，不能从错误区容量推断。
 * - skip_evaluation：兼容递归解析的跳过取值标志；嵌套调用保留已有 TRUE，普通返回才恢复临时设置。
 *
 * ## Value model
 * 入口先清零再填入借用指针、帧号和两组容量；不包含拥有型资源，也不负责销毁代理、VM、输入或输出缓冲区。
 *
 * ## Invariants and lifecycle
 * 同一状态只供当前同步递归链独占修改，本类型没有锁或线程切换协议。调用方保持所有借用对象有效并在更上层校验暂停/能力。skip 仍解析语法、拒绝兼容调用和赋值，字符串字面量仍可能分配；不承诺求值无副作用或事务回滚。FALSE 可留下推进的 cursor 和部分 outValue。临时 skip 仅在普通 C 返回路径恢复，非局部退出不能依赖该恢复。
 * BUG: DED-BUG-001：上游允许非空错误指针配零容量且不清零；失败路径仍读取该区并按 NUL 文本比较。有效暂停状态下，以 1 +、char error[1]={0x65} 和容量 0 进入兼容回退，strcmp 首字节匹配后可读过实际数组；这是完整静态路径结论，未经运行复现，当前未修复。
 *
 * ## References
 * - zr_debug_evaluate_expression_with_capabilities 构造本次状态 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:1016
 * - zr_debug_eval_parse_right_operand_with_skip 临时置位 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:311
 * - zr_debug_eval_parse_string 在 skip 下仍可能分配字符串 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:341
 * - zr_debug_evaluate_with_policy 暂停状态门禁 — zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2504
 */

/**
 * @typedef FZrDebugEvalParseValue
 *
 * ## Summary
 * 兼容优先级层为运算符右侧选择的同步递归下降接口。诊断包装器只在本次调用内派发，不注册、保存或异步调用该指针。
 *
 * ## Parameters
 * @param[in,out] parser 借用当前递归解析状态；实际仓内回调要求有效状态，可推进 cursor 并设置错误或引用展示区。
 * @param[out] outValue 可写的借用值输出；FALSE 也可能已经部分写入，不得当成功值使用。
 *
 * ## Returns
 * @return TRUE 表示本次子表达式解析成功，FALSE 由包装器细化适用错误。返回不转移值对象所有权或增加 GC root。
 *
 * ## Constraints and lifecycle
 * 11 个绑定位置选择 7 个真实解析函数。非空指针的参数前提由回调承担；NULL 由包装器作为失败处理而不派发。回调可能分配字符串或非局部退出，包装器不捕获 longjmp，也不恢复输入位置或部分值。代理、VM、表达式与缓冲区必须覆盖整个同步调用。
 *
 * ## References
 * - zr_debug_eval_parse_right_operand 唯一间接派发点 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:290
 * - zr_debug_eval_parse_multiplicative 绑定 unary — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:653
 * - zr_debug_eval_parse_logical_and 绑定 equality — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:831
 * - zr_debug_eval_parse_expression 绑定 expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:897
 */

/**
 * @fn void zr_debug_eval_set_error(ZrDebugEvalParser *parser, const TZrChar *message)
 *
 * ## Summary
 * 把兼容解析器选定的诊断复制到调用方错误区，作为语法或取值失败的文字结果。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 * @param[in] message 本次复制期间有效的 NUL 结尾文字；NULL 使用通用 debug evaluate error。实际调用来自字面量或当前栈缓冲区。
 *
 * ## Returns
 * 无返回值；容量为零或目标为空时不写入，有可写容量时交付有界、NUL 结尾的诊断，可能带截断标记。
 *
 * ## Constraints and lifecycle
 * 不保存 message，不取得缓冲区所有权，也不改变 cursor、值输出或 VM 状态；不承诺重叠源/目标的复制。
 *
 * ## References
 * - zr_debug_eval_set_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:5
 * - zr_debug_eval_set_numeric_operand_error — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:42
 * - zr_debug_eval_parse_primary — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:409
 * - zr_debug_copy_text — zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:34
 */

/**
 * @fn const TZrChar *zr_debug_eval_value_type_name(const SZrTypeValue *value)
 *
 * ## Summary
 * 为诊断取得值标签的稳定名称，不执行值转换或读取对象负载。
 *
 * ## Parameters
 * @param[in] value 只读值描述；NULL 或未识别类型使用通用 value 名称。
 *
 * ## Returns
 * @return 返回静态字符串字面量；调用方不得释放或改写，不依赖被描述对象的存活期。
 *
 * ## Constraints and lifecycle
 * 只读取 type 标签；不会给原值添加 GC root、转移所有权或校验对象布局。
 *
 * ## References
 * - zr_debug_eval_value_type_name 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:14
 * - zr_debug_eval_set_numeric_operand_error — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:38
 * - zr_debug_eval_parse_unary — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:574
 * - zr_debug_value_type_name — zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:188
 */

/**
 * @fn void zr_debug_eval_set_numeric_operand_error(ZrDebugEvalParser *parser, const TZrChar *op, const SZrTypeValue *left, const SZrTypeValue *right)
 *
 * ## Summary
 * 数值二元运算拒绝操作数时补充运算符、两侧类型、原因与替代写法。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 * @param[in] op NUL 结尾的运算符文字，仅供格式化；NULL 显示 operator。
 * @param[in] left 左值的只读类型描述，可为 NULL；不读取数值负载。
 * @param[in] right 右值的只读类型描述，可为 NULL；不读取数值负载。
 *
 * ## Returns
 * 无返回值；只通过 parser 的有界错误区交付诊断，不交付运算结果。
 *
 * ## Constraints and lifecycle
 * 调用者已发现数值转换失败；本函数不重试转换、不计算运算符，也不修改两个操作数。栈上消息在本次复制后失效。
 *
 * ## References
 * - zr_debug_eval_set_numeric_operand_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:22
 * - zr_debug_eval_apply_numeric_binary — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:598
 * - test_debug_evaluate_reports_numeric_operand_type_error_with_cause_and_suggestion — tests/debug/test_debug_expression_diagnostics.c:682
 */

/**
 * @fn void zr_debug_eval_set_division_by_zero_error(ZrDebugEvalParser *parser, const TZrChar *op)
 *
 * ## Summary
 * 兼容除法已判定右侧为零后，生成包含原因与除数保护建议的诊断。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 * @param[in] op 用于展示的 NUL 结尾运算符；NULL 使用 /，不参与再次判零。
 *
 * ## Returns
 * 无返回值；有界地覆盖错误文字，不修改除数或任何结果值。
 *
 * ## Constraints and lifecycle
 * 零值判断属于调用方；本函数只格式化消息。临时消息不逃逸。
 *
 * ## References
 * - zr_debug_eval_set_division_by_zero_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:45
 * - zr_debug_eval_apply_numeric_binary — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:610
 * - test_debug_evaluate_reports_division_by_zero_with_cause_and_suggestion — tests/debug/test_debug_expression_diagnostics.c:705
 */

/**
 * @fn void zr_debug_eval_set_missing_member_name_error(ZrDebugEvalParser *parser)
 *
 * ## Summary
 * 后缀解析消费点号后未取得标识符时，说明缺失的成员名及补全方式。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 *
 * ## Returns
 * 无返回值；将成员名缺失文字交付到错误区。
 *
 * ## Constraints and lifecycle
 * 不推进或修复 cursor，不执行成员访问；调用者据此返回解析失败。
 *
 * ## References
 * - zr_debug_eval_set_missing_member_name_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:62
 * - zr_debug_eval_parse_postfix — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:478
 * - test_debug_evaluate_reports_missing_member_name_with_cause_and_suggestion — tests/debug/test_debug_expression_diagnostics.c:752
 */

/**
 * @fn void zr_debug_eval_set_missing_index_close_error(ZrDebugEvalParser *parser)
 *
 * ## Summary
 * 索引表达式解析完成后未匹配右方括号时，提供结构错误诊断。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 *
 * ## Returns
 * 无返回值；将缺少右方括号的文字交付到错误区。
 *
 * ## Constraints and lifecycle
 * 不补插括号，不撤销已解析的索引或其输出；调用者终止当前表达式。
 *
 * ## References
 * - zr_debug_eval_set_missing_index_close_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:73
 * - zr_debug_eval_parse_postfix — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:511
 * - test_debug_condition_reports_missing_index_close_with_cause_and_suggestion — tests/debug/test_debug_expression_diagnostics.c:775
 */

/**
 * @fn void zr_debug_eval_set_missing_group_close_error(ZrDebugEvalParser *parser)
 *
 * ## Summary
 * 分组内部已解析但右圆括号未匹配时，说明闭合需求。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 *
 * ## Returns
 * 无返回值；将分组未闭合的文字交付到错误区。
 *
 * ## Constraints and lifecycle
 * 不回退分组解析已经推进的 cursor 或改写表达式源。
 *
 * ## References
 * - zr_debug_eval_set_missing_group_close_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:85
 * - zr_debug_eval_parse_primary — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:397
 * - test_debug_condition_reports_missing_group_close_with_cause_and_suggestion — tests/debug/test_debug_expression_diagnostics.c:798
 */

/**
 * @fn void zr_debug_eval_set_missing_conditional_separator_error(ZrDebugEvalParser *parser)
 *
 * ## Summary
 * 兼容条件表达式在真分支之后未匹配冒号时，给出缺失分隔符诊断。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 *
 * ## Returns
 * 无返回值；将缺少冒号的文字交付到错误区。
 *
 * ## Constraints and lifecycle
 * 只说明语法失败，不创建假分支或撤销真分支解析；实际条件断点当前禁用兼容回退。
 *
 * ## References
 * - zr_debug_eval_set_missing_conditional_separator_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:97
 * - zr_debug_eval_parse_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:905
 */

/**
 * @fn void zr_debug_eval_set_missing_conditional_consequent_error(ZrDebugEvalParser *parser)
 *
 * ## Summary
 * 问号后紧接分隔符或表达式边界时，指出缺少真分支表达式。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 *
 * ## Returns
 * 无返回值；将缺少真分支的文字交付到错误区。
 *
 * ## Constraints and lifecycle
 * 边界识别由调用方完成；本函数不消费输入或选择运行分支。
 *
 * ## References
 * - zr_debug_eval_set_missing_conditional_consequent_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:109
 * - zr_debug_eval_parse_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:894
 * - test_debug_condition_reports_missing_ternary_consequent_with_cause_and_suggestion — tests/debug/test_debug_expression_diagnostics.c:636
 */

/**
 * @fn void zr_debug_eval_set_missing_conditional_alternate_error(ZrDebugEvalParser *parser)
 *
 * ## Summary
 * 冒号后紧接表达式边界时，指出缺少假分支表达式。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 *
 * ## Returns
 * 无返回值；将缺少假分支的文字交付到错误区。
 *
 * ## Constraints and lifecycle
 * 不补默认值，不改变真值或回滚已解析的真分支。
 *
 * ## References
 * - zr_debug_eval_set_missing_conditional_alternate_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:121
 * - zr_debug_eval_parse_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:911
 * - test_debug_condition_reports_missing_ternary_alternate_with_cause_and_suggestion — tests/debug/test_debug_expression_diagnostics.c:659
 */

/**
 * @fn void zr_debug_eval_set_unterminated_string_error(ZrDebugEvalParser *parser)
 *
 * ## Summary
 * 兼容字符串扫描到结尾而未找到闭引号时，说明未完成字符串。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 *
 * ## Returns
 * 无返回值；将缺少闭引号的文字交付到错误区。
 *
 * ## Constraints and lifecycle
 * 不分配字符串或改写输入；扫描和失败退出由调用方负责。
 *
 * ## References
 * - zr_debug_eval_set_unterminated_string_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:133
 * - zr_debug_eval_parse_string — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:335
 * - test_debug_evaluate_reports_unterminated_string_with_cause_and_suggestion — tests/debug/test_debug_expression_diagnostics.c:821
 */

/**
 * @fn void zr_debug_eval_set_unsupported_string_escape_error(ZrDebugEvalParser *parser, TZrChar escapeChar)
 *
 * ## Summary
 * 兼容字符串读取到不受支持的反斜线转义时，保留该字符并给出受支持形式的提示。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 * @param[in] escapeChar 刚读取的转义字符；NUL 仅为展示改成问号，不改写原表达式。
 *
 * ## Returns
 * 无返回值；在有界错误文字中展示转义，临时格式化区不逃逸。
 *
 * ## Constraints and lifecycle
 * 不重新扫描或接受该转义；调用方仍返回失败，cursor 不由本函数回退。
 *
 * ## References
 * - zr_debug_eval_set_unsupported_string_escape_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:145
 * - zr_debug_eval_parse_string — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:323
 * - test_debug_evaluate_reports_unsupported_string_escape_with_cause_and_suggestion — tests/debug/test_debug_expression_diagnostics.c:845
 */

/**
 * @fn void zr_debug_eval_set_invalid_numeric_literal_error(ZrDebugEvalParser *parser)
 *
 * ## Summary
 * 兼容十进制扫描遇到第二个小数点时，给出可操作的字面量诊断。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 *
 * ## Returns
 * 无返回值；将多小数点的原因与更正建议写入错误区。
 *
 * ## Constraints and lifecycle
 * 只解释当前扫描失败，不进行通用数值语法验证、溢出检测或输入修复。
 *
 * ## References
 * - zr_debug_eval_set_invalid_numeric_literal_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:161
 * - zr_debug_eval_parse_number — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:367
 * - test_debug_evaluate_reports_invalid_numeric_literal_with_cause_and_suggestion — tests/debug/test_debug_expression_diagnostics.c:728
 */

/**
 * @fn void zr_debug_eval_set_function_call_error(ZrDebugEvalParser *parser)
 *
 * ## Summary
 * 兼容主项或后缀发现调用括号时，说明该旧求值子集不执行用户函数调用。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 *
 * ## Returns
 * 无返回值；写入调用被拒绝的诊断，具体 FALSE 由解析调用方返回。
 *
 * ## Constraints and lifecycle
 * 本函数没有调用派发能力；此限制属于兼容解析器，不能推成正式能力化求值器禁止所有已授权调用。
 *
 * ## References
 * - zr_debug_eval_set_function_call_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:172
 * - zr_debug_eval_parse_primary — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:434
 * - zr_debug_eval_parse_postfix — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:533
 */

/**
 * @fn void zr_debug_eval_set_assignment_error(ZrDebugEvalParser *parser)
 *
 * ## Summary
 * 兼容路径识别单等号写入意图时，说明旧只读求值子集拒绝赋值。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 *
 * ## Returns
 * 无返回值；将赋值被拒绝的文字交付到错误区。
 *
 * ## Constraints and lifecycle
 * 不执行写入、不恢复先前解析输出；正式求值器也有独立的 mutation gate，不能把两条路径混为一次调用。
 *
 * ## References
 * - zr_debug_eval_set_assignment_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:184
 * - zr_debug_eval_parse_primary — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:416
 * - zr_debug_evaluate_expression_with_capabilities — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:1040
 * - test_debug_evaluate_rejects_assignment_with_cause_and_suggestion — tests/debug/test_debug_evaluation_effect_policy_cases.h:35
 */

/**
 * @fn TZrBool zr_debug_eval_cursor_starts_assignment(const ZrDebugEvalParser *parser)
 *
 * ## Summary
 * 在不移动兼容解析光标的前提下，区分后续单等号与双等号，供只读语法保护使用。
 *
 * ## Parameters
 * @param[in] parser 只读解析状态；NULL 或 cursor 为 NULL 时返回 FALSE。非空 cursor 指向有效 NUL 结尾输入。
 *
 * ## Returns
 * @return 返回 TRUE 表示跳过空格、制表、CR、LF 后是单个等号起点；双等号返回 FALSE。
 *
 * ## Constraints and lifecycle
 * 仅检查等号形态，不解析完整赋值表达式或匹配其他复合赋值运算符；原 parser->cursor 保持不变。
 *
 * ## References
 * - zr_debug_eval_cursor_starts_assignment 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:196
 * - zr_debug_eval_parse_primary — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:415
 * - zr_debug_evaluate_expression_with_capabilities — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:1039
 */

/**
 * @fn void zr_debug_eval_set_modulo_operand_error(ZrDebugEvalParser *parser, const SZrTypeValue *left, const SZrTypeValue *right, TZrBool leftIsInteger, TZrBool rightIsInteger, TZrBool rightIsZero)
 *
 * ## Summary
 * 兼容取模发现整数前提不成立或右除数为零时，结合调用方标志解释失败。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 * @param[in] left 左操作数的只读类型描述，可为 NULL。
 * @param[in] right 右操作数的只读类型描述，可为 NULL。
 * @param[in] leftIsInteger 调用方数值转换得到的左侧整数标志，不由本函数重新计算。
 * @param[in] rightIsInteger 调用方数值转换得到的右侧整数标志。
 * @param[in] rightIsZero 调用方已转换右侧整数是否为零的标志。
 *
 * ## Returns
 * 无返回值；只写类型、整数标志和右侧零值说明，不交付余数。
 *
 * ## Constraints and lifecycle
 * 左侧零值本身不被本函数验证或禁止；调用方校验分支负责决定是否调用。两个操作数和标志只借用，不改变。
 *
 * ## References
 * - zr_debug_eval_set_modulo_operand_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:211
 * - zr_debug_eval_apply_numeric_binary — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:618
 */

/**
 * @fn void zr_debug_eval_refine_right_operand_error(ZrDebugEvalParser *parser, const TZrChar *op)
 *
 * ## Summary
 * 递归右侧解析失败后，仅将空错误或完整通用错误细化为缺失或非法起点提示。
 *
 * ## Parameters
 * @param[in,out] parser 本次兼容解析状态；可为 NULL，此时不写诊断。非空时只借用其中的错误输出区及容量。
 * @param[in] op 已消费的运算符展示文字；NULL 使用 operator。
 *
 * ## Returns
 * 无返回值；已有非空且非完整通用文字保持原样，包括已截断文字；其他情形写入细化诊断。
 *
 * ## Constraints and lifecycle
 * 只细化错误，不撤销 callback 推进的 cursor 或部分值输出；分类使用当前失败位置。NULL 错误目标不能交付文字。
 * BUG: DED-BUG-001：上游允许非空错误指针配零容量且不清零；失败路径仍读取该区并按 NUL 文本比较。有效暂停状态下，以 1 +、char error[1]={0x65} 和容量 0 进入兼容回退，strcmp 首字节匹配后可读过实际数组；这是完整静态路径结论，未经运行复现，当前未修复。
 *
 * ## References
 * - zr_debug_eval_refine_right_operand_error 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:257
 * - zr_debug_eval_parse_right_operand — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:294
 * - test_debug_evaluate_reports_missing_right_operand_with_cause_and_suggestion — tests/debug/test_debug_expression_diagnostics.c:67
 * - 容量零的原错误指针仍传入兼容 parser — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:1020
 */

/**
 * @fn TZrBool zr_debug_eval_parse_right_operand(ZrDebugEvalParser *parser, const TZrChar *op, FZrDebugEvalParseValue parseValue, SZrTypeValue *outValue)
 *
 * ## Summary
 * 由运算符优先级层选择同步解析回调，在 FALSE 返回后统一细化右侧错误。
 *
 * ## Parameters
 * @param[in,out] parser 借用本次状态并原样传给回调；非空回调的有效性前提由其自身约定，仓内递归回调要求有效状态。
 * @param[in] op 已消费运算符的借用展示文字，可为 NULL。
 * @param[in] parseValue 同步解析函数指针；NULL 不调用任何函数并进入失败诊断分支，不保存该指针。
 * @param[out] outValue 原样转交回调的值输出区；仓内回调要求可写。失败可能已有部分写入，不能当成功值使用。
 *
 * ## Returns
 * @return 非空回调返回 TRUE 时原样成功；否则细化诊断并返回 FALSE。
 *
 * ## Constraints and lifecycle
 * 不初始化或回滚 cursor/outValue，不取得值对象所有权，不捕获非局部退出；回调可能分配字符串，不能据包装器本身无分配推成整体无分配。
 * BUG: DED-BUG-001：上游允许非空错误指针配零容量且不清零；失败路径仍读取该区并按 NUL 文本比较。有效暂停状态下，以 1 +、char error[1]={0x65} 和容量 0 进入兼容回退，strcmp 首字节匹配后可读过实际数组；这是完整静态路径结论，未经运行复现，当前未修复。
 *
 * ## References
 * - zr_debug_eval_parse_right_operand 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:286
 * - zr_debug_eval_parse_multiplicative — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:653
 * - zr_debug_eval_parse_additive — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:710
 * - zr_debug_eval_parse_relational — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:766
 * - zr_debug_eval_parse_equality — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:797
 * - zr_debug_eval_parse_right_operand_with_skip — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:313
 * - 容量零的原错误指针仍传入兼容 parser — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:1020
 */

/**
 * @fn TZrBool zr_debug_eval_parse_right_operand_with_skip(ZrDebugEvalParser *parser, const TZrChar *op, FZrDebugEvalParseValue parseValue, SZrTypeValue *outValue, TZrBool skipEvaluation)
 *
 * ## Summary
 * 为逻辑短路或条件分支临时启用兼容解析的跳过取值状态，再同步调用右侧包装器。
 *
 * ## Parameters
 * @param[in,out] parser 借用解析状态；NULL 时仅委托普通包装器，是否可接受仍由实际回调前提决定。
 * @param[in] op 传给诊断层的借用运算符文字，可为 NULL。
 * @param[in] parseValue 本次同步递归回调，可为 NULL，语义与普通包装器相同。
 * @param[out] outValue 回调输出区；失败可能部分写入，本函数不恢复。
 * @param[in] skipEvaluation TRUE 且 parser 非空时临时置 skip_evaluation=TRUE；FALSE 委托并保留已有状态，不强制清除外层 skip。
 *
 * ## Returns
 * @return 返回普通包装器的解析状态；临时置位的 skip 仅在普通 C 返回路径恢复为先前值。
 *
 * ## Constraints and lifecycle
 * 仍解析语法并保留调用/赋值拒绝，不构成副作用能力授权或全程无分配保证；longjmp 等非局部退出不经过恢复，且不回滚 cursor/outValue。
 * BUG: DED-BUG-001：上游允许非空错误指针配零容量且不清零；失败路径仍读取该区并按 NUL 文本比较。有效暂停状态下，以 1 +、char error[1]={0x65} 和容量 0 进入兼容回退，strcmp 首字节匹配后可读过实际数组；这是完整静态路径结论，未经运行复现，当前未修复。
 *
 * ## References
 * - zr_debug_eval_parse_right_operand_with_skip 实现 — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:298
 * - zr_debug_eval_parse_logical_and — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:831
 * - zr_debug_eval_parse_logical_or — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:859
 * - zr_debug_eval_parse_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:897
 * - zr_debug_eval_parse_expression — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:914
 * - zr_debug_eval_parse_string — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:341
 * - 容量零的原错误指针仍传入兼容 parser — zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:1020
 */
