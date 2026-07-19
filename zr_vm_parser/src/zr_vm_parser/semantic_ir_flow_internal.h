#ifndef ZR_VM_PARSER_SEMANTIC_IR_FLOW_INTERNAL_H
#define ZR_VM_PARSER_SEMANTIC_IR_FLOW_INTERNAL_H

#include "zr_vm_parser/semantic_ir.h"

typedef struct SSemanticLoanAnalysis {
    SZrState *state;
    const SZrSemanticIrFunction *function;
    const SZrParserCfg *cfg;
    SZrSemanticFlowResult *result;
    TZrSize loanCount;
    TZrSize valueCount;
    TZrSize placeCount;
    TZrSize instructionCount;
    TZrSize blockCount;
    TZrSize trackedPlaceCount;
    TZrBool *valueLoans;
    TZrBool *placeLoans;
    TZrSize *trackedPlaceIndices;
    TZrBool *blockPlaceIn;
    TZrBool *blockPlaceOut;
    TZrBool *instructionUses;
    TZrBool *instructionLiveIn;
    TZrBool *instructionLiveOut;
    TZrBool *blockLiveIn;
    TZrBool *blockLiveOut;
    TZrBool *directParentLoans;
    TZrBool *ancestorLoans;
    TZrLoanId *parentLoanIds;
    TZrUInt32 *instructionBlockIds;
} SSemanticLoanAnalysis;

TZrBool semantic_loan_publish_liveness(SSemanticLoanAnalysis *analysis);
void semantic_loan_check_conflicts(SSemanticLoanAnalysis *analysis);

TZrBool semantic_loan_liveness_analyze(
        SZrState *state,
        const SZrSemanticIrFunction *function,
        const SZrParserCfg *cfg,
        SZrSemanticFlowResult *result);

#endif /* ZR_VM_PARSER_SEMANTIC_IR_FLOW_INTERNAL_H */
