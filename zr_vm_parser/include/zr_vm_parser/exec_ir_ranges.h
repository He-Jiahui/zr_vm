#ifndef ZR_VM_PARSER_EXEC_IR_RANGES_H
#define ZR_VM_PARSER_EXEC_IR_RANGES_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_core/exec_ir.h"

typedef struct SZrExecIrRangeFact {
    TZrExecIrValueId valueId;
    TZrInt64 lower;
    TZrInt64 upper;
    TZrUInt64 generation;
    TZrBool hasLower;
    TZrBool hasUpper;
    TZrBool overflowed;
    TZrBool lengthMutable;
} SZrExecIrRangeFact;

typedef struct SZrExecIrShapeFact {
    TZrExecIrValueId valueId;
    TZrExecIrTypeToken typeToken;
    TZrUInt32 layoutId;
    TZrUInt32 shapeId;
    TZrUInt64 generation;
} SZrExecIrShapeFact;

typedef enum EZrExecIrNullFact {
    ZR_EXEC_IR_NULL_FACT_UNKNOWN = 0,
    ZR_EXEC_IR_NULL_FACT_NONNULL,
    ZR_EXEC_IR_NULL_FACT_NULL
} EZrExecIrNullFact;

typedef struct SZrExecIrNullabilityFact {
    TZrExecIrValueId valueId;
    EZrExecIrNullFact state;
    TZrUInt64 generation;
} SZrExecIrNullabilityFact;

typedef struct SZrExecIrAnalysisFacts {
    SZrExecIrRangeFact *ranges;
    TZrUInt32 rangeCount;
    TZrUInt32 rangeCapacity;
    SZrExecIrShapeFact *shapes;
    TZrUInt32 shapeCount;
    TZrUInt32 shapeCapacity;
    SZrExecIrNullabilityFact *nullability;
    TZrUInt32 nullabilityCount;
    TZrUInt32 nullabilityCapacity;
    TZrUInt64 generation;
} SZrExecIrAnalysisFacts;

ZR_PARSER_API void ZrParser_ExecIr_AnalysisFactsInit(
        SZrExecIrAnalysisFacts *facts);
ZR_PARSER_API void ZrParser_ExecIr_AnalysisFactsFree(
        SZrExecIrAnalysisFacts *facts);
ZR_PARSER_API void ZrParser_ExecIr_AnalysisFacts_InvalidateGeneration(
        SZrExecIrAnalysisFacts *facts, TZrUInt64 generation);
ZR_PARSER_API TZrBool ZrParser_ExecIr_AnalysisFacts_AddRange(
        SZrExecIrAnalysisFacts *facts, const SZrExecIrRangeFact *range);
ZR_PARSER_API TZrBool ZrParser_ExecIr_AnalysisFacts_AddShape(
        SZrExecIrAnalysisFacts *facts, const SZrExecIrShapeFact *shape);
ZR_PARSER_API TZrBool ZrParser_ExecIr_AnalysisFacts_AddNullability(
        SZrExecIrAnalysisFacts *facts,
        const SZrExecIrNullabilityFact *nullability);
ZR_PARSER_API const SZrExecIrNullabilityFact *
ZrParser_ExecIr_AnalysisFacts_FindNullability(
        const SZrExecIrAnalysisFacts *facts, TZrExecIrValueId valueId);
ZR_PARSER_API const SZrExecIrShapeFact *
ZrParser_ExecIr_AnalysisFacts_FindShape(
        const SZrExecIrAnalysisFacts *facts, TZrExecIrValueId valueId);
ZR_PARSER_API TZrBool ZrParser_ExecIr_RangeProvesBounds(
        const SZrExecIrRangeFact *index, const SZrExecIrRangeFact *length);

#endif /* ZR_VM_PARSER_EXEC_IR_RANGES_H */
