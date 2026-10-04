#ifndef ZR_VM_PARSER_EXEC_IR_BRANCH_FACTS_H
#define ZR_VM_PARSER_EXEC_IR_BRANCH_FACTS_H

#include "zr_vm_parser/exec_ir_ranges.h"

typedef struct SZrExecIrBranchValueFact {
    TZrBool available;
    TZrBool signedInteger;
    TZrBool hasLower;
    TZrBool hasUpper;
    /* Known upstream poison or possible checked ADD/SUB endpoint overflow.
     * False alone is not a nonoverflow proof: complete witnessed bounds are
     * also required. Missing-domain/missing-bound facts remain UNKNOWN. */
    TZrBool overflowed;
    TZrInt64 lower;
    TZrInt64 upper;
    EZrExecIrNullFact nullState;
} SZrExecIrBranchValueFact;

typedef struct SZrExecIrBranchEntryFact {
    TZrExecIrValueId valueId;
    SZrExecIrBranchValueFact fact;
} SZrExecIrBranchEntryFact;

/* Domain witnesses are explicit caller proofs, never raw type-token tests.
 * They may cover local definitions, but do not make those values available
 * before definition. Entry bounds/null witnesses require external SSA values.
 * Analyze copies witnesses; caller arrays need only live for that call. Change
 * revision or generation whenever the upstream proof producer changes. */
typedef struct SZrExecIrBranchFactsInput {
    TZrUInt64 revision;
    TZrUInt64 generation;
    const TZrExecIrValueId *signedValues;
    TZrUInt32 signedValueCount;
    const SZrExecIrBranchEntryFact *entries;
    TZrUInt32 entryCount;
} SZrExecIrBranchFactsInput;

typedef struct SZrExecIrBranchFacts {
    const SZrExecIrModule *module;
    const SZrExecIrFunction *function;
    TZrUInt64 revision;
    TZrUInt64 generation;
    TZrUInt64 irHash;
    TZrUInt32 blockCount;
    TZrUInt32 valueCount;
    TZrUInt32 edgeCount;
    SZrExecIrBranchValueFact *blockEntries;
    SZrExecIrBranchValueFact *blockExits;
    SZrExecIrBranchValueFact *edges;
    TZrBool *blockReachable;
    TZrBool *edgeReachable;
    TZrExecIrBlockId *edgeSources;
    TZrUInt32 *edgeOrdinals;
    TZrBool *signedWitnesses;
    SZrExecIrBranchEntryFact *entryWitnesses;
    TZrUInt32 entryWitnessCount;
    TZrBool cyclicFallback;
} SZrExecIrBranchFacts;

ZR_PARSER_API void ZrParser_ExecIr_BranchFactsInit(SZrExecIrBranchFacts *result);
ZR_PARSER_API void ZrParser_ExecIr_BranchFactsFree(SZrExecIrBranchFacts *result);
ZR_PARSER_API TZrBool ZrParser_ExecIr_AnalyzeBranchFacts(
        const SZrExecIrModule *module, const SZrExecIrFunction *function,
        const SZrExecIrBranchFactsInput *input, SZrExecIrBranchFacts *result,
        SZrExecIrDiagnostic *diagnostic);
/* Must succeed before any query is consumed as a proof. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_BranchFactsIsCurrent(
        const SZrExecIrModule *module, const SZrExecIrFunction *function,
        const SZrExecIrBranchFacts *result,
        TZrUInt64 revision, TZrUInt64 generation);
ZR_PARSER_API const SZrExecIrBranchValueFact *
ZrParser_ExecIr_BranchFactsAtBlockEntry(const SZrExecIrBranchFacts *result,
        TZrExecIrBlockId block, TZrExecIrValueId value);
ZR_PARSER_API const SZrExecIrBranchValueFact *
ZrParser_ExecIr_BranchFactsAtEdge(const SZrExecIrBranchFacts *result,
        TZrExecIrBlockId predecessor, TZrUInt32 successorOrdinal,
        TZrExecIrValueId value);

#endif
