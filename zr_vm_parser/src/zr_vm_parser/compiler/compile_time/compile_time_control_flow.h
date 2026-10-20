#ifndef ZR_VM_PARSER_COMPILE_TIME_CONTROL_FLOW_H
#define ZR_VM_PARSER_COMPILE_TIME_CONTROL_FLOW_H

#include "zr_vm_parser/compiler.h"
#include "zr_vm_core/gc_domain.h"

typedef struct SZrCompileTimeFrame SZrCompileTimeFrame;

typedef enum EZrCompileTimeFlowStatus {
    ZR_PARSER_COMPTIME_FLOW_NORMAL = 0,
    ZR_PARSER_COMPTIME_FLOW_RETURN,
    ZR_PARSER_COMPTIME_FLOW_BREAK,
    ZR_PARSER_COMPTIME_FLOW_CONTINUE,
    ZR_PARSER_COMPTIME_FLOW_ERROR
} EZrCompileTimeFlowStatus;

typedef struct SZrCompileTimeFlowResult {
    EZrCompileTimeFlowStatus status;
    SZrTypeValue value;
    SZrGcRootHandle valueRoot;
    TZrBool hasValueRoot;
} SZrCompileTimeFlowResult;

EZrCompileTimeFlowStatus ZrParser_CompileTimeFlow_ExecuteStatement(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompileTimeFrame *frame,
        TZrBool inLoop,
        SZrCompileTimeFlowResult *result);

EZrCompileTimeFlowStatus ZrParser_CompileTimeFlow_ExecuteBlock(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompileTimeFrame *frame,
        TZrBool inLoop,
        SZrCompileTimeFlowResult *result);

#endif // ZR_VM_PARSER_COMPILE_TIME_CONTROL_FLOW_H
