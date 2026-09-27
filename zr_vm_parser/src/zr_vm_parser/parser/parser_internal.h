// 供 parser 翻译单元共享的词法游标、构造和释放契约。
#ifndef ZR_VM_PARSER_INTERNAL_H
#define ZR_VM_PARSER_INTERNAL_H

#include "zr_vm_parser/parser.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/** @brief 推测解析的词法快照；token 负载仍借用同一 lexer/state。
 * @note 恢复时必须使用保存时的 parser，且源文本与 token 负载仍有效。 */
typedef struct SZrParserCursor {
    TZrSize currentPos;
    TZrInt32 currentChar;
    TZrInt32 lineNumber;
    TZrInt32 lastLine;
    TZrSize currentLineStartOffset;
    TZrSize tokenStartOffset;
    TZrSize tokenStartLineStart;
    TZrInt32 tokenStartLine;
    SZrToken token;
    SZrToken lookahead;
    TZrSize lookaheadPos;
    TZrInt32 lookaheadChar;
    TZrInt32 lookaheadLine;
    TZrInt32 lookaheadLastLine;
    TZrSize lookaheadCurrentLineStartOffset;
    TZrSize lookaheadTokenStartOffset;
    TZrSize lookaheadTokenStartLineStart;
    TZrInt32 lookaheadTokenStartLine;
    TZrBool hasError;
    TZrBool hasFatalError;
    const TZrChar *errorMessage;
} SZrParserCursor;

/** @brief 属性解析器的宿主种类，决定 accessor 可用的语法形态。 */
typedef enum EZrPropertyContainerKind {
    ZR_PROPERTY_CONTAINER_CLASS = 0,
    ZR_PROPERTY_CONTAINER_STRUCT,
    ZR_PROPERTY_CONTAINER_INTERFACE
} EZrPropertyContainerKind;

void expect_token(SZrParserState *ps, EZrToken expected);

TZrBool consume_token(SZrParserState *ps, EZrToken token);

EZrToken peek_token(SZrParserState *ps);

/** @brief 保存当前位置、lookahead 和错误状态，以便语法探测后回滚。 */
void save_parser_cursor(SZrParserState *ps, SZrParserCursor *cursor);

/** @brief 在同一 parser 上恢复已保存的词法与错误状态。 */
void restore_parser_cursor(SZrParserState *ps, const SZrParserCursor *cursor);

TZrBool current_identifier_equals(SZrParserState *ps, const TZrChar *text);

TZrBool report_removed_percent_syntax(SZrParserState *ps);

void report_removed_legacy_syntax(SZrParserState *ps,
                                  const TZrChar *spelling,
                                  const TZrChar *suggestion);
void report_removed_legacy_syntax_at(SZrParserState *ps,
                                     SZrFileRange location,
                                     EZrToken token,
                                     const TZrChar *spelling,
                                     const TZrChar *suggestion);

TZrBool is_module_path_segment_token(EZrToken token);

TZrBool is_type_modifier_token(EZrToken token);

TZrBool is_member_modifier_token(EZrToken token);

TZrUInt32 token_to_declaration_modifier_flag(EZrToken token);

TZrUInt32 parse_declaration_modifier_flags(SZrParserState *ps, TZrUInt32 allowedFlags);

void skip_balanced_after_open_paren(SZrParserState *ps);

void skip_to_semicolon_or_eos(SZrParserState *ps);


/** @brief 将模块路径各段合成为字符串字面量 AST；字符串由 VM 管理，返回节点由调用方接管。 */
SZrAstNode *parse_normalized_dotted_module_path(SZrParserState *ps, const TZrChar *directiveName);

SZrAstNode *parse_normalized_module_path(SZrParserState *ps, const TZrChar *directiveName);

/** @brief 读取声明前的装饰器；返回数组及其中节点由调用方接管。 */
SZrAstNodeArray *parse_leading_decorators(SZrParserState *ps);

/** @brief 在泛型类型上下文消费一个右尖括号，必要时拆分右移 token。 */
TZrBool consume_type_closing_angle(SZrParserState *ps);

/** @brief 从当前 lexer 游标计算 AST 位置；首行列号偏差见实现处 BUG。 */
SZrFileRange get_current_location(SZrParserState *ps);

void get_string_view_for_length(SZrString *value, const TZrChar **text, TZrSize *length);

