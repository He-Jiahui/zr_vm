//
// Canonical compile-time evaluation effects and deterministic resource budgets.
//

#ifndef ZR_VM_PARSER_COMPTIME_CONTRACT_H
#define ZR_VM_PARSER_COMPTIME_CONTRACT_H

#include "zr_vm_parser/compile_tool.h"

typedef enum EZrParserComptimeContext {
    ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE = 0,
    ZR_PARSER_COMPTIME_CONTEXT_CHECK = 1,
    ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM = 2,
    /* Appended so existing cache/context IDs remain stable. */
    ZR_PARSER_COMPTIME_CONTEXT_SYNTAX_EXPANSION = 3
} EZrParserComptimeContext;

typedef enum EZrParserComptimeBudgetResource {
    ZR_PARSER_COMPTIME_BUDGET_NONE = 0,
    ZR_PARSER_COMPTIME_BUDGET_FUEL = 1,
    ZR_PARSER_COMPTIME_BUDGET_CALL_DEPTH = 2,
    ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES = 3,
    ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT = 4,
    ZR_PARSER_COMPTIME_BUDGET_GENERATED_DECLARATION_COUNT = 5,
    ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT = 6,
    ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT = 7,
    ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT = 8,
    ZR_PARSER_COMPTIME_BUDGET_MATCHING_COUNT = 9,
    ZR_PARSER_COMPTIME_BUDGET_EXPANSION_COUNT = 10
} EZrParserComptimeBudgetResource;

#define ZR_PARSER_COMPTIME_DEFAULT_FUEL ((TZrUInt64)1000000U)
#define ZR_PARSER_COMPTIME_DEFAULT_CALL_DEPTH ((TZrUInt64)128U)
#define ZR_PARSER_COMPTIME_DEFAULT_HEAP_BYTES \
    ((TZrUInt64)64U * (TZrUInt64)1024U * (TZrUInt64)1024U)
#define ZR_PARSER_COMPTIME_DEFAULT_AGGREGATE_COUNT ((TZrUInt64)100000U)
#define ZR_PARSER_COMPTIME_DEFAULT_GENERATED_DECLARATION_COUNT \
    ((TZrUInt64)10000U)
#define ZR_PARSER_COMPTIME_DEFAULT_DIAGNOSTIC_COUNT ((TZrUInt64)1024U)
/* Syntax work is bounded in the same units as the existing evaluation work. */
#define ZR_PARSER_COMPTIME_DEFAULT_TOKEN_COUNT ((TZrUInt64)1000000U)
#define ZR_PARSER_COMPTIME_DEFAULT_NODE_COUNT ((TZrUInt64)1000000U)
#define ZR_PARSER_COMPTIME_DEFAULT_MATCHING_COUNT ((TZrUInt64)100000U)
#define ZR_PARSER_COMPTIME_DEFAULT_EXPANSION_COUNT ((TZrUInt64)10000U)

typedef struct SZrParserComptimeBudgetLimits {
    TZrUInt64 fuel;
    TZrUInt64 callDepth;
    TZrUInt64 heapBytes;
    TZrUInt64 aggregateCount;
    TZrUInt64 generatedDeclarationCount;
    TZrUInt64 diagnosticCount;
    TZrUInt64 tokenCount;
    TZrUInt64 nodeCount;
    TZrUInt64 matchingCount;
    TZrUInt64 expansionCount;
} SZrParserComptimeBudgetLimits;

typedef SZrParserComptimeBudgetLimits SZrParserComptimeBudgetUsage;

typedef struct SZrParserComptimeBudget {
    SZrParserComptimeBudgetLimits limits;
    SZrParserComptimeBudgetUsage usage;
    EZrParserComptimeBudgetResource exceededResource;
    TZrUInt64 exceededLimit;
    TZrUInt64 requestedUsage;
} SZrParserComptimeBudget;

/*
 * Resource reservation is shadowed until commit. This lets a syntax
 * expansion charge token/node/matching resources as one operation and abort
 * without leaving a partially consumed budget behind.
 */
typedef struct SZrParserComptimeBudgetTransaction {
    SZrParserComptimeBudget *budget;
    /* Projected usage is refreshed from the live budget at each operation. */
    SZrParserComptimeBudgetUsage usage;
    /* Delta reserved by this transaction; commit merges this delta. */
    SZrParserComptimeBudgetUsage reserved;
    EZrParserComptimeBudgetResource exceededResource;
    TZrUInt64 exceededLimit;
    TZrUInt64 requestedUsage;
    TZrBool active;
    TZrBool failed;
} SZrParserComptimeBudgetTransaction;

ZR_PARSER_API TZrBool ZrParser_ComptimeEffect_IsAllowed(
        EZrParserComptimeContext context,
        EZrParserCompileToolEffect effect);
ZR_PARSER_API TZrBool ZrParser_ComptimeEffect_IsAllowedAtPhase(
        EZrParserComptimeContext context,
        EZrParserCompilePhase phase,
        EZrParserCompileToolEffect effect);
ZR_PARSER_API void ZrParser_ComptimeBudget_Init(
        SZrParserComptimeBudget *budget,
        const SZrParserComptimeBudgetLimits *limits);
ZR_PARSER_API TZrBool ZrParser_ComptimeBudget_TryConsume(
        SZrParserComptimeBudget *budget,
        EZrParserComptimeBudgetResource resource,
        TZrUInt64 amount);
ZR_PARSER_API void ZrParser_ComptimeBudget_BeginTransaction(
        SZrParserComptimeBudget *budget,
        SZrParserComptimeBudgetTransaction *transaction);
ZR_PARSER_API TZrBool ZrParser_ComptimeBudget_TryConsumeTransaction(
        SZrParserComptimeBudgetTransaction *transaction,
        EZrParserComptimeBudgetResource resource,
        TZrUInt64 amount);
ZR_PARSER_API TZrBool ZrParser_ComptimeBudget_CommitTransaction(
        SZrParserComptimeBudgetTransaction *transaction);
ZR_PARSER_API void ZrParser_ComptimeBudget_AbortTransaction(
        SZrParserComptimeBudgetTransaction *transaction);

#endif // ZR_VM_PARSER_COMPTIME_CONTRACT_H
