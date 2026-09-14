#include "zr_vm_parser/exec_ir_escape.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/exec_ir_state_map.h"

/*
 * Allocation placement is intentionally represented as a separate plan.
 * ExecIR currently has no stack/region allocation opcode, and changing an
 * ALLOC's flags would make the core effect verifier lie about allocation and
 * GC behaviour.  This pass therefore computes and validates a placement
 * witness; frame/GC lowering can consume it at a representation boundary.
 */

static void zr_allocation_diag(SZrExecIrDiagnostic *diagnostic,
                               EZrExecutionDiagnosticCode code,
                               const SZrExecIrFunction *function,
                               TZrExecIrInstructionId instructionId,
                               TZrUInt32 sourceId) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->instructionId = instructionId;
    diagnostic->sourceId = sourceId;
}
static void zr_allocation_hash_diag(SZrExecIrDiagnostic *diagnostic,
                                    const SZrExecIrFunction *function,
                                    TZrUInt64 expected,
                                    TZrUInt64 actual) {
    zr_allocation_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION,
                       function, 0u, 0u);
    if (diagnostic != ZR_NULL) {
        diagnostic->expectedHash = expected;
        diagnostic->actualHash = actual;
    }
}

static TZrBool zr_allocation_range_valid(SZrExecIrRange range,
                                         TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool zr_allocation_size_valid(TZrUInt32 count, size_t elementSize) {
    return (TZrBool)(elementSize != 0u &&
                     (size_t)count <= SIZE_MAX / elementSize);
}

static TZrBool zr_allocation_plan_storage_valid(
        const SZrExecIrAllocationPlan *plan) {
    TZrUInt32 magic = 0u;
    TZrUInt32 count = 0u;
    TZrUInt32 capacity = 0u;
    SZrExecIrAllocationSite *sites = ZR_NULL;

    if (plan == ZR_NULL) return ZR_FALSE;
    /* Copy object representations into initialized locals.  This keeps an
     * Init/Free call on zeroed or otherwise uninitialised caller storage from
     * evaluating indeterminate scalar/pointer values directly. */
    memcpy(&magic, &plan->magic, sizeof(magic));
    memcpy(&count, &plan->siteCount, sizeof(count));
    memcpy(&capacity, &plan->siteCapacity, sizeof(capacity));
    memcpy(&sites, &plan->sites, sizeof(sites));
    if (magic != ZR_EXEC_IR_ALLOCATION_PLAN_MAGIC || count > capacity) {
        return ZR_FALSE;
    }
    return (TZrBool)((capacity == 0u && sites == ZR_NULL) ||
                     (capacity != 0u && sites != ZR_NULL));
}

static void zr_allocation_plan_release(SZrExecIrAllocationPlan *plan) {
    SZrExecIrAllocationSite *sites = ZR_NULL;
    if (!zr_allocation_plan_storage_valid(plan)) return;
    memcpy(&sites, &plan->sites, sizeof(sites));
    free(sites);
}

void ZrParser_ExecIr_AllocationPlanInit(SZrExecIrAllocationPlan *plan) {
    if (plan == ZR_NULL) return;
    zr_allocation_plan_release(plan);
    memset(plan, 0, sizeof(*plan));
    plan->magic = ZR_EXEC_IR_ALLOCATION_PLAN_MAGIC;
}

void ZrParser_ExecIr_AllocationPlanFree(SZrExecIrAllocationPlan *plan) {
    if (plan == ZR_NULL) return;
    zr_allocation_plan_release(plan);
    memset(plan, 0, sizeof(*plan));
}

const TZrChar *ZrParser_ExecIr_AllocationReasonName(
        EZrExecIrAllocationReason reason) {
    switch (reason) {
        case ZR_EXEC_IR_ALLOCATION_REASON_PROVEN_LOCAL:
            return "proven-local";
        case ZR_EXEC_IR_ALLOCATION_REASON_ESCAPE:
            return "escape";
        case ZR_EXEC_IR_ALLOCATION_REASON_IDENTITY_OBSERVED:
            return "identity-observed";
        case ZR_EXEC_IR_ALLOCATION_REASON_CROSSED_SUSPEND:
            return "escape-across-suspend";
        case ZR_EXEC_IR_ALLOCATION_REASON_NATIVE_RETAINED:
            return "native-retained";
        case ZR_EXEC_IR_ALLOCATION_REASON_DROP_OBSERVABLE:
            return "drop-observable";
        case ZR_EXEC_IR_ALLOCATION_REASON_UNKNOWN_ESCAPE:
            return "unknown-escape";
        case ZR_EXEC_IR_ALLOCATION_REASON_NO_LAYOUT:
            return "layout-unproven";
        case ZR_EXEC_IR_ALLOCATION_REASON_NOT_CANDIDATE:
            return "not-candidate";
        case ZR_EXEC_IR_ALLOCATION_REASON_MATERIALIZATION:
            return "materialization-required";
        case ZR_EXEC_IR_ALLOCATION_REASON_INVALID:
        case ZR_EXEC_IR_ALLOCATION_REASON_COUNT:
        default:
            return "invalid";
    }
}

static TZrBool zr_allocation_summary_valid(
        const SZrExecIrFunction *function,
        const SZrExecIrEscapeSummary *summary,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 magic = 0u;
    TZrUInt32 factCount = 0u;
    TZrUInt32 factCapacity = 0u;
    SZrExecIrEscapeFact *facts = ZR_NULL;
    TZrUInt64 summaryHash = 0u;
    TZrUInt64 inputHash;

    if (function == ZR_NULL || summary == ZR_NULL) {
        zr_allocation_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                           function, 0u, 0u);
        return ZR_FALSE;
    }
    memcpy(&magic, &summary->magic, sizeof(magic));
    memcpy(&factCount, &summary->factCount, sizeof(factCount));
    memcpy(&factCapacity, &summary->factCapacity, sizeof(factCapacity));
    memcpy(&facts, &summary->facts, sizeof(facts));
    memcpy(&summaryHash, &summary->irHash, sizeof(summaryHash));
    if (magic != ZR_EXEC_IR_ESCAPE_SUMMARY_MAGIC ||
        factCount > factCapacity || factCount != function->valueCount ||
        (factCount != 0u && facts == ZR_NULL)) {
        zr_allocation_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                           function, 0u, 0u);
        return ZR_FALSE;
    }
    inputHash = ZrParser_ExecIr_EscapeInputHash(function);
    if (inputHash == 0u || summaryHash != inputHash) {
        zr_allocation_hash_diag(diagnostic, function, inputHash, summaryHash);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_allocation_state_map_valid(
        const SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrStateMap *map;
    TZrUInt32 index;

    if (function == ZR_NULL) return ZR_FALSE;
    map = function->stateMap;
    if (map == ZR_NULL) return ZR_TRUE;
    if ((map->entryCount != 0u && map->entries == ZR_NULL) ||
        (map->valueCount != 0u && map->valuePool == ZR_NULL) ||
        (map->rootCount != 0u && map->rootPool == ZR_NULL) ||
        (map->ownerStateCount != 0u && map->ownerStatePool == ZR_NULL) ||
        map->entryCount > map->entryCapacity ||
        map->valueCount > map->valueCapacity ||
        map->rootCount > map->rootCapacity ||
        map->ownerStateCount > map->ownerStateCapacity) {
        zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                           function, 0u, 0u);
        return ZR_FALSE;
    }
    for (index = 0u; index < map->entryCount; ++index) {
        const SZrExecIrStateMapEntry *entry = &map->entries[index];
        if (!zr_allocation_range_valid(entry->liveValues, map->valueCount) ||
            !zr_allocation_range_valid(entry->rootValues, map->rootCount) ||
            !zr_allocation_range_valid(entry->ownerStates,
                                       map->ownerStateCount)) {
            zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                               function, entry->instructionId, entry->sourceId);
            return ZR_FALSE;
        }
        if (entry->instructionId != 0u &&
            entry->instructionId > function->instructionCount) {
            zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                               function, entry->instructionId, entry->sourceId);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_allocation_value_in_metadata(
        const SZrExecIrFunction *function, TZrExecIrValueId valueId,
        TZrBool *requiresMaterialization, SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (requiresMaterialization != ZR_NULL) *requiresMaterialization = ZR_FALSE;
    if (function == ZR_NULL || valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
        valueId > function->valueCount) {
        zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                           function, 0u, 0u);
        return ZR_FALSE;
    }
    if ((function->gcRootCount != 0u && function->gcRoots == ZR_NULL) ||
        (function->deoptValueCount != 0u && function->deoptValues == ZR_NULL) ||
        function->deoptStateCount > function->deoptStateCapacity ||
        (function->deoptStateCount != 0u && function->deoptStates == ZR_NULL)) {
        zr_allocation_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                           function, 0u, 0u);
        return ZR_FALSE;
    }
    for (index = 0u; index < function->gcRootCount; ++index) {
        if (function->gcRoots[index] == valueId) {
            if (requiresMaterialization != ZR_NULL) *requiresMaterialization = ZR_TRUE;
            return ZR_TRUE;
        }
    }
    for (index = 0u; index < function->deoptValueCount; ++index) {
        if (function->deoptValues[index] == valueId) {
            if (requiresMaterialization != ZR_NULL) *requiresMaterialization = ZR_TRUE;
            return ZR_TRUE;
        }
    }
    for (index = 0u; index < function->deoptStateCount; ++index) {
        const SZrExecIrDeoptState *state = &function->deoptStates[index];
        if (!zr_allocation_range_valid(state->reconstruction,
                                       function->deoptValueCount)) {
            zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                               function, 0u, 0u);
            return ZR_FALSE;
        }
        {
            TZrUInt32 at;
            for (at = state->reconstruction.start;
                 at < state->reconstruction.start + state->reconstruction.count;
                 ++at) {
                if (function->deoptValues[at] == valueId) {
                    if (requiresMaterialization != ZR_NULL) {
                        *requiresMaterialization = ZR_TRUE;
                    }
                    return ZR_TRUE;
                }
            }
        }
    }
    if (function->stateMap != ZR_NULL) {
        const SZrExecIrStateMap *map = function->stateMap;
        for (index = 0u; index < map->entryCount; ++index) {
            const SZrExecIrStateMapEntry *entry = &map->entries[index];
            TZrUInt32 at;
            for (at = entry->liveValues.start;
                 at < entry->liveValues.start + entry->liveValues.count; ++at) {
                if (map->valuePool[at] == valueId) {
                    if (requiresMaterialization != ZR_NULL) {
                        *requiresMaterialization = ZR_TRUE;
                    }
                    return ZR_TRUE;
                }
            }
            for (at = entry->rootValues.start;
                 at < entry->rootValues.start + entry->rootValues.count; ++at) {
                if (map->rootPool[at] == valueId) {
                    if (requiresMaterialization != ZR_NULL) {
                        *requiresMaterialization = ZR_TRUE;
                    }
                    return ZR_TRUE;
                }
            }
        }
    }
    return ZR_TRUE;
}

