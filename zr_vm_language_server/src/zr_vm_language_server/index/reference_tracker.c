//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_language_server/reference_tracker.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

#define ZR_LSP_RANGE_SPAN_SCORE_INVALID ((TZrSize)-1)

/** 精确位置查询以 URI 文本为边界；不同字符串对象但同 URI 仍可命中。 */
static TZrBool source_uri_equals(SZrString *left, SZrString *right) {
    TZrNativeString leftText;
    TZrNativeString rightText;
    TZrSize leftLength;
    TZrSize rightLength;

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    if (left == right) {
        return ZR_TRUE;
    }

    if (left->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        leftText = ZrCore_String_GetNativeStringShort(left);
        leftLength = left->shortStringLength;
    } else {
        leftText = ZrCore_String_GetNativeString(left);
        leftLength = left->longStringLength;
    }

    if (right->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        rightText = ZrCore_String_GetNativeStringShort(right);
        rightLength = right->shortStringLength;
    } else {
        rightText = ZrCore_String_GetNativeString(right);
        rightLength = right->longStringLength;
    }

    return leftText != ZR_NULL && rightText != ZR_NULL &&
           leftLength == rightLength &&
           memcmp(leftText, rightText, leftLength) == 0;
}

/** 先比 URI，再用完整 offset 或行列坐标判定引用是否覆盖请求位置。 */
static TZrBool range_contains_position(SZrFileRange range, SZrFileRange position) {
    TZrBool startMatch;
    TZrBool endMatch;

    if (!source_uri_equals(range.source, position.source)) {
        return ZR_FALSE;
    }

    if (range.start.offset > 0 && range.end.offset > 0 &&
        position.start.offset > 0 && position.end.offset > 0) {
        return range.start.offset <= position.start.offset &&
               position.end.offset <= range.end.offset;
    }

    startMatch = (range.start.line < position.start.line) ||
                 (range.start.line == position.start.line &&
                  range.start.column <= position.start.column);
    endMatch = (position.end.line < range.end.line) ||
               (position.end.line == range.end.line &&
                position.end.column <= range.end.column);
    return startMatch && endMatch;
}
// TODO: 若同一 tracker 混用完整 offset 与仅行列的范围，跨度分数单位不同；核对上游是否统一坐标来源。
/** 将范围跨度转为命中优先级；没有可用 offset 时退回行列。 */
static TZrSize range_span_score(SZrFileRange range) {
    if (range.start.offset > 0 && range.end.offset >= range.start.offset) {
        return range.end.offset - range.start.offset;
    }

    if (range.end.line < range.start.line) {
        return ZR_LSP_RANGE_SPAN_SCORE_INVALID;
    }

    return (TZrSize)(range.end.line - range.start.line) * ZR_LSP_SIGNATURE_RANGE_PACK_BASE +
           (TZrSize)(range.end.column >= range.start.column
                         ? range.end.column - range.start.column
                         : 0);
}

/** 多个引用覆盖同一点时优先最窄范围，同跨度时使用优先于定义。 */
static TZrBool reference_is_better_match(SZrReference *candidate, SZrReference *best) {
    TZrSize candidateSpan;
    TZrSize bestSpan;

    if (candidate == ZR_NULL) {
        return ZR_FALSE;
    }
    if (best == ZR_NULL) {
        return ZR_TRUE;
    }

    candidateSpan = range_span_score(candidate->location);
    bestSpan = range_span_score(best->location);
    if (candidateSpan != bestSpan) {
        return candidateSpan < bestSpan;
    }

    if (candidate->type != best->type) {
        return candidate->type != ZR_REFERENCE_DEFINITION;
    }

    return ZR_FALSE;
}

