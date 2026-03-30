#ifndef ZR_VM_PARSER_COMPILE_TIME_EXECUTOR_INTERNAL_H
#define ZR_VM_PARSER_COMPILE_TIME_EXECUTOR_INTERNAL_H

#include "zr_vm_parser/compiler.h"

typedef struct SZrCompileTimeBinding {
    SZrString *name;
    SZrTypeValue value;
} SZrCompileTimeBinding;

typedef struct SZrCompileTimeFrame {
    SZrArray bindings;
    struct SZrCompileTimeFrame *parent;
} SZrCompileTimeFrame;

const TZrChar *ct_name(SZrString *name);
void ct_error_name(SZrCompilerState *cs, SZrString *name, const TZrChar *prefix, SZrFileRange location);
TZrBool ct_truthy(const SZrTypeValue *value);
void ct_init_type_from_value(SZrCompilerState *cs, const SZrTypeValue *value, SZrInferredType *result);
TZrBool ct_string_equals(SZrString *value, const TZrChar *literal);
TZrBool ct_eval_import_expression(SZrCompilerState *cs, SZrAstNode *node, SZrTypeValue *result);
void ct_frame_init(SZrCompilerState *cs, SZrCompileTimeFrame *frame, SZrCompileTimeFrame *parent);
void ct_frame_free(SZrCompilerState *cs, SZrCompileTimeFrame *frame);
TZrBool ct_frame_get(SZrCompileTimeFrame *frame, SZrString *name, SZrTypeValue *result);
TZrBool ct_frame_set(SZrCompilerState *cs, SZrCompileTimeFrame *frame, SZrString *name, const SZrTypeValue *value);
SZrCompileTimeVariable *find_compile_time_variable(SZrCompilerState *cs, SZrString *name);
SZrCompileTimeFunction *find_compile_time_function(SZrCompilerState *cs, SZrString *name);
TZrBool ct_value_from_compile_time_function(SZrCompilerState *cs, SZrCompileTimeFunction *func, SZrTypeValue *result);
TZrBool ct_value_try_get_compile_time_function(SZrCompilerState *cs,
                                               const SZrTypeValue *value,
                                               SZrCompileTimeFunction **result);
TZrBool evaluate_compile_time_expression_internal(SZrCompilerState *cs,
                                                  SZrAstNode *node,
                                                  SZrCompileTimeFrame *frame,
                                                  SZrTypeValue *result);
TZrBool register_compile_time_variable_declaration(SZrCompilerState *cs, SZrAstNode *node, SZrFileRange location);
TZrBool register_compile_time_function_declaration(SZrCompilerState *cs, SZrAstNode *node, SZrFileRange location);
TZrBool execute_compile_time_statement(SZrCompilerState *cs,
                                       SZrAstNode *node,
                                       SZrCompileTimeFrame *frame,
                                       TZrBool *didReturn,
                                       SZrTypeValue *result);
TZrBool execute_compile_time_block(SZrCompilerState *cs,
                                   SZrAstNode *node,
                                   SZrCompileTimeFrame *frame,
                                   TZrBool *didReturn,
                                   SZrTypeValue *result);

#endif // ZR_VM_PARSER_COMPILE_TIME_EXECUTOR_INTERNAL_H
