#ifndef ZR_VM_PARSER_COMPILE_TIME_CALL_H
#define ZR_VM_PARSER_COMPILE_TIME_CALL_H

#include "compiler/compile_time/compile_time_executor_internal.h"

/* A successful selection may contain a NULL AST; evaluation owns diagnostics. */
TZrBool ct_select_supplied_call_arg(SZrFunctionCall *call,
                                    SZrParameter *param,
                                    TZrSize paramIndex,
                                    SZrAstNode **outArgument);

TZrBool ct_call_function(SZrCompilerState *cs,
                        SZrAstNode *callSite,
                        SZrCompileTimeFunction *func,
                        SZrFunctionCall *call,
                        SZrCompileTimeFrame *parentFrame,
                        SZrTypeValue *result);

#endif
