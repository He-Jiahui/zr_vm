#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_H

#include "zr_vm_parser/cfg.h"

typedef enum EZrParserDataflowDirection {
    ZR_PARSER_DATAFLOW_FORWARD = 0,
    ZR_PARSER_DATAFLOW_BACKWARD
} EZrParserDataflowDirection;

typedef void (*TZrParserDataflowInitFn)(void *state, void *userData);
typedef TZrBool (*TZrParserDataflowJoinFn)(void *dst, const void *src, void *userData);
typedef void (*TZrParserDataflowTransferFn)(SZrAstNode *statement, void *state, void *userData);

typedef struct SZrParserDataflowAnalysis {
    EZrParserDataflowDirection direction;
    TZrSize stateSize;
    TZrParserDataflowInitFn initEntry;
    TZrParserDataflowJoinFn join;
    TZrParserDataflowTransferFn transferStatement;
    void *userData;
} SZrParserDataflowAnalysis;

typedef struct SZrParserDataflowBlockState {
    TZrBool isReachable;
    void *inState;
    void *outState;
} SZrParserDataflowBlockState;

typedef struct SZrParserDataflowResult {
    SZrArray blockStates;
    TZrSize stateSize;
} SZrParserDataflowResult;

ZR_PARSER_API void ZrParser_DataflowResult_Init(SZrParserDataflowResult *result);
ZR_PARSER_API void ZrParser_DataflowResult_Free(SZrState *state, SZrParserDataflowResult *result);
ZR_PARSER_API const SZrParserDataflowBlockState *ZrParser_Dataflow_GetBlockState(
        const SZrParserDataflowResult *result,
        TZrUInt32 blockId);
ZR_PARSER_API TZrBool ZrParser_Dataflow_Run(SZrState *state,
                                            const SZrParserCfg *cfg,
                                            const SZrParserDataflowAnalysis *analysis,
                                            SZrParserDataflowResult *result);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_H
