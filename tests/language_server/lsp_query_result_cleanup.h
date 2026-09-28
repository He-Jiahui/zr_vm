#ifndef ZR_TESTS_LSP_QUERY_RESULT_CLEANUP_H
#define ZR_TESTS_LSP_QUERY_RESULT_CLEANUP_H

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_language_server.h"

/** @brief 释放导航/引用投影返回的逐项原生对象和数组缓冲区。
 *  @pre state 有效且仍持有原分配器；locations/highlights 若非空，应是
 *       按对应指针元素大小初始化的数组，且尚未被别的所有者释放。
 *  @note 测试在成功与失败出口均调用；SZrString 字段由 VM GC 管理。 */
static void free_local_reference_projection_results(
        SZrState *state,
        SZrArray *locations,
        SZrArray *highlights) {
    TZrSize index;

    if (state == ZR_NULL) {
        return;
    }
    /* lsp_interface 的 location/highlight 生产者分配单个 RawMalloc 对象，
     * Array_Free 仅释放元素缓冲区，因此先释放每个指针目标。 */
    if (locations != ZR_NULL && locations->isValid) {
        for (index = 0U; index < locations->length; index++) {
            SZrLspLocation **slot =
                    (SZrLspLocation **)ZrCore_Array_Get(locations, index);
            if (slot != ZR_NULL && *slot != ZR_NULL) {
                ZrCore_Memory_RawFree(
                        state->global, *slot, sizeof(SZrLspLocation));
            }
        }
        ZrCore_Array_Free(state, locations);
    }
    if (highlights != ZR_NULL && highlights->isValid) {
        for (index = 0U; index < highlights->length; index++) {
            SZrLspDocumentHighlight **slot =
                    (SZrLspDocumentHighlight **)ZrCore_Array_Get(
                            highlights, index);
            if (slot != ZR_NULL && *slot != ZR_NULL) {
                ZrCore_Memory_RawFree(
                        state->global,
                        *slot,
                        sizeof(SZrLspDocumentHighlight));
            }
        }
        ZrCore_Array_Free(state, highlights);
    }
}

#endif
