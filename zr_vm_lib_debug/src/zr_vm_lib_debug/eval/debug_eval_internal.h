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
