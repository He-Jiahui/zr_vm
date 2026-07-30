#ifndef ZR_VM_PARSER_COMPILE_TOOL_EVALUATOR_H
#define ZR_VM_PARSER_COMPILE_TOOL_EVALUATOR_H

#include "compile_time_executor_internal.h"

TZrBool ZrParser_CompileToolEvaluator_TryEvaluate(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompileTimeFrame *frame,
        SZrTypeValue *result,
        TZrBool *handled);

#endif