/** @brief 将源字节偏移换算为行列；单独 CR 的处理与 lexer 不一致，见实现处 BUG。 */
SZrFilePosition get_file_position_from_offset(SZrLexState *lexer, TZrSize offset);

TZrSize get_current_token_length(SZrParserState *ps);

/** @brief 基于 lexer 的 token 起点和源文本求精确范围，供诊断与 AST 定位。 */
ZR_PARSER_API SZrFileRange get_current_token_location(SZrParserState *ps);

/** @brief 从当前源行提取日志片段，并给出片段内的错误列。
 * @pre buffer 至少有 bufferSize 个可写字节，bufferSize 大于零；errorColumn 非空。 */
void get_line_snippet(SZrParserState *ps, TZrChar *buffer, TZrSize bufferSize, TZrInt32 *errorColumn);

/** @brief 标记 parser 错误并同步通知回调或日志。
 * @note 回调中的 msg 仅在调用期间有效；ps->errorMessage 当前借用 msg。 */
void report_error_with_token(SZrParserState *ps, const TZrChar *msg, EZrToken token);

void report_error(SZrParserState *ps, const TZrChar *msg);

TZrBool report_reserved_ownership_intrinsic_name(SZrParserState *ps);

/** @brief 先发布结构化诊断，再将 error 级别映射到旧错误状态。
 * @note 调用方在返回后仍负责释放 diagnostic。 */
void report_structured_parser_error(SZrParserState *ps,
                                    const SZrStructuredDiagnostic *diagnostic,
                                    EZrToken token);

void report_missing_expression_after_assignment(SZrParserState *ps);

void report_missing_right_operand(SZrParserState *ps, const TZrChar *operatorText, SZrFileRange operatorLocation);

void report_missing_condition(SZrParserState *ps, const TZrChar *statementKind, SZrFileRange location);

void report_missing_condition_close(SZrParserState *ps, const TZrChar *statementKind, SZrFileRange location);

void report_missing_member_name(SZrParserState *ps, SZrFileRange location);

void report_missing_index_close(SZrParserState *ps, SZrFileRange location);

void report_missing_call_close(SZrParserState *ps, SZrFileRange location);

void report_missing_parameter_list_close(SZrParserState *ps, SZrFileRange location);

void report_missing_group_close(SZrParserState *ps, SZrFileRange location);

void report_array_element_assignment(SZrParserState *ps, SZrFileRange location);

void report_missing_array_close(SZrParserState *ps, SZrFileRange location);

void report_missing_array_element_separator(SZrParserState *ps, SZrFileRange location);

void report_missing_object_close(SZrParserState *ps, SZrFileRange location);

void report_missing_object_computed_key_close(SZrParserState *ps, SZrFileRange location);

void report_missing_object_property_colon(SZrParserState *ps, SZrFileRange location);

void report_missing_object_property_separator(SZrParserState *ps, SZrFileRange location);

void report_missing_conditional_consequent(SZrParserState *ps, SZrFileRange location);

void report_missing_conditional_colon(SZrParserState *ps,
                                      SZrFileRange location,
                                      TZrBool hasAlternateExpression);

void report_missing_conditional_alternate(SZrParserState *ps, SZrFileRange location);

void report_missing_statement_semicolon(SZrParserState *ps, const TZrChar *statementKind, SZrFileRange location);

void report_missing_declaration_body_open(SZrParserState *ps,
                                          const TZrChar *declarationKind,
                                          SZrFileRange location);

void report_missing_declaration_body_close(SZrParserState *ps,
                                           const TZrChar *declarationKind,
                                           SZrFileRange location);

void report_missing_statement_body_open(SZrParserState *ps,
                                        const TZrChar *statementKind,
                                        SZrFileRange location);

void report_missing_block_close(SZrParserState *ps, SZrFileRange location);

void report_missing_catch_pattern_close(SZrParserState *ps, SZrFileRange location);

void report_missing_using_resource_close(SZrParserState *ps, SZrFileRange location);

void report_using_binder_invalid(SZrParserState *ps, SZrFileRange location);

void report_import_path_not_constant(SZrParserState *ps,
                                     SZrFileRange location,
                                     const TZrChar *directiveName);

void report_missing_for_header_close(SZrParserState *ps, SZrFileRange location);

void report_missing_for_header_separator(SZrParserState *ps, SZrFileRange location);

void report_missing_foreach_header_close(SZrParserState *ps, SZrFileRange location);

void report_missing_foreach_in_keyword(SZrParserState *ps, SZrFileRange location);

