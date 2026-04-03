// Internal parser helpers shared across parser translation units.
#ifndef ZR_VM_PARSER_INTERNAL_H
#define ZR_VM_PARSER_INTERNAL_H

#include "zr_vm_parser/parser.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct SZrParserCursor {
    TZrSize currentPos;
    TZrInt32 currentChar;
    TZrInt32 lineNumber;
    TZrInt32 lastLine;
    SZrToken token;
    SZrToken lookahead;
    TZrSize lookaheadPos;
    TZrInt32 lookaheadChar;
    TZrInt32 lookaheadLine;
    TZrInt32 lookaheadLastLine;
    TZrBool hasError;
    const TZrChar *errorMessage;
} SZrParserCursor;

void expect_token(SZrParserState *ps, EZrToken expected);

TZrBool consume_token(SZrParserState *ps, EZrToken token);

EZrToken peek_token(SZrParserState *ps);

void save_parser_cursor(SZrParserState *ps, SZrParserCursor *cursor);

void restore_parser_cursor(SZrParserState *ps, const SZrParserCursor *cursor);

TZrBool current_identifier_equals(SZrParserState *ps, const TZrChar *text);

TZrBool current_percent_directive_equals(SZrParserState *ps, const TZrChar *text);

TZrBool is_module_path_segment_token(EZrToken token);

void skip_balanced_after_open_paren(SZrParserState *ps);

void skip_to_semicolon_or_eos(SZrParserState *ps);

void skip_legacy_import_call(SZrParserState *ps);

SZrAstNode *parse_normalized_dotted_module_path(SZrParserState *ps, const TZrChar *directiveName);

SZrAstNode *parse_normalized_module_path(SZrParserState *ps, const TZrChar *directiveName);

SZrAstNodeArray *parse_leading_decorators(SZrParserState *ps);

TZrBool consume_type_closing_angle(SZrParserState *ps);

SZrFileRange get_current_location(SZrParserState *ps);

void get_string_view_for_length(SZrString *value, const TZrChar **text, TZrSize *length);

SZrFilePosition get_file_position_from_offset(SZrLexState *lexer, TZrSize offset);

TZrSize get_current_token_length(SZrParserState *ps);

SZrFileRange get_current_token_location(SZrParserState *ps);

void get_line_snippet(SZrParserState *ps, TZrChar *buffer, TZrSize bufferSize, TZrInt32 *errorColumn);

void report_error_with_token(SZrParserState *ps, const TZrChar *msg, EZrToken token);

void report_error(SZrParserState *ps, const TZrChar *msg);

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

SZrAstNode *parse_array_literal(SZrParserState *ps);

SZrAstNode *parse_object_literal(SZrParserState *ps);

SZrAstNodeArray *parse_argument_list(SZrParserState *ps, SZrArray **argNames);

SZrAstNode *append_primary_member(SZrParserState *ps, SZrAstNode *base, SZrAstNode *memberNode,
                                         SZrFileRange startLoc);

TZrBool is_lambda_expression_after_lparen(SZrParserState *ps);

TZrBool is_expression_level_using_new(SZrParserState *ps);

SZrAstNodeArray *create_empty_argument_list(SZrParserState *ps);

TZrBool reject_named_construct_arguments(SZrParserState *ps, SZrArray *argNames, SZrFileRange location);

SZrAstNode *create_prototype_reference_node(SZrParserState *ps, SZrAstNode *target, SZrFileRange location);

SZrAstNode *create_construct_expression_node(SZrParserState *ps, SZrAstNode *target, SZrAstNodeArray *args,
                                                    EZrOwnershipQualifier ownershipQualifier, TZrBool isUsing,
                                                    TZrBool isNew, EZrOwnershipBuiltinKind builtinKind,
                                                    SZrFileRange location);

SZrAstNode *parse_prototype_path_expression(SZrParserState *ps);

SZrAstNode *parse_prototype_reference_expression(SZrParserState *ps);

SZrAstNode *parse_construct_expression(SZrParserState *ps,
                                              SZrFileRange startLoc,
                                              EZrOwnershipQualifier ownershipQualifier,
                                              TZrBool isUsing,
                                              EZrOwnershipBuiltinKind builtinKind);

EZrOwnershipQualifier parse_optional_method_receiver_qualifier(SZrParserState *ps);

SZrAstNode *parse_percent_ownership_expression(SZrParserState *ps);

SZrAstNode *parse_reserved_import_expression(SZrParserState *ps);

