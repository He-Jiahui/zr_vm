#ifndef ZR_VM_PARSER_COMPILE_STATEMENT_INTERNAL_H
#define ZR_VM_PARSER_COMPILE_STATEMENT_INTERNAL_H

#include "compiler_internal.h"

void compile_while_statement(SZrCompilerState *cs, SZrAstNode *node);
void compile_for_statement(SZrCompilerState *cs, SZrAstNode *node);
void compile_foreach_statement(SZrCompilerState *cs, SZrAstNode *node);
void compile_switch_statement(SZrCompilerState *cs, SZrAstNode *node);
void compile_break_continue_statement(SZrCompilerState *cs, SZrAstNode *node);
void compile_out_statement(SZrCompilerState *cs, SZrAstNode *node);
void compile_throw_statement(SZrCompilerState *cs, SZrAstNode *node);
void compile_try_catch_finally_statement(SZrCompilerState *cs, SZrAstNode *node);
void compile_destructuring_object(SZrCompilerState *cs, SZrAstNode *pattern, SZrAstNode *value);
void compile_destructuring_array(SZrCompilerState *cs, SZrAstNode *pattern, SZrAstNode *value);
TZrBool try_context_find_innermost_finally(const SZrCompilerState *cs, SZrCompilerTryContext *outContext);
TZrUInt32 bind_existing_stack_slot_as_local_var(SZrCompilerState *cs,
                                                SZrString *name,
                                                TZrUInt32 stackSlot,
                                                TZrUInt32 activateOffset);
void emit_jump_to_label(SZrCompilerState *cs, TZrSize labelId);

#endif // ZR_VM_PARSER_COMPILE_STATEMENT_INTERNAL_H