static EZrExecIrAllocationReason zr_allocation_reason(
        const SZrExecIrEscapeFact *fact,
        const SZrExecIrFunction *function) {
    const SZrExecIrInstruction *instruction = ZR_NULL;
    if (fact == ZR_NULL) return ZR_EXEC_IR_ALLOCATION_REASON_INVALID;
    if (fact->decision == ZR_EXEC_IR_ALLOC_STACK) {
        return ZR_EXEC_IR_ALLOCATION_REASON_PROVEN_LOCAL;
    }
    if (fact->crossesSuspend) {
        return ZR_EXEC_IR_ALLOCATION_REASON_CROSSED_SUSPEND;
    }
    if (fact->nativeRetained) {
        return ZR_EXEC_IR_ALLOCATION_REASON_NATIVE_RETAINED;
    }
    if (fact->dropObservable) {
        return ZR_EXEC_IR_ALLOCATION_REASON_DROP_OBSERVABLE;
    }
    if (fact->identityObserved) {
        return ZR_EXEC_IR_ALLOCATION_REASON_IDENTITY_OBSERVED;
    }
    if (fact->state == ZR_EXEC_IR_ESCAPE_UNKNOWN) {
        return ZR_EXEC_IR_ALLOCATION_REASON_UNKNOWN_ESCAPE;
    }
    if (fact->isAllocationCandidate && function != ZR_NULL &&
        fact->allocationInstructionId != 0u &&
        fact->allocationInstructionId <= function->instructionCount) {
        instruction = &function->instructions[fact->allocationInstructionId - 1u];
        if (instruction->layoutId == 0u || instruction->layoutId == UINT32_MAX) {
            return ZR_EXEC_IR_ALLOCATION_REASON_NO_LAYOUT;
        }
    }
    if (!fact->isAllocationCandidate) {
        return ZR_EXEC_IR_ALLOCATION_REASON_NOT_CANDIDATE;
    }
    return ZR_EXEC_IR_ALLOCATION_REASON_ESCAPE;
}