void report_missing_switch_case_header_close(SZrParserState *ps, SZrFileRange location);

void report_missing_switch_body_close(SZrParserState *ps, SZrFileRange location);

void report_missing_extern_spec_close(SZrParserState *ps, SZrFileRange location);

void report_missing_test_name_close(SZrParserState *ps, SZrFileRange location);

SZrAstNode *create_ast_node(SZrParserState *ps, EZrAstNodeType type, SZrFileRange location);

SZrAstNode *create_identifier_node_with_location(SZrParserState *ps, SZrString *name, SZrFileRange location);

SZrAstNode *create_identifier_node(SZrParserState *ps, SZrString *name);

SZrAstNode *create_boolean_literal_node(SZrParserState *ps, TZrBool value);

SZrAstNode *create_integer_literal_node(SZrParserState *ps, TZrInt64 value, SZrString *literal);

SZrAstNode *create_float_literal_node(SZrParserState *ps, TZrDouble value, SZrString *literal,
                                             TZrBool isSingle);

SZrAstNode *create_string_literal_node(SZrParserState *ps, SZrString *value, TZrBool hasError,
                                              SZrString *literal);

SZrAstNode *create_string_literal_node_with_location(SZrParserState *ps,
                                                            SZrString *value,
                                                            TZrBool hasError,
                                                            SZrString *literal,
                                                            SZrFileRange location);

SZrAstNode *create_char_literal_node(SZrParserState *ps, TZrChar value, TZrBool hasError, SZrString *literal);

SZrAstNode *create_null_literal_node(SZrParserState *ps);

SZrAstNode *create_template_string_literal_node(SZrParserState *ps, SZrAstNodeArray *segments);

SZrAstNode *create_interpolated_segment_node(SZrParserState *ps, SZrAstNode *expression);

void get_string_native_parts(SZrString *value, TZrNativeString *nativeValue, TZrSize *length);

TZrBool zr_string_equals_literal(SZrString *value, const TZrChar *literal);

TZrBool try_get_ownership_qualifier(SZrString *name, EZrOwnershipQualifier *qualifier);

SZrAstNode *parse_embedded_expression(SZrParserState *ps, const TZrChar *source, TZrSize sourceLength);

TZrBool append_template_static_segment(SZrParserState *ps, SZrAstNodeArray *segments, const TZrChar *text,
                                              TZrSize length);

SZrAstNode *parse_template_string_literal(SZrParserState *ps, SZrString *rawValue);

SZrAstNode *parse_literal(SZrParserState *ps);

SZrAstNode *parse_identifier(SZrParserState *ps);

TZrBool is_ownership_intrinsic_token(EZrToken token);

SZrAstNode *parse_member_identifier(SZrParserState *ps);

SZrAstNode *parse_ownership_intrinsic_expression(SZrParserState *ps);

SZrAstNode *parse_array_literal(SZrParserState *ps);

SZrAstNode *parse_object_literal(SZrParserState *ps);

/** @brief 解析调用实参和可选的名称、传递标记侧数组。
 * @note 返回的数组由调用方接管；解析错误时可能返回部分 AST，须结合 ps->hasError 清理。 */
SZrAstNodeArray *parse_argument_list(
        SZrParserState *ps,
        SZrArray **argNames,
        SZrArray **argumentMarkers);

TZrBool call_has_explicit_argument_marker(const SZrArray *markers);

SZrAstNode *append_primary_member(SZrParserState *ps, SZrAstNode *base, SZrAstNode *memberNode,
                                         SZrFileRange startLoc);

SZrAstNode *try_parse_braced_primary_member(SZrParserState *ps,
                                            SZrAstNode *base,
                                            SZrFileRange startLoc,
                                            TZrBool *outHandled);

TZrBool is_lambda_expression_after_lparen(SZrParserState *ps);

SZrAstNodeArray *create_empty_argument_list(SZrParserState *ps);

TZrBool reject_named_construct_arguments(SZrParserState *ps, SZrArray *argNames, SZrFileRange location);

SZrAstNode *create_prototype_reference_node(SZrParserState *ps, SZrAstNode *target, SZrFileRange location);

SZrAstNode *create_construct_expression_node(SZrParserState *ps, SZrAstNode *target, SZrAstNodeArray *args,
                                                    EZrOwnershipQualifier ownershipQualifier, TZrBool isUsing,
                                                    TZrBool isNew, EZrOwnershipBuiltinKind builtinKind,
                                                    SZrFileRange location);

