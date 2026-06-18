#ifndef ZR_VM_PARSER_COMPILER_TASK_EFFECTS_INTERNAL_H
#define ZR_VM_PARSER_COMPILER_TASK_EFFECTS_INTERNAL_H

#include "compiler_internal.h"

typedef struct ZrTaskEffectContext ZrTaskEffectContext;

void task_effects_validate_node(ZrTaskEffectContext *context, SZrAstNode *node);
void task_effects_validate_node_array(ZrTaskEffectContext *context, SZrAstNodeArray *nodes);
void task_effects_validate_function_like(ZrTaskEffectContext *parentContext,
                                         TZrBool asyncAllowed,
                                         SZrAstNodeArray *params,
                                         SZrParameter *args,
                                         SZrAstNode *body);
void task_effects_validate_decorators(ZrTaskEffectContext *context, SZrAstNodeArray *decorators);
void task_effects_validate_declaration(ZrTaskEffectContext *context, SZrAstNode *node);

#endif
