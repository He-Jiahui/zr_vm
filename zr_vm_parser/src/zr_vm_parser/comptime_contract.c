#include <string.h>

#include "zr_vm_parser/comptime_contract.h"

TZrBool ZrParser_ComptimeEffect_IsAllowed(
        EZrParserComptimeContext context,
        EZrParserCompileToolEffect effect) {
    if (effect == ZR_PARSER_COMPILE_TOOL_EFFECT_PURE_VALUE) {
        return context == ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE ||
               context == ZR_PARSER_COMPTIME_CONTEXT_CHECK ||
               context == ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM;
    }
    if (effect == ZR_PARSER_COMPILE_TOOL_EFFECT_DIAGNOSTIC) {
        return context == ZR_PARSER_COMPTIME_CONTEXT_CHECK ||
               context == ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM;
    }
    if (effect == ZR_PARSER_COMPILE_TOOL_EFFECT_DECLARATION_BUILD) {
        return context == ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM;
    }
    return ZR_FALSE;
}

void ZrParser_ComptimeBudget_Init(
        SZrParserComptimeBudget *budget,
        const SZrParserComptimeBudgetLimits *limits) {
    if (budget == ZR_NULL) {
        return;
    }
    memset(budget, 0, sizeof(*budget));
    if (limits != ZR_NULL) {
        budget->limits = *limits;
    }
}

static TZrBool comptime_budget_select_resource(
        SZrParserComptimeBudget *budget,
        EZrParserComptimeBudgetResource resource,
        TZrUInt64 **usage,
        TZrUInt64 **limit) {
    if (budget == ZR_NULL || usage == ZR_NULL || limit == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (resource) {
        case ZR_PARSER_COMPTIME_BUDGET_FUEL:
            *usage = &budget->usage.fuel;
            *limit = &budget->limits.fuel;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_CALL_DEPTH:
            *usage = &budget->usage.callDepth;
            *limit = &budget->limits.callDepth;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES:
            *usage = &budget->usage.heapBytes;
            *limit = &budget->limits.heapBytes;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT:
            *usage = &budget->usage.aggregateCount;
            *limit = &budget->limits.aggregateCount;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_GENERATED_DECLARATION_COUNT:
            *usage = &budget->usage.generatedDeclarationCount;
            *limit = &budget->limits.generatedDeclarationCount;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT:
            *usage = &budget->usage.diagnosticCount;
            *limit = &budget->limits.diagnosticCount;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_NONE:
        default:
            return ZR_FALSE;
    }
}

TZrBool ZrParser_ComptimeBudget_TryConsume(
        SZrParserComptimeBudget *budget,
        EZrParserComptimeBudgetResource resource,
        TZrUInt64 amount) {
    TZrUInt64 *usage;
    TZrUInt64 *limit;
    TZrUInt64 requested;
    const TZrUInt64 maximum = (TZrUInt64)-1;

    if (!comptime_budget_select_resource(budget, resource, &usage, &limit)) {
        return ZR_FALSE;
    }

    requested = *usage > maximum - amount ? maximum : *usage + amount;
    if (amount > *limit || *usage > *limit - amount) {
        budget->exceededResource = resource;
        budget->exceededLimit = *limit;
        budget->requestedUsage = requested;
        return ZR_FALSE;
    }

    *usage = requested;
    return ZR_TRUE;
}