SZrAstNode *parse_reserved_type_expression(SZrParserState *ps);

SZrAstNode *parse_owned_class_declaration(SZrParserState *ps);

SZrAstNode *parse_member_access(SZrParserState *ps, SZrAstNode *base);

SZrAstNode *parse_primary_expression(SZrParserState *ps);

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

SZrAstNode *parse_module_declaration(SZrParserState *ps);

SZrAstNode *parse_variable_declaration(SZrParserState *ps);

SZrAstNode *parse_function_declaration(SZrParserState *ps);

SZrAstNode *parse_block(SZrParserState *ps);

SZrAstNode *parse_expression_statement(SZrParserState *ps);

SZrAstNode *parse_return_statement(SZrParserState *ps);

SZrAstNode *parse_switch_expression(SZrParserState *ps);

SZrAstNode *parse_if_expression(SZrParserState *ps);

SZrAstNode *parse_while_loop(SZrParserState *ps);

SZrAstNode *parse_for_loop(SZrParserState *ps);

SZrAstNode *parse_foreach_loop(SZrParserState *ps);

SZrAstNode *parse_break_continue_statement(SZrParserState *ps);

SZrAstNode *parse_out_statement(SZrParserState *ps);

SZrAstNode *parse_throw_statement(SZrParserState *ps);

SZrAstNode *parse_try_catch_finally_statement(SZrParserState *ps);

SZrAstNode *parse_using_statement(SZrParserState *ps);

SZrAstNode *parse_statement(SZrParserState *ps);

SZrAstNode *parse_top_level_statement(SZrParserState *ps);

SZrAstNode *parse_script(SZrParserState *ps);

void free_type_info(SZrState *state, SZrType *type);

void free_ast_node_array_with_elements(SZrState *state, SZrAstNodeArray *array);

void free_identifier_node_from_ptr(SZrState *state, SZrIdentifier *identifier);

void free_parameter_node_from_ptr(SZrState *state, SZrParameter *parameter);

void free_owned_type(SZrState *state, SZrType *type);

void free_generic_declaration(SZrState *state, SZrGenericDeclaration *generic);

SZrAstNode *parse_struct_field(SZrParserState *ps);

SZrAstNode *parse_struct_method(SZrParserState *ps);

SZrAstNode *parse_struct_meta_function(SZrParserState *ps);

SZrAstNode *parse_struct_declaration(SZrParserState *ps);

SZrAstNode *parse_class_declaration(SZrParserState *ps);

SZrAstNode *parse_interface_field_declaration(SZrParserState *ps);

SZrAstNode *parse_interface_method_signature(SZrParserState *ps);

SZrAstNode *parse_interface_property_signature(SZrParserState *ps);

SZrAstNode *parse_interface_meta_signature(SZrParserState *ps);

SZrAstNode *parse_interface_declaration(SZrParserState *ps);

SZrAstNode *parse_enum_member(SZrParserState *ps);

SZrAstNode *parse_enum_declaration(SZrParserState *ps);

SZrAstNode *parse_extern_function_declaration(SZrParserState *ps, SZrAstNodeArray *decorators);

SZrAstNode *parse_extern_delegate_declaration(SZrParserState *ps, SZrAstNodeArray *decorators);

SZrAstNode *parse_extern_member_declaration(SZrParserState *ps);

SZrAstNode *parse_extern_block(SZrParserState *ps);

SZrAstNode *parse_test_declaration(SZrParserState *ps);

TZrBool is_compile_time_function_declaration(SZrParserState *ps);

SZrAstNode *parse_compile_time_declaration(SZrParserState *ps);

SZrAstNode *parse_intermediate_instruction_parameter(SZrParserState *ps);

SZrAstNode *parse_intermediate_instruction(SZrParserState *ps);

SZrAstNode *parse_intermediate_constant(SZrParserState *ps);

SZrAstNode *parse_intermediate_declaration(SZrParserState *ps);

SZrAstNode *parse_intermediate_statement(SZrParserState *ps);

SZrAstNode *parse_generator_expression(SZrParserState *ps);

SZrAstNode *parse_class_field(SZrParserState *ps);

SZrAstNode *parse_class_method(SZrParserState *ps);

SZrAstNode *parse_property_get(SZrParserState *ps);

SZrAstNode *parse_property_set(SZrParserState *ps);

SZrAstNode *parse_class_property(SZrParserState *ps);

SZrAstNode *parse_class_meta_function(SZrParserState *ps);

#endif // ZR_VM_PARSER_INTERNAL_H