SZrAstNode *parse_prototype_path_expression(SZrParserState *ps);

SZrAstNode *parse_construct_expression(SZrParserState *ps,
                                              SZrFileRange startLoc,
                                              EZrOwnershipQualifier ownershipQualifier,
                                              TZrBool isUsing,
                                              EZrOwnershipBuiltinKind builtinKind);

SZrAstNode *parse_struct_init_expression(SZrParserState *ps);

SZrAstNode *parse_reference_expression(SZrParserState *ps);

SZrAstNode *parse_reserved_import_expression(SZrParserState *ps);

SZrAstNode *parse_await_expression(SZrParserState *ps);

SZrAstNode *parse_reserved_async_function_declaration(SZrParserState *ps);

SZrAstNode *parse_member_access(SZrParserState *ps, SZrAstNode *base);

SZrAstNode *parse_postfix_call_segment(SZrParserState *ps,
                                       SZrAstNode *base,
                                       SZrFileRange chainStartLoc,
                                       SZrFileRange segmentStartLoc,
                                       EZrPostfixAccessMode accessMode);

SZrAstNode *parse_primary_expression(SZrParserState *ps);

SZrAstNode *parse_fn_expression(SZrParserState *ps);

SZrAstNode *parse_unary_expression(SZrParserState *ps);

SZrAstNode *parse_multiplicative_expression(SZrParserState *ps);

SZrAstNode *parse_additive_expression(SZrParserState *ps);

SZrAstNode *parse_shift_expression(SZrParserState *ps);

SZrAstNode *parse_relational_expression(SZrParserState *ps);

SZrAstNode *parse_equality_expression(SZrParserState *ps);

SZrAstNode *parse_binary_and_expression(SZrParserState *ps);

SZrAstNode *parse_binary_xor_expression(SZrParserState *ps);

SZrAstNode *parse_binary_or_expression(SZrParserState *ps);

SZrAstNode *parse_logical_and_expression(SZrParserState *ps);

SZrAstNode *parse_logical_or_expression(SZrParserState *ps);

SZrAstNode *parse_conditional_expression(SZrParserState *ps);

SZrAstNode *parse_assignment_expression(SZrParserState *ps);

SZrAstNode *parse_expression(SZrParserState *ps);

SZrAstNode *parse_generic_type(SZrParserState *ps);

SZrAstNodeArray *parse_generic_argument_list(SZrParserState *ps);

SZrAstNode *parse_tuple_type(SZrParserState *ps);

SZrType *parse_type(SZrParserState *ps);

SZrType *parse_type_no_generic(SZrParserState *ps);

TZrBool parse_array_size_constraint(SZrParserState *ps, SZrType *type);

SZrGenericDeclaration *parse_generic_declaration(SZrParserState *ps, TZrBool allowVariance);

TZrBool parse_optional_where_clauses(SZrParserState *ps, SZrGenericDeclaration *generic);

SZrAstNode *parse_meta_identifier(SZrParserState *ps);

SZrAstNode *parse_decorator_expression(SZrParserState *ps);

SZrAstNode *parse_destructuring_object(SZrParserState *ps);

SZrAstNode *parse_destructuring_array(SZrParserState *ps);

EZrAccessModifier parse_access_modifier(SZrParserState *ps);

SZrAstNode *parse_parameter(SZrParserState *ps);

SZrAstNodeArray *parse_parameter_list(SZrParserState *ps);

TZrBool parse_parameter_source_passing_form(
        SZrParserState *ps,
        EZrParameterSourcePassingForm *sourceForm,
        EZrParameterPassingMode *legacyMode,
        SZrFileRange *location);

SZrAstNode *parse_module_declaration(SZrParserState *ps);

SZrAstNode *parse_variable_declaration(SZrParserState *ps);

SZrAstNode *parse_variable_declaration_for_header(SZrParserState *ps);

SZrAstNode *parse_function_declaration(SZrParserState *ps);

SZrAstNode *parse_block(SZrParserState *ps);

SZrAstNode *parse_declaration_body_block(SZrParserState *ps, const TZrChar *declarationKind);

SZrAstNode *parse_expression_statement(SZrParserState *ps);

SZrAstNode *parse_return_statement(SZrParserState *ps);

SZrAstNode *try_parse_switch_struct_variant_payload_case(SZrParserState *ps, SZrAstNode *value);

