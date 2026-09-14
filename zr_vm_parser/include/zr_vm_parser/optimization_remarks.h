#ifndef ZR_VM_PARSER_OPTIMIZATION_REMARKS_H
#define ZR_VM_PARSER_OPTIMIZATION_REMARKS_H

#include "zr_vm_core/optimization_remark.h"
#include "zr_vm_parser/exec_ir_pass_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Context supplied by the canonical ExecIR owner.  No parser or runtime
 * pointer is copied into the resulting core records. */
typedef struct SZrParserOptimizationRemarkContext {
    /* Address-free profile/runtime identity.  Producers may obtain this from
     * ZrParser_ExecIr_ProfileSiteKey; zero means that no site is associated
     * with the pass remark. */
    TZrUInt64 siteKey;
    TZrUInt64 moduleHash;
    TZrUInt64 irHash;
    TZrUInt64 moduleVersion;
    TZrUInt64 irVersion;
    TZrUInt64 sourceVersion;
    TZrUInt32 backendMask;
    TZrUInt32 measuredCounterMask;
    TZrUInt64 profileCount;
    TZrUInt64 estimatedCost;
    TZrUInt64 softwareIcMisses;
    TZrUInt64 hardwareCacheMisses;
    TZrUInt64 branchMisses;
    TZrUInt64 allocations;
    TZrUInt64 deopts;
    TZrUInt64 proofId;
    TZrUInt32 beforeRepresentation;
    TZrUInt32 afterRepresentation;
    TZrUInt32 boxingFlags;
    TZrUInt32 allocationFlags;
    TZrUInt32 cacheFlags;
    TZrUInt32 deoptFlags;
    TZrBool hasProfile;
    TZrBool hasMeasuredCounters;
    /* Borrowed producer metadata; copied into the fixed-width core record. */
    const TZrChar *moduleName;
} SZrParserOptimizationRemarkContext;

ZR_PARSER_API void ZrParser_OptimizationRemarkContext_Init(
        SZrParserOptimizationRemarkContext *context);

/* Copy one parser/ExecIR remark into the stable core schema. */
ZR_PARSER_API TZrBool ZrParser_OptimizationRemark_FromExecIr(
        const SZrExecIrOptimizationRemark *source,
        const SZrExecIrFunction *function,
        const SZrParserOptimizationRemarkContext *context,
        SZrOptimizationRemark *destination,
        SZrOptimizationRemarkDiagnostic *diagnostic);

/* Transactional-at-the-store-boundary import of an append-only pass sink.
 * A malformed row or append failure restores count/truncation counters, so
 * callers never observe a partial import as a completed batch. */
ZR_PARSER_API TZrBool ZrParser_OptimizationRemarks_ImportExecIr(
        const SZrExecIrRemarkSink *source,
        const SZrExecIrFunction *function,
        const SZrParserOptimizationRemarkContext *context,
        SZrOptimizationRemarkStore *destination,
        SZrOptimizationRemarkDiagnostic *diagnostic);

/* Stable reason-code lookup used by parser diagnostics and tooling. */
ZR_PARSER_API const TZrChar *ZrParser_OptimizationRemark_ReasonCodeName(
        EZrOptimizationRemarkReason reason);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_PARSER_OPTIMIZATION_REMARKS_H */