static TZrBool zr_allocation_append(SZrExecIrAllocationPlan *plan,
                                    const SZrExecIrAllocationSite *site,
                                    SZrExecIrDiagnostic *diagnostic,
                                    const SZrExecIrFunction *function) {
    SZrExecIrAllocationSite *sites;
    TZrUInt32 capacity;
    if (plan == ZR_NULL || site == ZR_NULL || plan->siteCount == UINT32_MAX) {
        zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                           function, site != ZR_NULL ? site->instructionId : 0u,
                           0u);
        return ZR_FALSE;
    }
    if (plan->siteCount == plan->siteCapacity) {
        capacity = plan->siteCapacity == 0u ? 8u : plan->siteCapacity;
        if (capacity > UINT32_MAX / 2u) {
            capacity = UINT32_MAX;
        } else {
            capacity *= 2u;
        }
        if (!zr_allocation_size_valid(capacity, sizeof(*sites))) {
            zr_allocation_diag(diagnostic,
                               ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                               function, site->instructionId, 0u);
            return ZR_FALSE;
        }
        sites = (SZrExecIrAllocationSite *)realloc(
                plan->sites, (size_t)capacity * sizeof(*sites));
        if (sites == ZR_NULL) {
            zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                               function, site->instructionId, 0u);
            return ZR_FALSE;
        }
        plan->sites = sites;
        plan->siteCapacity = capacity;
    }
    plan->sites[plan->siteCount++] = *site;
    switch (site->decision) {
        case ZR_EXEC_IR_ALLOC_STACK: ++plan->stackCount; break;
        case ZR_EXEC_IR_ALLOC_REGION: ++plan->regionCount; break;
        case ZR_EXEC_IR_ALLOC_HEAP:
        default: ++plan->heapCount; break;
    }
    if (site->reason == ZR_EXEC_IR_ALLOCATION_REASON_UNKNOWN_ESCAPE) {
        plan->hasUnknownEscape = ZR_TRUE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_BuildAllocationPlan(
        const SZrExecIrFunction *function,
        const SZrExecIrEscapeSummary *summary,
        SZrExecIrAllocationPlan *plan,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrAllocationPlan candidate;
    TZrUInt32 index;

    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (function == ZR_NULL || plan == ZR_NULL ||
        !ZrCore_ExecIr_VerifyFunction(
                function,
                (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                       ZR_EXEC_IR_VERIFY_SSA),
                diagnostic) ||
        !zr_allocation_summary_valid(function, summary, diagnostic) ||
        !zr_allocation_state_map_valid(function, diagnostic)) {
        return ZR_FALSE;
    }
    memset(&candidate, 0, sizeof(candidate));
    ZrParser_ExecIr_AllocationPlanInit(&candidate);
    for (index = 0u; index < summary->factCount; ++index) {
        const SZrExecIrEscapeFact *fact = &summary->facts[index];
        SZrExecIrAllocationSite site;
        TZrBool materialization = ZR_FALSE;
        TZrExecIrInstructionId instructionId;
        const SZrExecIrInstruction *instruction;
        TZrUInt32 resultIndex;
        TZrBool foundResult = ZR_FALSE;

        if (!fact->isAllocationCandidate) continue;
        instructionId = fact->allocationInstructionId;
        if (instructionId == 0u || instructionId > function->instructionCount ||
            function->instructions == ZR_NULL) {
            zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                               function, instructionId, 0u);
            goto fail;
        }
        instruction = &function->instructions[instructionId - 1u];
        if ((EZrExecIrOpcode)instruction->opcode != ZR_EXEC_IR_OPCODE_ALLOC ||
            !zr_allocation_range_valid(instruction->results,
                                       function->resultCount) ||
            instruction->results.count == 0u || function->results == ZR_NULL) {
            zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                               function, instructionId, instruction->sourceId);
            goto fail;
        }
        for (resultIndex = instruction->results.start;
             resultIndex < instruction->results.start + instruction->results.count;
             ++resultIndex) {
            if (function->results[resultIndex] == fact->valueId) {
                foundResult = ZR_TRUE;
                break;
            }
        }
        if (!foundResult || fact->valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
            fact->valueId > function->valueCount ||
            !zr_allocation_value_in_metadata(function, fact->valueId,
                                              &materialization, diagnostic)) {
            if (diagnostic != ZR_NULL && diagnostic->code == ZR_EXECUTION_DIAGNOSTIC_NONE) {
                zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                                   function, instructionId, instruction->sourceId);
            }
            goto fail;
        }
        memset(&site, 0, sizeof(site));
        site.valueId = fact->valueId;
        site.instructionId = instructionId;
        site.decision = fact->decision;
        site.reason = zr_allocation_reason(fact, function);
        site.liveStart = fact->liveStart;
        site.liveEnd = fact->liveEnd;
        site.requiresGcRoot = (TZrBool)(function->values[fact->valueId - 1u].ownership ==
                                        ZR_EXEC_IR_OWNERSHIP_GC);
        site.requiresMaterialization = materialization;
        site.identityPreserved = ZR_TRUE;
        if (!zr_allocation_append(&candidate, &site, diagnostic, function)) {
            goto fail;
        }
    }
    candidate.irHash = summary->irHash;
    ZrParser_ExecIr_AllocationPlanFree(plan);
    *plan = candidate;
    return ZR_TRUE;

