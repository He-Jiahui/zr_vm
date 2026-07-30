//
// Canonical compile-time evaluation effects and deterministic resource budgets.
//

#ifndef ZR_VM_PARSER_COMPTIME_CONTRACT_H
#define ZR_VM_PARSER_COMPTIME_CONTRACT_H

#include "zr_vm_parser/compile_tool.h"

typedef enum EZrParserComptimeContext {
    ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE = 0,
    ZR_PARSER_COMPTIME_CONTEXT_CHECK = 1,
    ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM = 2
} EZrParserComptimeContext;

typedef enum EZrParserComptimeBudgetResource {
    ZR_PARSER_COMPTIME_BUDGET_NONE = 0,
    ZR_PARSER_COMPTIME_BUDGET_FUEL = 1,
    ZR_PARSER_COMPTIME_BUDGET_CALL_DEPTH = 2,
    ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES = 3,
    ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT = 4,
    ZR_PARSER_COMPTIME_BUDGET_GENERATED_DECLARATION_COUNT = 5,
    ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT = 6
} EZrParserComptimeBudgetResource;

typedef struct SZrParserComptimeBudgetLimits {
    TZrUInt64 fuel;
    TZrUInt64 callDepth;
    TZrUInt64 heapBytes;
    TZrUInt64 aggregateCount;
    TZrUInt64 generatedDeclarationCount;
    TZrUInt64 diagnosticCount;
} SZrParserComptimeBudgetLimits;

typedef SZrParserComptimeBudgetLimits SZrParserComptimeBudgetUsage;

typedef struct SZrParserComptimeBudget {
    SZrParserComptimeBudgetLimits limits;
    SZrParserComptimeBudgetUsage usage;
    EZrParserComptimeBudgetResource exceededResource;
    TZrUInt64 exceededLimit;
    TZrUInt64 requestedUsage;
} SZrParserComptimeBudget;

ZR_PARSER_API TZrBool ZrParser_ComptimeEffect_IsAllowed(
        EZrParserComptimeContext context,
        EZrParserCompileToolEffect effect);
ZR_PARSER_API void ZrParser_ComptimeBudget_Init(
        SZrParserComptimeBudget *budget,
        const SZrParserComptimeBudgetLimits *limits);
ZR_PARSER_API TZrBool ZrParser_ComptimeBudget_TryConsume(
        SZrParserComptimeBudget *budget,
        EZrParserComptimeBudgetResource resource,
        TZrUInt64 amount);

#endif // ZR_VM_PARSER_COMPTIME_CONTRACT_H
