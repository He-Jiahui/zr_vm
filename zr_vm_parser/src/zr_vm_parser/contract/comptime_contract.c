#include <string.h>

#include "zr_vm_parser/comptime_contract.h"

TZrBool ZrParser_ComptimeEffect_IsAllowed(
        EZrParserComptimeContext context,
        EZrParserCompileToolEffect effect) {
    switch (context) {
        case ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE:
            return effect == ZR_PARSER_COMPILE_TOOL_EFFECT_PURE_VALUE;
        case ZR_PARSER_COMPTIME_CONTEXT_CHECK:
            return effect == ZR_PARSER_COMPILE_TOOL_EFFECT_PURE_VALUE ||
                   effect == ZR_PARSER_COMPILE_TOOL_EFFECT_DIAGNOSTIC;
        case ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM:
            return effect == ZR_PARSER_COMPILE_TOOL_EFFECT_PURE_VALUE ||
                   effect == ZR_PARSER_COMPILE_TOOL_EFFECT_DIAGNOSTIC ||
                   effect == ZR_PARSER_COMPILE_TOOL_EFFECT_DECLARATION_BUILD;
        case ZR_PARSER_COMPTIME_CONTEXT_SYNTAX_EXPANSION:
            return effect == ZR_PARSER_COMPILE_TOOL_EFFECT_PURE_VALUE ||
                   effect == ZR_PARSER_COMPILE_TOOL_EFFECT_DIAGNOSTIC ||
                   effect == ZR_PARSER_COMPILE_TOOL_EFFECT_SYNTAX_BUILD;
        default:
            return ZR_FALSE;
    }
}

