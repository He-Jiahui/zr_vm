#include "zr_vm_core/function.h"

#include <stdint.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/function_identity.h"

/* 单次查询的遍历账本；只借用函数图，visitedFunctions 由本次查询分配和释放。 */
typedef struct SZrFunctionGraphIndexResolver {
    struct SZrState *state;
    const SZrFunction **visitedFunctions;
    TZrUInt32 visitedFunctionCount;
    TZrUInt32 visitedFunctionCapacity;
    TZrUInt32 targetFlatIndex;
    const SZrFunction *resolvedFunction;
    TZrBool failed;
} SZrFunctionGraphIndexResolver;

/* 与 AOT 函数表采用相同定义身份，使重建后的等价函数仍占同一平坦位置。 */
static TZrBool function_graph_functions_equivalent(
        const SZrFunction *left,
        const SZrFunction *right) {
    return ZrCore_Function_HasSameDefinition(left, right);
}

/* 常量边可能回到祖先，按指针或定义身份去重后才能保持 AOT 的索引空间。 */
static TZrBool function_graph_index_resolver_has_visited(
        const SZrFunctionGraphIndexResolver *resolver,
        const SZrFunction *function) {
    for (TZrUInt32 index = 0u; index < resolver->visitedFunctionCount; ++index) {
        if (resolver->visitedFunctions[index] == function ||
            function_graph_functions_equivalent(resolver->visitedFunctions[index], function)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 扩充本次遍历的原生数组；失败留给公开入口统一返回空结果。 */
static TZrBool function_graph_index_resolver_reserve(
        SZrFunctionGraphIndexResolver *resolver) {
    const SZrFunction **newVisitedFunctions;
    TZrUInt32 newCapacity;
    TZrSize newByteSize;

    if (resolver->visitedFunctionCount < resolver->visitedFunctionCapacity) {
        return ZR_TRUE;
    }
    newCapacity = resolver->visitedFunctionCapacity == 0u
                          ? 16u
                          : (resolver->visitedFunctionCapacity <= UINT32_MAX / 2u
                                     ? resolver->visitedFunctionCapacity * 2u
                                     : UINT32_MAX);
    if (newCapacity <= resolver->visitedFunctionCapacity) {
        return ZR_FALSE;
    }
#if SIZE_MAX < UINT32_MAX
    if (newCapacity > SIZE_MAX / sizeof(*resolver->visitedFunctions)) {
        return ZR_FALSE;
    }
#endif
    newByteSize = (TZrSize)newCapacity * sizeof(*resolver->visitedFunctions);
    newVisitedFunctions = (const SZrFunction **)ZrCore_Memory_RawMallocWithType(
            resolver->state->global, newByteSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newVisitedFunctions == ZR_NULL) {
        return ZR_FALSE;
    }
    if (resolver->visitedFunctions != ZR_NULL) {
        TZrSize oldByteSize = (TZrSize)resolver->visitedFunctionCapacity *
                              sizeof(*resolver->visitedFunctions);
        ZrCore_Memory_RawCopy(
                (TZrPtr)newVisitedFunctions,
                (TZrPtr)resolver->visitedFunctions,
                (TZrSize)resolver->visitedFunctionCount *
                        sizeof(*resolver->visitedFunctions));
        ZrCore_Memory_RawFreeWithType(
                resolver->state->global,
                (TZrPtr)resolver->visitedFunctions,
                oldByteSize,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    resolver->visitedFunctions = newVisitedFunctions;
    resolver->visitedFunctionCapacity = newCapacity;
    return ZR_TRUE;
}

/* 按 AOT 展平顺序先登记当前函数，再访常量函数和显式子函数。 */
static TZrBool function_graph_index_resolver_visit(
        SZrFunctionGraphIndexResolver *resolver,
        const SZrFunction *function) {
    if (resolver->failed || function == ZR_NULL ||
        function_graph_index_resolver_has_visited(resolver, function)) {
        return ZR_FALSE;
    }
    /* 命中后只借出原图节点，不需要再扩容；索引由前面已登记的唯一节点数确定。 */
    if (resolver->visitedFunctionCount == resolver->targetFlatIndex) {
        resolver->resolvedFunction = function;
        return ZR_TRUE;
    }
    if (!function_graph_index_resolver_reserve(resolver)) {
        resolver->failed = ZR_TRUE;
        return ZR_FALSE;
    }
    resolver->visitedFunctions[resolver->visitedFunctionCount++] = function;

    /* 常量函数边必须先于 childFunctionList，和 backend_aot_flatten_function_graph 对齐。 */
    for (TZrUInt32 constantIndex = 0u;
         constantIndex < function->constantValueLength;
         ++constantIndex) {
        const SZrFunction *constantFunction = ZrCore_Closure_GetMetadataFunctionFromValue(
                resolver->state, &function->constantValueList[constantIndex]);
        if (function_graph_index_resolver_visit(resolver, constantFunction)) {
            return ZR_TRUE;
        }
    }
    for (TZrUInt32 childIndex = 0u;
         childIndex < function->childFunctionLength;
         ++childIndex) {
        if (function_graph_index_resolver_visit(
                    resolver, &function->childFunctionList[childIndex])) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/** @brief 由 AOT 函数表平坦索引找回 VM 函数图中的节点，供元数据与导入绑定消费。
 * @pre rootFunction 是有效且在调用期间稳定的函数图，state->global 可分配原生暂存。
 * @return 命中时借出原图函数，仅在节点存活且地址稳定期间有效；失败时返回空指针。
 * @note 入口会重建子函数 owner/prototype 链并重绑常量别名，不是纯只读查询。 */
SZrFunction *ZrCore_Function_ResolveGraphFunctionByFlatIndex(
        struct SZrState *state,
        SZrFunction *rootFunction,
        TZrUInt32 flatIndex) {
    SZrFunctionGraphIndexResolver resolver = {0};

    if (state == ZR_NULL || state->global == ZR_NULL || rootFunction == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Function_RebindConstantFunctionValuesToChildren(rootFunction);
    resolver.state = state;
    resolver.targetFlatIndex = flatIndex;
    /* BUG: flatIndex>0 的暂存分配失败返回 NULL；LinkCallBindings 第二轮解析
     * 未检查结果便取 callSiteCaches，前一轮校验不能保证这次分配成功。 */
    function_graph_index_resolver_visit(&resolver, rootFunction);
    if (resolver.visitedFunctions != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                state->global,
                (TZrPtr)resolver.visitedFunctions,
                (TZrSize)resolver.visitedFunctionCapacity *
                        sizeof(*resolver.visitedFunctions),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    return (SZrFunction *)(TZrPtr)resolver.resolvedFunction;
}