SZrAstNode *try_parse_switch_move_variant_pattern_case(SZrParserState *ps);

SZrAstNode *parse_switch_expression(SZrParserState *ps);

SZrAstNode *parse_if_expression(SZrParserState *ps);

SZrAstNode *parse_while_loop(SZrParserState *ps);

SZrAstNode *parse_for_loop(SZrParserState *ps);

SZrAstNode *parse_foreach_loop(SZrParserState *ps);

SZrAstNode *parse_break_continue_statement(SZrParserState *ps);

SZrAstNode *parse_out_statement(SZrParserState *ps);

SZrAstNode *parse_yield_statement(SZrParserState *ps);

SZrAstNode *parse_throw_statement(SZrParserState *ps);

SZrAstNode *parse_try_catch_finally_statement(SZrParserState *ps);

SZrAstNode *parse_using_statement(SZrParserState *ps);

SZrAstNode *parse_statement(SZrParserState *ps);

SZrAstNode *parse_top_level_statement(SZrParserState *ps);

SZrAstNode *parse_script(SZrParserState *ps);

/** @brief 递归释放类型的子节点，但不释放 type 本身。 */
void free_type_info(SZrState *state, SZrType *type);

/** @brief 释放数组中的各 AST 节点及数组容器。 */
void free_ast_node_array_with_elements(SZrState *state, SZrAstNodeArray *array);

/** @brief 从嵌入的 identifier 字段还原并释放其 AST 外层节点。
 * @pre identifier 指向 SZrAstNode.data.identifier，而非独立分配的结构。 */
void free_identifier_node_from_ptr(SZrState *state, SZrIdentifier *identifier);

/** @brief 从嵌入的 parameter 字段还原并释放其 AST 外层节点。
 * @pre parameter 指向 SZrAstNode.data.parameter。 */
void free_parameter_node_from_ptr(SZrState *state, SZrParameter *parameter);

/** @brief 释放类型子树和独立分配的 SZrType 外层结构。 */
void free_owned_type(SZrState *state, SZrType *type);

/** @brief 释放泛型形参数组及泛型声明结构。 */
void free_generic_declaration(SZrState *state, SZrGenericDeclaration *generic);

SZrAstNode *parse_struct_field(SZrParserState *ps);

SZrAstNode *parse_struct_method(SZrParserState *ps);

SZrAstNode *parse_struct_meta_function(SZrParserState *ps);

TZrBool parser_struct_declaration_starts_here(SZrParserState *ps);

SZrAstNode *parse_struct_declaration(SZrParserState *ps);

SZrAstNode *parse_class_declaration(SZrParserState *ps);

SZrAstNode *parse_interface_field_declaration(SZrParserState *ps);

SZrAstNode *parse_interface_method_signature(SZrParserState *ps);

SZrAstNode *parse_interface_property_signature(SZrParserState *ps);

SZrAstNode *parse_interface_meta_signature(SZrParserState *ps);

SZrAstNode *parse_interface_declaration(SZrParserState *ps);

SZrAstNode *parse_enum_member(SZrParserState *ps);

SZrAstNode *parse_enum_declaration(SZrParserState *ps);

SZrAstNode *parse_union_variant(SZrParserState *ps);

SZrAstNode *parse_union_declaration(SZrParserState *ps);

SZrAstNode *parse_extern_function_declaration(SZrParserState *ps, SZrAstNodeArray *decorators);

SZrAstNode *parse_extern_delegate_declaration(SZrParserState *ps, SZrAstNodeArray *decorators);

SZrAstNode *parse_extern_block(SZrParserState *ps);

SZrAstNode *parse_compile_time_declaration(SZrParserState *ps);

SZrAstNode *parse_generator_expression(SZrParserState *ps);

SZrAstNode *parse_class_field(SZrParserState *ps);

SZrAstNode *parse_class_method(SZrParserState *ps);

SZrAstNode *parse_property_get(SZrParserState *ps);

SZrAstNode *parse_property_set(SZrParserState *ps);

SZrAstNode *parse_class_property(SZrParserState *ps);

TZrBool parser_property_declaration_starts_here(SZrParserState *ps);

SZrAstNode *parse_property_declaration(SZrParserState *ps,
                                       EZrPropertyContainerKind containerKind);

SZrAstNode *parse_class_meta_function(SZrParserState *ps);

#endif // ZR_VM_PARSER_INTERNAL_H
