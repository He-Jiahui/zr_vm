#include "zr_vm_parser/optimization_remarks.h"

#include <string.h>

#include "zr_vm_parser/exec_ir_escape.h"
#include "zr_vm_parser/exec_ir_loops.h"
#include "zr_vm_parser/exec_ir_vectorize.h"

/* The pass-manager reason values are deliberately not reused as the public
 * core enum: producers may add pass-local values (LICM currently prefixes
 * loop reasons with 100).  Unknown values are rejected instead of being
 * silently downgraded to `none`, which would make an explanation claim more
 * knowledge than the producer supplied. */
static TZrBool zr_parser_remark_reason(
        const TZrChar *pass, TZrUInt32 reason,
        EZrOptimizationRemarkReason *mapped) {
    if (mapped == ZR_NULL) return ZR_FALSE;

    /* Vectorization publishes its own reason enum into the shared ExecIR
     * sink.  Those values intentionally overlap the pass-manager enum, so
     * identify the producer before applying the generic mapping.  Keep the
     * public schema small while retaining the useful category (alias,
     * bounds, effect/order, budget, target, or vector) for each producer
     * reason.  Unknown/future vectorizer values remain a hard diagnostic
     * rather than being mislabeled as a generic pass-manager reason. */
    if (pass != ZR_NULL &&
        (strcmp(pass, "vectorize") == 0 ||
         strcmp(pass, "vectorizer") == 0 ||
         strcmp(pass, "exec_ir_vectorize") == 0)) {
        switch ((EZrExecIrVectorizeReason)reason) {
            case ZR_EXEC_IR_VECTORIZER_REASON_NONE:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_NONE;
                return ZR_TRUE;
            case ZR_EXEC_IR_VECTORIZER_REASON_DISABLED:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_CAPABILITY_DENIED;
                return ZR_TRUE;
            case ZR_EXEC_IR_VECTORIZER_REASON_INVALID_LOOP:
            case ZR_EXEC_IR_VECTORIZER_REASON_NO_PREHEADER:
            case ZR_EXEC_IR_VECTORIZER_REASON_MULTIPLE_ENTRY:
            case ZR_EXEC_IR_VECTORIZER_REASON_IRREDUCIBLE:
            case ZR_EXEC_IR_VECTORIZER_REASON_ZERO_TRIP:
            case ZR_EXEC_IR_VECTORIZER_REASON_UNKNOWN_TRIP:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_BOUNDS_UNKNOWN;
                return ZR_TRUE;
            case ZR_EXEC_IR_VECTORIZER_REASON_SMALL_TRIP:
            case ZR_EXEC_IR_VECTORIZER_REASON_COST:
            case ZR_EXEC_IR_VECTORIZER_REASON_CODE_SIZE:
            case ZR_EXEC_IR_VECTORIZER_REASON_BRIDGE_BUDGET:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_CODE_BUDGET;
                return ZR_TRUE;
            case ZR_EXEC_IR_VECTORIZER_REASON_DEPENDENCE:
            case ZR_EXEC_IR_VECTORIZER_REASON_EFFECT:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_EFFECT_ORDER;
                return ZR_TRUE;
            case ZR_EXEC_IR_VECTORIZER_REASON_ALIAS_UNSAFE:
            case ZR_EXEC_IR_VECTORIZER_REASON_ALIAS_UNKNOWN:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_ALIAS_UNKNOWN;
                return ZR_TRUE;
            case ZR_EXEC_IR_VECTORIZER_REASON_BOUNDS_UNKNOWN:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_BOUNDS_UNKNOWN;
                return ZR_TRUE;
            case ZR_EXEC_IR_VECTORIZER_REASON_STRIDE_UNKNOWN:
            case ZR_EXEC_IR_VECTORIZER_REASON_ALIGNMENT:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_LAYOUT;
                return ZR_TRUE;
            case ZR_EXEC_IR_VECTORIZER_REASON_UNSUPPORTED_OPCODE:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_TARGET_UNSUPPORTED;
                return ZR_TRUE;
            case ZR_EXEC_IR_VECTORIZER_REASON_NUMERIC_POLICY:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_CAPABILITY_DENIED;
                return ZR_TRUE;
            case ZR_EXEC_IR_VECTORIZER_REASON_TAIL:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_VECTOR;
                return ZR_TRUE;
            case ZR_EXEC_IR_VECTORIZER_REASON_VERSION:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_PROFILE_STALE;
                return ZR_TRUE;
            case ZR_EXEC_IR_VECTORIZER_REASON_INVALID:
            case ZR_EXEC_IR_VECTORIZER_REASON_COUNT:
            default:
                return ZR_FALSE;
        }
    }

    /* Allocation/escape analysis may publish a side-table row through the
     * same sink in a later pipeline.  Its enum also starts at zero, hence the
     * pass-qualified adapter. */
    if (pass != ZR_NULL &&
        (strcmp(pass, "allocation") == 0 ||
         strcmp(pass, "escape") == 0 ||
         strcmp(pass, "alloc") == 0 ||
         strcmp(pass, "exec_ir_allocation") == 0)) {
        switch ((EZrExecIrAllocationReason)reason) {
            case ZR_EXEC_IR_ALLOCATION_REASON_PROVEN_LOCAL:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_NONE;
                return ZR_TRUE;
            case ZR_EXEC_IR_ALLOCATION_REASON_ESCAPE:
            case ZR_EXEC_IR_ALLOCATION_REASON_CROSSED_SUSPEND:
            case ZR_EXEC_IR_ALLOCATION_REASON_NATIVE_RETAINED:
            case ZR_EXEC_IR_ALLOCATION_REASON_UNKNOWN_ESCAPE:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_ESCAPES;
                return ZR_TRUE;
            case ZR_EXEC_IR_ALLOCATION_REASON_IDENTITY_OBSERVED:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_ABI_VISIBLE;
                return ZR_TRUE;
            case ZR_EXEC_IR_ALLOCATION_REASON_DROP_OBSERVABLE:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_EFFECT_ORDER;
                return ZR_TRUE;
            case ZR_EXEC_IR_ALLOCATION_REASON_NO_LAYOUT:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_LAYOUT;
                return ZR_TRUE;
            case ZR_EXEC_IR_ALLOCATION_REASON_NOT_CANDIDATE:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_ALLOCATION;
                return ZR_TRUE;
            case ZR_EXEC_IR_ALLOCATION_REASON_MATERIALIZATION:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_DEOPT;
                return ZR_TRUE;
            case ZR_EXEC_IR_ALLOCATION_REASON_INVALID:
            case ZR_EXEC_IR_ALLOCATION_REASON_COUNT:
            default:
                return ZR_FALSE;
        }
    }

    switch (reason) {
        case ZR_EXEC_IR_PASS_REASON_NONE:
        case ZR_EXEC_IR_PASS_REASON_NO_CHANGE:
            *mapped = ZR_OPTIMIZATION_REMARK_REASON_NONE;
            return ZR_TRUE;
        case ZR_EXEC_IR_PASS_REASON_BUDGET:
            *mapped = ZR_OPTIMIZATION_REMARK_REASON_CODE_BUDGET;
            return ZR_TRUE;
        case ZR_EXEC_IR_PASS_REASON_UNSUPPORTED:
            *mapped = ZR_OPTIMIZATION_REMARK_REASON_TARGET_UNSUPPORTED;
            return ZR_TRUE;
        case ZR_EXEC_IR_PASS_REASON_REQUIREMENT:
            *mapped = ZR_OPTIMIZATION_REMARK_REASON_CAPABILITY_DENIED;
            return ZR_TRUE;
        case ZR_EXEC_IR_PASS_REASON_VERIFIER:
            *mapped = ZR_OPTIMIZATION_REMARK_REASON_ALIAS_UNKNOWN;
            return ZR_TRUE;
        case ZR_EXEC_IR_PASS_REASON_WILL_THROW:
            *mapped = ZR_OPTIMIZATION_REMARK_REASON_EFFECT_ORDER;
            return ZR_TRUE;
        default:
            break;
    }

    /* exec_ir_licm.c stores EZrExecIrLoopReason + 100.  Keep that adapter
     * here, at the parser boundary, rather than leaking a private enum into
     * the core schema. */
    if (reason >= 100u &&
        reason <= 100u + (TZrUInt32)ZR_EXEC_IR_LOOP_REASON_INVALID) {
        switch ((EZrExecIrLoopReason)(reason - 100u)) {
            case ZR_EXEC_IR_LOOP_REASON_NONE:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_NONE;
                return ZR_TRUE;
            case ZR_EXEC_IR_LOOP_REASON_TRAPPING:
            case ZR_EXEC_IR_LOOP_REASON_EFFECT:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_EFFECT_ORDER;
                return ZR_TRUE;
            case ZR_EXEC_IR_LOOP_REASON_NOT_INVARIANT:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_ALIAS_UNKNOWN;
                return ZR_TRUE;
            case ZR_EXEC_IR_LOOP_REASON_OVERFLOW:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_BOUNDS;
                return ZR_TRUE;
            case ZR_EXEC_IR_LOOP_REASON_BUDGET:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_CODE_BUDGET;
                return ZR_TRUE;
            case ZR_EXEC_IR_LOOP_REASON_NO_PREHEADER:
            case ZR_EXEC_IR_LOOP_REASON_MULTIPLE_ENTRY:
            case ZR_EXEC_IR_LOOP_REASON_IRREDUCIBLE:
            case ZR_EXEC_IR_LOOP_REASON_ZERO_TRIP:
            case ZR_EXEC_IR_LOOP_REASON_SEALED:
            case ZR_EXEC_IR_LOOP_REASON_INVALID:
            default:
                *mapped = ZR_OPTIMIZATION_REMARK_REASON_TARGET_UNSUPPORTED;
                return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_parser_remark_set_diagnostic(
        SZrOptimizationRemarkDiagnostic *diagnostic,
        EZrOptimizationRemarkDiagnosticCode code,
        TZrUInt32 field, TZrUInt64 expected, TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->code = code;
        diagnostic->field = field;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
    return ZR_FALSE;
}

static TZrBool zr_parser_bounded_string_length(const TZrChar *text,
                                               TZrSize capacity,
                                               TZrSize *length) {
    TZrSize index;
    if (text == ZR_NULL) return ZR_FALSE;
    for (index = 0u; index < capacity; ++index) {
        if (text[index] == '\0') {
            if (length != ZR_NULL) *length = index;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

void ZrParser_OptimizationRemarkContext_Init(
        SZrParserOptimizationRemarkContext *context) {
    if (context == ZR_NULL) return;
    memset(context, 0, sizeof(*context));
    context->backendMask = ZR_OPTIMIZATION_REMARK_BACKEND_EXEC_BC;
}

static const SZrExecIrSourceMap *zr_parser_remark_source_map(
        const SZrExecIrFunction *function,
        TZrExecIrSourceId sourceId) {
    TZrUInt32 index;
    if (function == ZR_NULL || function->sourceMaps == ZR_NULL) return ZR_NULL;
    for (index = 0u; index < function->sourceMapCount; index++) {
        if (function->sourceMaps[index].sourceId == sourceId) {
            return &function->sourceMaps[index];
        }
    }
    return ZR_NULL;
}

TZrBool ZrParser_OptimizationRemark_FromExecIr(
        const SZrExecIrOptimizationRemark *source,
        const SZrExecIrFunction *function,
        const SZrParserOptimizationRemarkContext *context,
        SZrOptimizationRemark *destination,
        SZrOptimizationRemarkDiagnostic *diagnostic) {
    const SZrExecIrSourceMap *sourceMap;
    const TZrChar *pass;
    const TZrChar *moduleName;
    EZrOptimizationRemarkReason reason;
    TZrSize passLength;
    TZrSize moduleLength = 0u;

    ZrCore_OptimizationRemarkDiagnostic_Clear(diagnostic);
    if (source == ZR_NULL || destination == ZR_NULL)
        return zr_parser_remark_set_diagnostic(
                diagnostic, ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT,
                0u, 1u, 0u);
    if (function != ZR_NULL && function->sourceMapCount != 0u &&
        function->sourceMaps == ZR_NULL)
        return zr_parser_remark_set_diagnostic(
                diagnostic, ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT,
                3u, function->sourceMapCount, 0u);

    pass = source->pass;
    if (!zr_parser_bounded_string_length(
                pass, ZR_OPTIMIZATION_REMARK_PASS_NAME_MAX, &passLength) ||
        passLength == 0u)
        return zr_parser_remark_set_diagnostic(
                diagnostic, ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT,
                2u, 1u, 0u);
    moduleName = context != ZR_NULL ? context->moduleName : ZR_NULL;
    if (moduleName != ZR_NULL &&
        !zr_parser_bounded_string_length(
                moduleName, ZR_OPTIMIZATION_REMARK_MODULE_NAME_MAX,
                &moduleLength))
        return zr_parser_remark_set_diagnostic(
                diagnostic, ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT,
                2u, ZR_OPTIMIZATION_REMARK_MODULE_NAME_MAX - 1u, 0u);
    if (!zr_parser_remark_reason(pass, source->reasonCode, &reason))
        return zr_parser_remark_set_diagnostic(
                diagnostic, ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_REASON,
                5u, 0u, source->reasonCode);

    ZrCore_OptimizationRemark_Init(destination);
    destination->siteKey = context != ZR_NULL ? context->siteKey : 0u;
    destination->moduleHash = context != ZR_NULL ? context->moduleHash : 0u;
    destination->irHash = context != ZR_NULL ? context->irHash : 0u;
    destination->moduleVersion = context != ZR_NULL ? context->moduleVersion : 0u;
    destination->irVersion = context != ZR_NULL ? context->irVersion : 0u;
    destination->sourceVersion = context != ZR_NULL ? context->sourceVersion : 0u;
    destination->sourceId = source->sourceId;
    destination->status = (EZrOptimizationRemarkStatus)source->outcome;
    destination->reason = reason;
    destination->backendMask = context != ZR_NULL && context->backendMask != 0u
                                ? context->backendMask
                                : ZR_OPTIMIZATION_REMARK_BACKEND_EXEC_BC;
    destination->proofId = context != ZR_NULL ? context->proofId : 0u;
    destination->profileCount = context != ZR_NULL ? context->profileCount : 0u;
    destination->estimatedCost = context != ZR_NULL ? context->estimatedCost : 0u;
    destination->boxingFlags = context != ZR_NULL ? context->boxingFlags : 0u;
    destination->allocationFlags = context != ZR_NULL ? context->allocationFlags : 0u;
    destination->cacheFlags = context != ZR_NULL ? context->cacheFlags : 0u;
    destination->deoptFlags = context != ZR_NULL ? context->deoptFlags : 0u;
    if (moduleName != ZR_NULL) {
        (void)memcpy(destination->module, moduleName, moduleLength);
        destination->module[moduleLength] = '\0';
    }
    destination->before = context != ZR_NULL && context->beforeRepresentation != 0u
                            ? context->beforeRepresentation
                            : (TZrUInt32)(source->beforeHash ^
                                          (source->beforeHash >> 32u));
    destination->after = context != ZR_NULL && context->afterRepresentation != 0u
                           ? context->afterRepresentation
                           : (TZrUInt32)(source->afterHash ^
                                         (source->afterHash >> 32u));
    (void)memcpy(destination->pass, pass, passLength);
    destination->pass[passLength] = '\0';

    sourceMap = zr_parser_remark_source_map(function, source->sourceId);
    if (sourceMap != ZR_NULL) {
        destination->sourceRange.startOffset = sourceMap->startOffset;
        destination->sourceRange.endOffset = sourceMap->endOffset;
    } else if (function != ZR_NULL && source->sourceId != 0u) {
        return zr_parser_remark_set_diagnostic(
                diagnostic, ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_NOT_FOUND,
                3u, source->sourceId, 0u);
    }
    if (destination->status != ZR_OPTIMIZATION_REMARK_SUCCESS &&
        destination->reason == ZR_OPTIMIZATION_REMARK_REASON_NONE) {
        /* NO_CHANGE is a valid producer outcome but has no direct public
         * reason code.  Keep the explanation non-empty regardless of whether
         * a profile or live counters accompany it. */
        destination->reason = context != ZR_NULL && context->hasProfile
                                ? ZR_OPTIMIZATION_REMARK_REASON_TARGET_UNSUPPORTED
                                : ZR_OPTIMIZATION_REMARK_REASON_NO_PROFILE;
    }
    if (context != ZR_NULL && context->hasMeasuredCounters) {
        destination->evidence = ZR_OPTIMIZATION_REMARK_EVIDENCE_MEASURED;
        destination->measuredCounterMask = context->measuredCounterMask;
        destination->softwareIcMisses = context->softwareIcMisses;
        destination->hardwareCacheMisses = context->hardwareCacheMisses;
        destination->branchMisses = context->branchMisses;
        destination->allocations = context->allocations;
        destination->deopts = context->deopts;
    } else if (context != ZR_NULL && context->hasProfile) {
        destination->evidence = ZR_OPTIMIZATION_REMARK_EVIDENCE_PROVEN;
    } else if (destination->status != ZR_OPTIMIZATION_REMARK_SUCCESS) {
        destination->evidence = ZR_OPTIMIZATION_REMARK_EVIDENCE_ESTIMATED;
        if (destination->reason == ZR_OPTIMIZATION_REMARK_REASON_NONE) {
            destination->reason = ZR_OPTIMIZATION_REMARK_REASON_NO_PROFILE;
        }
    } else {
        /* A successful pass without a proof/profile is still a valid
         * optimization fact, but its evidence is unavailable.  Do not let
         * the Init default (`proven`) manufacture certainty at the parser
         * boundary. */
        destination->evidence = ZR_OPTIMIZATION_REMARK_EVIDENCE_UNAVAILABLE;
    }
    return ZrCore_OptimizationRemark_Validate(destination, diagnostic);
}

TZrBool ZrParser_OptimizationRemarks_ImportExecIr(
        const SZrExecIrRemarkSink *source,
        const SZrExecIrFunction *function,
        const SZrParserOptimizationRemarkContext *context,
        SZrOptimizationRemarkStore *destination,
        SZrOptimizationRemarkDiagnostic *diagnostic) {
    TZrUInt32 index;
    SZrOptimizationRemark converted;
    TZrUInt32 originalCount;
    TZrUInt64 originalDroppedCount;
    TZrBool originalTruncated;

    ZrCore_OptimizationRemarkDiagnostic_Clear(diagnostic);
    if (source == ZR_NULL || destination == ZR_NULL ||
        (source->count != 0u && source->items == ZR_NULL) ||
        source->count > source->capacity ||
        (destination->count != 0u && destination->items == ZR_NULL) ||
        destination->count > destination->capacity)
        return zr_parser_remark_set_diagnostic(
                diagnostic, ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT,
                0u, 0u, 1u);

    /* Import is a transaction from the producer's point of view.  The core
     * append API may grow the destination and may intentionally mark a full
     * store as truncated, but a malformed later source row must not leave a
     * prefix that looks like a complete import.  Snapshot the observable
     * store state and restore it on every conversion/append failure. */
    originalCount = destination->count;
    originalDroppedCount = destination->droppedCount;
    originalTruncated = destination->truncated;
    for (index = 0u; index < source->count; index++) {
        if (!ZrParser_OptimizationRemark_FromExecIr(
                    &source->items[index], function, context, &converted, diagnostic) ||
            !ZrCore_OptimizationRemarks_Append(destination, &converted, diagnostic)) {
            destination->count = originalCount;
            destination->droppedCount = originalDroppedCount;
            destination->truncated = originalTruncated;
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

const TZrChar *ZrParser_OptimizationRemark_ReasonCodeName(
        EZrOptimizationRemarkReason reason) {
    return ZrCore_OptimizationRemark_ReasonName(reason);
}
