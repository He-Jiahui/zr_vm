#ifndef ZR_VM_CORE_CONTIGUOUS_VIEW_H
#define ZR_VM_CORE_CONTIGUOUS_VIEW_H

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_core/conf.h"

typedef enum EZrViewDiagnosticCode {
    ZR_VIEW_DIAGNOSTIC_NONE = 0,
    ZR_VIEW_DIAGNOSTIC_INVALID,
    ZR_VIEW_DIAGNOSTIC_BOUNDS,
    ZR_VIEW_DIAGNOSTIC_OVERFLOW,
    ZR_VIEW_DIAGNOSTIC_GENERATION,
    ZR_VIEW_DIAGNOSTIC_LIFETIME,
    ZR_VIEW_DIAGNOSTIC_NOT_CONTIGUOUS
} EZrViewDiagnosticCode;

typedef struct SZrViewDiagnostic {
    EZrViewDiagnosticCode code;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrViewDiagnostic;

#define ZR_VIEW_FLAG_READ_ONLY ((TZrUInt32)1u << 0u)
#define ZR_VIEW_FLAG_PINNED ((TZrUInt32)1u << 1u)
#define ZR_VIEW_FLAG_INLINE_STORAGE ((TZrUInt32)1u << 2u)

typedef struct SZrContiguousViewRequest {
    TZrPtr ownerRoot;
    TZrSize byteOffset;
    TZrSize length;
    TZrSize stride;
    TZrSize elementSize;
    TZrUInt64 elementLayoutHash;
    TZrUInt64 storageGeneration;
    TZrUInt64 lifetimeRegion;
    TZrUInt32 flags;
} SZrContiguousViewRequest;

typedef struct SZrContiguousView {
    TZrPtr ownerRoot;
    TZrSize byteOffset;
    TZrSize length;
    TZrSize stride;
    TZrSize elementSize;
    TZrUInt64 elementLayoutHash;
    TZrUInt64 storageGeneration;
    TZrUInt64 lifetimeRegion;
    TZrUInt32 flags;
} SZrContiguousView;

ZR_CORE_API void ZrCore_View_Init(SZrContiguousView *view);
ZR_CORE_API TZrBool ZrCore_View_Create(const SZrContiguousViewRequest *request,
                                        SZrContiguousView *view,
                                        SZrViewDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_View_Validate(const SZrContiguousView *view,
                                          TZrUInt64 currentGeneration,
                                          SZrViewDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_View_Slice(const SZrContiguousView *view,
                                       TZrSize start, TZrSize length,
                                       SZrContiguousView *slice,
                                       SZrViewDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_View_IndexOffset(const SZrContiguousView *view,
                                             TZrInt64 index,
                                             TZrSize *offset,
                                             SZrViewDiagnostic *diagnostic);

#endif