fail:
    ZrParser_ExecIr_AllocationPlanFree(&candidate);
    return ZR_FALSE;
}

TZrBool ZrParser_ExecIr_AnalyzeAllocation(
        const SZrExecIrFunction *function,
        const SZrExecIrEscapeSummary *summary,
        SZrExecIrAllocationPlan *plan,
        SZrExecIrDiagnostic *diagnostic) {
    return ZrParser_ExecIr_BuildAllocationPlan(function, summary, plan,
                                               diagnostic);
}

static TZrBool zr_allocation_site_matches(
        const SZrExecIrFunction *function,
        const SZrExecIrAllocationSite *site,
        SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrInstruction *instruction;
    TZrUInt32 index;
    TZrBool found = ZR_FALSE;
    if (function == ZR_NULL || site == ZR_NULL ||
        site->valueId == ZR_EXEC_IR_VALUE_ID_INVALID ||
        site->valueId > function->valueCount || site->instructionId == 0u ||
        site->instructionId > function->instructionCount ||
        site->decision < ZR_EXEC_IR_ALLOC_HEAP ||
        site->decision > ZR_EXEC_IR_ALLOC_REGION ||
        site->reason >= ZR_EXEC_IR_ALLOCATION_REASON_COUNT) {
        zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                           function, site != ZR_NULL ? site->instructionId : 0u,
                           0u);
        return ZR_FALSE;
    }
    instruction = &function->instructions[site->instructionId - 1u];
    if ((EZrExecIrOpcode)instruction->opcode != ZR_EXEC_IR_OPCODE_ALLOC ||
        !zr_allocation_range_valid(instruction->results, function->resultCount) ||
        instruction->results.count == 0u || function->results == ZR_NULL) {
        zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                           function, site->instructionId, instruction->sourceId);
        return ZR_FALSE;
    }
    for (index = instruction->results.start;
         index < instruction->results.start + instruction->results.count; ++index) {
        if (function->results[index] == site->valueId) {
            found = ZR_TRUE;
            break;
        }
    }
    if (!found) {
        zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                           function, site->instructionId, instruction->sourceId);
        return ZR_FALSE;
    }
    if (site->decision == ZR_EXEC_IR_ALLOC_STACK &&
        (function->values[site->valueId - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_GC ||
         instruction->layoutId == 0u || instruction->layoutId == UINT32_MAX ||
         site->reason != ZR_EXEC_IR_ALLOCATION_REASON_PROVEN_LOCAL)) {
        zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                           function, site->instructionId, instruction->sourceId);
        return ZR_FALSE;
    }
    if (site->requiresGcRoot &&
        function->values[site->valueId - 1u].ownership != ZR_EXEC_IR_OWNERSHIP_GC) {
        zr_allocation_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                           function, site->instructionId, instruction->sourceId);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_ApplyAllocationPlan(
        SZrExecIrFunction *function,
        const SZrExecIrAllocationPlan *plan,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt32 heapCount = 0u;
    TZrUInt32 stackCount = 0u;
    TZrUInt32 regionCount = 0u;
    TZrUInt64 inputHash;
    TZrUInt64 planHash = 0u;
    TZrUInt32 magic = 0u;

    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (function == ZR_NULL || plan == ZR_NULL ||
        !ZrCore_ExecIr_VerifyFunction(
                function,
                (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                       ZR_EXEC_IR_VERIFY_SSA),
                diagnostic) || !zr_allocation_plan_storage_valid(plan)) {
        zr_allocation_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                           function, 0u, 0u);
        return ZR_FALSE;
    }
    memcpy(&magic, &plan->magic, sizeof(magic));
    memcpy(&planHash, &plan->irHash, sizeof(planHash));
    if (magic != ZR_EXEC_IR_ALLOCATION_PLAN_MAGIC) {
        zr_allocation_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                           function, 0u, 0u);
        return ZR_FALSE;
    }
    inputHash = ZrParser_ExecIr_EscapeInputHash(function);
    if (inputHash == 0u || inputHash != planHash) {
        zr_allocation_hash_diag(diagnostic, function, inputHash, planHash);
        return ZR_FALSE;
    }
    for (index = 0u; index < plan->siteCount; ++index) {
        const SZrExecIrAllocationSite *site = &plan->sites[index];
        if (!zr_allocation_site_matches(function, site, diagnostic)) return ZR_FALSE;
        switch (site->decision) {
            case ZR_EXEC_IR_ALLOC_STACK: ++stackCount; break;
            case ZR_EXEC_IR_ALLOC_REGION: ++regionCount; break;
            case ZR_EXEC_IR_ALLOC_HEAP:
            default: ++heapCount; break;
        }
    }
    if (heapCount != plan->heapCount || stackCount != plan->stackCount ||
        regionCount != plan->regionCount) {
        zr_allocation_diag(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                           function, 0u, 0u);
        return ZR_FALSE;
    }
    /* No instruction is rewritten here: changing ALLOC to an ad-hoc opcode or
     * dropping MAY_GC would invalidate the core effect contract.  Successful
     * validation is the publication point for a backend-owned placement plan.
     */
    return ZR_TRUE;
}
