#include "zr_vm_parser/bound_expression.h"

#include "zr_vm_parser/semantic.h"
#include "zr_vm_core/memory.h"

#include <string.h>

/**
 * @brief 为已经初始化或刚清零的结果建立 INVALID 状态和参数映射数组。
 *
 * Bind 通过此路径复用输出对象，因此仅在 isValid 时释放旧数组，随后统一重建哨兵。
 * @pre bound 为零初始化对象或此前由本模块初始化的有效对象。
 */
static void bound_value_construct_reset(SZrState *state, SZrBoundValueConstruct *bound) {
    if (state == ZR_NULL || bound == ZR_NULL) {
        return;
    }
    if (bound->arguments.isValid) {
        ZrCore_Array_Free(state, &bound->arguments);
    }
    memset(bound, 0, sizeof(*bound));
    bound->kind = ZR_BOUND_EXPRESSION_INVALID;
    bound->typeId = ZR_SEMANTIC_ID_INVALID;
    bound->constructorId = ZR_SEMANTIC_ID_INVALID;
    bound->resultTypeId = ZR_SEMANTIC_ID_INVALID;
    /* BUG: Array_Init 将分配失败的数组仍标为有效；后续成功解析后 Push 会访问空存储。
     * 底层契约与故障路径见 zr_vm_core/include/zr_vm_core/array.h:34-42,73-89。 */
    ZrCore_Array_Init(
            state,
            &bound->arguments,
            sizeof(SZrBoundValueConstructArgument),
            ZR_PARSER_INITIAL_CAPACITY_TINY);
}

/**
 * @brief 建立可供后续 Bind 复用的空绑定结果。
 *
 * 调用方可直接初始化未定义内容的栈对象；重复初始化前必须先 Free，以免丢弃数组。
 */
void ZrParser_BoundValueConstruct_Init(
        SZrState *state,
        SZrBoundValueConstruct *bound) {
    if (state == ZR_NULL || bound == ZR_NULL) {
        return;
    }
    memset(bound, 0, sizeof(*bound));
    bound_value_construct_reset(state, bound);
}

/**
 * @brief 释放本对象拥有的参数映射数组并清除全部绑定状态。
 *
 * 构造器名称等字段只是借用标识，不在此处释放；调用方应对每次成功初始化配对调用。
 */
void ZrParser_BoundValueConstruct_Free(
        SZrState *state,
        SZrBoundValueConstruct *bound) {
    if (state == ZR_NULL || bound == ZR_NULL) {
        return;
    }
    if (bound->arguments.isValid) {
        ZrCore_Array_Free(state, &bound->arguments);
    }
    memset(bound, 0, sizeof(*bound));
}

/**
 * @brief 将源序实参描述解析成可直接驱动 struct-init lowering 的绑定记录。
 *
 * canonical resolver 决定公开性、唯一匹配和默认构造规则；本层只把 resolver 给出的
 * parameterIndex 与源序 sourceIndex 合并，避免编译器再次实现具名/默认实参匹配。
 * @pre context 含有效 state；outBound 已由 Init 初始化；arguments 在 argumentCount
 *      范围内有效，name 字符串在 outBound 使用期间仍由调用方保持存活。
 * @return 通过入口校验后，只有 RESOLVED 会提交 kind、constructorId、类型和参数映射；
 *         解析失败保持 INVALID 输出，供调用方据具体状态诊断。
 * @note 入口形态错误在 reset 之前返回并保留 outBound 原值；进入解析后失败已清理旧绑定，
 *       可直接再次 Bind 或最终 Free。
 */
EZrValueConstructorResolution ZrParser_BoundValueConstruct_Bind(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrBoundValueConstructArgumentInput *arguments,
        TZrSize argumentCount,
        SZrFileRange sourceRange,
        SZrBoundValueConstruct *outBound) {
    SZrCanonicalValueArgument *canonicalArguments = ZR_NULL;
    TZrUInt32 *parameterIndices = ZR_NULL;
    TZrSymbolId constructorId = ZR_SEMANTIC_ID_INVALID;
    EZrValueConstructorResolution resolution;
    TZrSize index;

    /* 输出对象需先初始化；参数数组为空时允许传 NULL，保持默认构造的零实参形式。 */
    if (context == ZR_NULL || outBound == ZR_NULL ||
        (argumentCount > 0U && arguments == ZR_NULL)) {
        return ZR_VALUE_CONSTRUCTOR_INVALID_ARGUMENTS;
    }
    bound_value_construct_reset(context->state, outBound);
    if (argumentCount > 0U) {
        canonicalArguments = (SZrCanonicalValueArgument *)ZrCore_Memory_RawMallocWithType(
                context->state->global,
                sizeof(SZrCanonicalValueArgument) * argumentCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        parameterIndices = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(
                context->state->global,
                sizeof(TZrUInt32) * argumentCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (canonicalArguments == ZR_NULL || parameterIndices == ZR_NULL) {
            if (canonicalArguments != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(
                        context->state->global,
                        canonicalArguments,
                        sizeof(SZrCanonicalValueArgument) * argumentCount,
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            if (parameterIndices != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(
                        context->state->global,
                        parameterIndices,
                        sizeof(TZrUInt32) * argumentCount,
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            return ZR_VALUE_CONSTRUCTOR_INVALID_ARGUMENTS;
        }
        for (index = 0U; index < argumentCount; index++) {
            canonicalArguments[index].typeId = arguments[index].typeId;
            canonicalArguments[index].name = arguments[index].name;
            canonicalArguments[index].callSiteMarker = arguments[index].callSiteMarker;
        }
    }
    /* 所有源实参先转换为 resolver 所需的轻量视图；索引输出由该 contract 原子决定。 */
    resolution = ZrParser_CanonicalType_ResolveValueConstructorContract(
            context,
            typeId,
            canonicalArguments,
            argumentCount,
            &constructorId,
            parameterIndices);
    if (resolution == ZR_VALUE_CONSTRUCTOR_RESOLVED) {
        for (index = 0U; index < argumentCount; index++) {
            SZrBoundValueConstructArgument argument;
            argument.typeId = arguments[index].typeId;
            argument.name = arguments[index].name;
            argument.callSiteMarker = arguments[index].callSiteMarker;
            argument.sourceIndex = (TZrUInt32)index;
            argument.parameterIndex = parameterIndices[index];
            argument.sourceRange = arguments[index].sourceRange;
            /* BUG: 底层 Push 扩容失败会丢失旧 head，随后向空指针写入；见 array.h:82-88。 */
            ZrCore_Array_Push(context->state, &outBound->arguments, &argument);
        }
        outBound->kind = ZR_BOUND_EXPRESSION_VALUE_CONSTRUCT;
        outBound->typeId = typeId;
        outBound->constructorId = constructorId;
        outBound->resultTypeId = typeId;
        outBound->sourceRange = sourceRange;
    }
    if (canonicalArguments != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                context->state->global,
                canonicalArguments,
                sizeof(SZrCanonicalValueArgument) * argumentCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (parameterIndices != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                context->state->global,
                parameterIndices,
                sizeof(TZrUInt32) * argumentCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return resolution;
}