TZrBool ZrParser_ComptimeEffect_IsAllowedAtPhase(
        EZrParserComptimeContext context,
        EZrParserCompilePhase phase,
        EZrParserCompileToolEffect effect) {
    if (!ZrParser_CompilePhase_IsValid(phase) ||
        !ZrParser_ComptimeEffect_IsAllowed(context, effect)) {
        return ZR_FALSE;
    }
    if (context == ZR_PARSER_COMPTIME_CONTEXT_SYNTAX_EXPANSION &&
        phase != ZR_PARSER_COMPILE_PHASE_SYNTAX_EXPANSION) {
        return ZR_FALSE;
    }
    if (effect == ZR_PARSER_COMPILE_TOOL_EFFECT_SYNTAX_BUILD) {
        return context == ZR_PARSER_COMPTIME_CONTEXT_SYNTAX_EXPANSION &&
               phase == ZR_PARSER_COMPILE_PHASE_SYNTAX_EXPANSION;
    }
    if (effect == ZR_PARSER_COMPILE_TOOL_EFFECT_DECLARATION_BUILD) {
        return ZrParser_CompilePhase_IsAtLeast(
                phase, ZR_PARSER_COMPILE_PHASE_SEMANTIC_EXPANSION);
    }
    return ZR_TRUE;
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
        const SZrParserComptimeBudgetLimits *limits,
        SZrParserComptimeBudgetUsage *usageState,
        EZrParserComptimeBudgetResource resource,
        TZrUInt64 **usage,
        const TZrUInt64 **limit) {
    if (limits == ZR_NULL || usageState == ZR_NULL ||
        usage == ZR_NULL || limit == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (resource) {
        case ZR_PARSER_COMPTIME_BUDGET_FUEL:
            *usage = &usageState->fuel;
            *limit = &limits->fuel;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_CALL_DEPTH:
            *usage = &usageState->callDepth;
            *limit = &limits->callDepth;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES:
            *usage = &usageState->heapBytes;
            *limit = &limits->heapBytes;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT:
            *usage = &usageState->aggregateCount;
            *limit = &limits->aggregateCount;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_GENERATED_DECLARATION_COUNT:
            *usage = &usageState->generatedDeclarationCount;
            *limit = &limits->generatedDeclarationCount;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT:
            *usage = &usageState->diagnosticCount;
            *limit = &limits->diagnosticCount;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT:
            *usage = &usageState->tokenCount;
            *limit = &limits->tokenCount;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT:
            *usage = &usageState->nodeCount;
            *limit = &limits->nodeCount;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_MATCHING_COUNT:
            *usage = &usageState->matchingCount;
            *limit = &limits->matchingCount;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_EXPANSION_COUNT:
            *usage = &usageState->expansionCount;
            *limit = &limits->expansionCount;
            return ZR_TRUE;
        case ZR_PARSER_COMPTIME_BUDGET_NONE:
        default:
            return ZR_FALSE;
    }
}

static TZrBool comptime_budget_try_consume_usage(
        const SZrParserComptimeBudgetLimits *limits,
        SZrParserComptimeBudgetUsage *usageState,
        EZrParserComptimeBudgetResource resource,
        TZrUInt64 amount,
        EZrParserComptimeBudgetResource *outExceededResource,
        TZrUInt64 *outExceededLimit,
        TZrUInt64 *outRequestedUsage) {
    TZrUInt64 *usage;
    const TZrUInt64 *limit;
    TZrUInt64 requested;
    const TZrUInt64 maximum = (TZrUInt64)-1;

    if (!comptime_budget_select_resource(
                limits, usageState, resource, &usage, &limit)) {
        return ZR_FALSE;
    }

    requested = *usage > maximum - amount ? maximum : *usage + amount;
    if (amount > *limit || *usage > *limit - amount) {
        if (outExceededResource != ZR_NULL) {
            *outExceededResource = resource;
        }
        if (outExceededLimit != ZR_NULL) {
            *outExceededLimit = *limit;
        }
        if (outRequestedUsage != ZR_NULL) {
            *outRequestedUsage = requested;
        }
        return ZR_FALSE;
    }

    *usage = requested;
    return ZR_TRUE;
}

static TZrBool comptime_budget_add_usage(
        const SZrParserComptimeBudgetLimits *limits,
        const SZrParserComptimeBudgetUsage *base,
        const SZrParserComptimeBudgetUsage *delta,
        SZrParserComptimeBudgetUsage *outUsage,
        EZrParserComptimeBudgetResource *outExceededResource,
        TZrUInt64 *outExceededLimit,
        TZrUInt64 *outRequestedUsage) {
    const TZrUInt64 maximum = (TZrUInt64)-1;

#define ZR_COMPTIME_MERGE_USAGE_FIELD(field, resource) \
    do { \
        if (base->field > maximum - delta->field) { \
            if (outExceededResource != ZR_NULL) { \
                *outExceededResource = resource; \
            } \
            if (outExceededLimit != ZR_NULL) { \
                *outExceededLimit = limits->field; \
            } \
            if (outRequestedUsage != ZR_NULL) { \
                *outRequestedUsage = maximum; \
            } \
            return ZR_FALSE; \
        } \
        outUsage->field = base->field + delta->field; \
    } while (0)

    if (limits == ZR_NULL || base == ZR_NULL || delta == ZR_NULL ||
        outUsage == ZR_NULL) {
        return ZR_FALSE;
    }
    ZR_COMPTIME_MERGE_USAGE_FIELD(
            fuel, ZR_PARSER_COMPTIME_BUDGET_FUEL);
    ZR_COMPTIME_MERGE_USAGE_FIELD(
            callDepth, ZR_PARSER_COMPTIME_BUDGET_CALL_DEPTH);
    ZR_COMPTIME_MERGE_USAGE_FIELD(
            heapBytes, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES);
    ZR_COMPTIME_MERGE_USAGE_FIELD(
            aggregateCount, ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT);
    ZR_COMPTIME_MERGE_USAGE_FIELD(
            generatedDeclarationCount,
            ZR_PARSER_COMPTIME_BUDGET_GENERATED_DECLARATION_COUNT);
    ZR_COMPTIME_MERGE_USAGE_FIELD(
            diagnosticCount, ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT);
    ZR_COMPTIME_MERGE_USAGE_FIELD(
            tokenCount, ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT);
    ZR_COMPTIME_MERGE_USAGE_FIELD(
            nodeCount, ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT);
    ZR_COMPTIME_MERGE_USAGE_FIELD(
            matchingCount, ZR_PARSER_COMPTIME_BUDGET_MATCHING_COUNT);
    ZR_COMPTIME_MERGE_USAGE_FIELD(
            expansionCount, ZR_PARSER_COMPTIME_BUDGET_EXPANSION_COUNT);
#undef ZR_COMPTIME_MERGE_USAGE_FIELD
    return ZR_TRUE;
}

static void comptime_budget_subtract_usage(
        const SZrParserComptimeBudgetUsage *value,
        const SZrParserComptimeBudgetUsage *base,
        SZrParserComptimeBudgetUsage *outDelta) {
    outDelta->fuel = value->fuel - base->fuel;
    outDelta->callDepth = value->callDepth - base->callDepth;
    outDelta->heapBytes = value->heapBytes - base->heapBytes;
    outDelta->aggregateCount = value->aggregateCount - base->aggregateCount;
    outDelta->generatedDeclarationCount =
            value->generatedDeclarationCount - base->generatedDeclarationCount;
    outDelta->diagnosticCount =
            value->diagnosticCount - base->diagnosticCount;
    outDelta->tokenCount = value->tokenCount - base->tokenCount;
    outDelta->nodeCount = value->nodeCount - base->nodeCount;
    outDelta->matchingCount = value->matchingCount - base->matchingCount;
    outDelta->expansionCount = value->expansionCount - base->expansionCount;
}

static TZrBool comptime_budget_usage_within_limits(
        const SZrParserComptimeBudgetLimits *limits,
        const SZrParserComptimeBudgetUsage *usage,
        EZrParserComptimeBudgetResource *outExceededResource,
        TZrUInt64 *outExceededLimit,
        TZrUInt64 *outRequestedUsage) {
    static const EZrParserComptimeBudgetResource resources[] = {
            ZR_PARSER_COMPTIME_BUDGET_FUEL,
            ZR_PARSER_COMPTIME_BUDGET_CALL_DEPTH,
            ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
            ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT,
            ZR_PARSER_COMPTIME_BUDGET_GENERATED_DECLARATION_COUNT,
            ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT,
            ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT,
            ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT,
            ZR_PARSER_COMPTIME_BUDGET_MATCHING_COUNT,
            ZR_PARSER_COMPTIME_BUDGET_EXPANSION_COUNT,
    };

    if (limits == ZR_NULL || usage == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U;
         index < sizeof(resources) / sizeof(resources[0]);
         index++) {
        SZrParserComptimeBudgetUsage candidate = *usage;

        if (!comptime_budget_try_consume_usage(
                    limits,
                    &candidate,
                    resources[index],
                    0U,
                    outExceededResource,
                    outExceededLimit,
                    outRequestedUsage)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ComptimeBudget_TryConsume(
        SZrParserComptimeBudget *budget,
        EZrParserComptimeBudgetResource resource,
        TZrUInt64 amount) {
    SZrParserComptimeBudgetTransaction transaction;

    ZrParser_ComptimeBudget_BeginTransaction(budget, &transaction);
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                &transaction, resource, amount)) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_FALSE;
    }
    return ZrParser_ComptimeBudget_CommitTransaction(&transaction);
}

void ZrParser_ComptimeBudget_BeginTransaction(
        SZrParserComptimeBudget *budget,
        SZrParserComptimeBudgetTransaction *transaction) {
    if (transaction == ZR_NULL) {
        return;
    }
    memset(transaction, 0, sizeof(*transaction));
    if (budget == ZR_NULL) {
        return;
    }
    transaction->budget = budget;
    transaction->usage = budget->usage;
    memset(&transaction->reserved, 0, sizeof(transaction->reserved));
    transaction->active = ZR_TRUE;
}

TZrBool ZrParser_ComptimeBudget_TryConsumeTransaction(
        SZrParserComptimeBudgetTransaction *transaction,
        EZrParserComptimeBudgetResource resource,
        TZrUInt64 amount) {
    if (transaction == ZR_NULL || !transaction->active ||
        transaction->failed || transaction->budget == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!comptime_budget_add_usage(
                &transaction->budget->limits,
                &transaction->budget->usage,
                &transaction->reserved,
                &transaction->usage,
                &transaction->exceededResource,
                &transaction->exceededLimit,
                &transaction->requestedUsage)) {
        transaction->failed = ZR_TRUE;
        return ZR_FALSE;
    }
    if (comptime_budget_try_consume_usage(
                &transaction->budget->limits,
                &transaction->usage,
                resource,
                amount,
                &transaction->exceededResource,
                &transaction->exceededLimit,
                &transaction->requestedUsage)) {
        comptime_budget_subtract_usage(
                &transaction->usage,
                &transaction->budget->usage,
                &transaction->reserved);
        return ZR_TRUE;
    }
    transaction->failed = ZR_TRUE;
    return ZR_FALSE;
}

TZrBool ZrParser_ComptimeBudget_CommitTransaction(
        SZrParserComptimeBudgetTransaction *transaction) {
    SZrParserComptimeBudgetUsage projected;

    if (transaction == ZR_NULL || !transaction->active ||
        transaction->failed || transaction->budget == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!comptime_budget_add_usage(
                &transaction->budget->limits,
                &transaction->budget->usage,
                &transaction->reserved,
                &projected,
                &transaction->exceededResource,
                &transaction->exceededLimit,
                &transaction->requestedUsage)) {
        transaction->failed = ZR_TRUE;
        return ZR_FALSE;
    }
    if (!comptime_budget_usage_within_limits(
                &transaction->budget->limits,
                &projected,
                &transaction->exceededResource,
                &transaction->exceededLimit,
                &transaction->requestedUsage)) {
        transaction->usage = projected;
        transaction->failed = ZR_TRUE;
        return ZR_FALSE;
    }
    transaction->usage = projected;
    transaction->budget->usage = projected;
    transaction->active = ZR_FALSE;
    return ZR_TRUE;
}

void ZrParser_ComptimeBudget_AbortTransaction(
        SZrParserComptimeBudgetTransaction *transaction) {
    if (transaction == ZR_NULL || !transaction->active) {
        return;
    }
    if (transaction->failed && transaction->budget != ZR_NULL &&
        transaction->exceededResource != ZR_PARSER_COMPTIME_BUDGET_NONE) {
        transaction->budget->exceededResource =
                transaction->exceededResource;
        transaction->budget->exceededLimit = transaction->exceededLimit;
        transaction->budget->requestedUsage = transaction->requestedUsage;
    }
    transaction->active = ZR_FALSE;
}