/** @brief 为语义分析器建立引用列表；symbolTable 仍由调用方持有。
 *  @pre state 及其 global、symbolTable 有效；释放 tracker 须早于符号表。
 *  @return 新 tracker，或显式参数/结构分配失败时返回空。
 *  @note BUG: Array_Init 的缓冲分配失败仍标记数组有效；此处未检查 head 就返回，后续 AddReference 会断言失败或向空缓冲写入。 */
SZrReferenceTracker *ZrLanguageServer_ReferenceTracker_New(SZrState *state, SZrSymbolTable *symbolTable) {
    if (state == ZR_NULL || symbolTable == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrReferenceTracker *tracker = (SZrReferenceTracker *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrReferenceTracker));
    if (tracker == ZR_NULL) {
        return ZR_NULL;
    }
    
    ZrCore_Array_Init(state,
                      &tracker->allReferences,
                      sizeof(SZrReference *),
                      ZR_LSP_LARGE_ARRAY_INITIAL_CAPACITY);
    
    return tracker;
}

/** @brief 释放 tracker 持有的引用记录与数组，不释放借用的 symbol 或 URI。
 *  @pre 使用与构造时同一全局分配器的 state；调用后所有 FindReferenceAt 结果失效。 */
void ZrLanguageServer_ReferenceTracker_Free(SZrState *state, SZrReferenceTracker *tracker) {
    if (state == ZR_NULL || tracker == ZR_NULL) {
        return;
    }

    // 释放所有引用
    for (TZrSize i = 0; i < tracker->allReferences.length; i++) {
        SZrReference **refPtr = (SZrReference **)ZrCore_Array_Get(&tracker->allReferences, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *refPtr, sizeof(SZrReference));
        }
    }
    
    ZrCore_Array_Free(state, &tracker->allReferences);

    ZrCore_Memory_RawFree(state->global, tracker, sizeof(SZrReferenceTracker));
}

/** @brief 将定义或使用位置同步登记到 tracker 和 symbol，供精确位置查询与语义索引使用。
 *  @pre symbol 和 location.source 的寿命覆盖 tracker；tracker 在有效数组状态下使用。
 *  @note BUG: Array_Push 扩容失败没有状态返回；这里可能写空缓冲且无法保证两份索引一致。 */
TZrBool ZrLanguageServer_ReferenceTracker_AddReference(SZrState *state, 
                                     SZrReferenceTracker *tracker,
                                     SZrSymbol *symbol,
                                     SZrFileRange location,
                                     EZrReferenceType type) {
    if (state == ZR_NULL || tracker == ZR_NULL || symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 创建引用
    SZrReference *reference = (SZrReference *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrReference));
    if (reference == ZR_NULL) {
        return ZR_FALSE;
    }
    
    reference->symbol = symbol;
    reference->symbolId = symbol->semanticId;
    reference->location = location;
    reference->type = type;
    
    // 添加到所有引用数组
    ZrCore_Array_Push(state, &tracker->allReferences, &reference);
    
    // 同时添加到符号的引用列表
    ZrLanguageServer_Symbol_AddReference(state, symbol, location);
    
    return ZR_TRUE;
}

/** @brief 查找同 URI 下覆盖指定位置的最具体引用；当前直接调用主要来自引用追踪测试。
 *  @return 借用 tracker 内部记录；tracker Free、重建或其 symbol/URI 释放后失效。 */
SZrReference *ZrLanguageServer_ReferenceTracker_FindReferenceAt(SZrReferenceTracker *tracker,
                                                 SZrFileRange position) {
    SZrReference *bestReference = ZR_NULL;

    if (tracker == ZR_NULL) {
        return ZR_NULL;
    }
    
    for (TZrSize i = 0; i < tracker->allReferences.length; i++) {
        SZrReference **refPtr = (SZrReference **)ZrCore_Array_Get(&tracker->allReferences, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            SZrReference *ref = *refPtr;
            if (range_contains_position(ref->location, position) &&
                reference_is_better_match(ref, bestReference)) {
                bestReference = ref;
            }
        }
    }
    
    return bestReference;
}
