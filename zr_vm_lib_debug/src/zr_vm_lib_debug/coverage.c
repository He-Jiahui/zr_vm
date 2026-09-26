#include "zr_vm_lib_debug/coverage.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"

/* hook 回调只有 state 参数，因此用进程内活动表找回该 state 的 Coverage 会话。 */
/* TODO: 活动表为无锁链表；核对多 state 并发运行时 hook 与 Start/Stop 的线程约束，
 * 并用并发采集测试决定是否需要互斥或按 state 存储的会话指针。 */
static ZrDebugCoverage *g_active_coverages = ZR_NULL;

/* hook 期间借用当前栈帧的名称和源信息，随后复制到 CoverageLine 快照。 */
typedef struct ZrDebugCoverageLocation {
    const SZrFunction *function;
    const TZrChar *name;
    const TZrChar *source;
    TZrUInt32 line;
} ZrDebugCoverageLocation;

static void zr_debug_coverage_copy_text(TZrChar *destination, TZrSize destinationSize, const TZrChar *source) {
    TZrSize length;

    if (destination == ZR_NULL || destinationSize == 0u) {
        return;
    }

    if (source == ZR_NULL) {
        source = "";
    }

    length = strlen(source);
    if (length >= destinationSize) {
        length = destinationSize - 1u;
    }
    if (length > 0u) {
        memcpy(destination, source, length);
    }
    destination[length] = '\0';
}

/* 仅把原 hook 原先订阅的事件转发给它，避免 Coverage 扩展 LINE mask 改变旧观察者语义。 */
static TZrUInt32 zr_debug_coverage_event_mask(EZrDebugHookEvent event) {
    if (event >= ZR_DEBUG_HOOK_EVENT_MAX) {
        return 0u;
    }
    return 1u << event;
}

static const TZrChar *zr_debug_coverage_function_name(const SZrFunction *function) {
    if (function == ZR_NULL || function->functionName == ZR_NULL) {
        return "<anonymous>";
    }
    return ZrCore_String_GetNativeString(function->functionName);
}

static const TZrChar *zr_debug_coverage_function_source(const SZrFunction *function) {
    if (function == ZR_NULL || function->sourceCodeList == ZR_NULL) {
        return "";
    }
    return ZrCore_String_GetNativeString(function->sourceCodeList);
}

static ZrDebugCoverage *zr_debug_coverage_find_active(SZrState *state) {
    ZrDebugCoverage *coverage;

    for (coverage = g_active_coverages; coverage != ZR_NULL; coverage = coverage->next_active) {
        if (coverage->state == state) {
            return coverage;
        }
    }

    return ZR_NULL;
}

/* 同一个 state 只对应一个 Coverage，hook 才能无歧义地定位其行表。 */
static TZrBool zr_debug_coverage_register_active(ZrDebugCoverage *coverage) {
    if (coverage == ZR_NULL || coverage->state == ZR_NULL) {
        return ZR_FALSE;
    }
    if (zr_debug_coverage_find_active(coverage->state) != ZR_NULL) {
        return ZR_FALSE;
    }

    coverage->next_active = g_active_coverages;
    g_active_coverages = coverage;
    return ZR_TRUE;
}

static void zr_debug_coverage_unregister_active(ZrDebugCoverage *coverage) {
    ZrDebugCoverage **cursor = &g_active_coverages;

    while (*cursor != ZR_NULL) {
        if (*cursor == coverage) {
            *cursor = coverage->next_active;
            coverage->next_active = ZR_NULL;
            return;
        }
        cursor = &(*cursor)->next_active;
    }
}

/* 行结果随入口函数树和执行期 LINE hook 增长；注册时的扩容失败返回调用方，
 * hook 路径的扩容失败会静默少报该行。 */
/* TODO: 倍增容量和 sizeof(*lines) * newCapacity 未查溢出；核对 TZrSize 上限与
 * 超大函数树的可达性，再决定是否补受控失败路径。 */
static TZrBool zr_debug_coverage_reserve_lines(ZrDebugCoverage *coverage, TZrSize minimumCapacity) {
    ZrDebugCoverageLine *lines;
    TZrSize newCapacity;

    if (coverage == ZR_NULL) {
        return ZR_FALSE;
    }
    if (coverage->line_capacity >= minimumCapacity) {
        return ZR_TRUE;
    }

    newCapacity = coverage->line_capacity == 0u ? 32u : coverage->line_capacity * 2u;
    while (newCapacity < minimumCapacity) {
        newCapacity *= 2u;
    }

    lines = (ZrDebugCoverageLine *)realloc(coverage->lines, sizeof(*lines) * newCapacity);
    if (lines == ZR_NULL) {
        return ZR_FALSE;
    }

    coverage->lines = lines;
    coverage->line_capacity = newCapacity;
    return ZR_TRUE;
}

static TZrSize zr_debug_coverage_find_line(const ZrDebugCoverage *coverage,
                                           const SZrFunction *function,
                                           TZrUInt32 line) {
    TZrSize index;

    if (coverage == ZR_NULL || function == ZR_NULL || line == 0u) {
        return (TZrSize)-1;
    }

    for (index = 0u; index < coverage->line_count; index++) {
        if (coverage->lines[index].function == function && coverage->lines[index].line == line) {
            return index;
        }
    }

    return (TZrSize)-1;
}

/* 以函数身份和行号去重，合并预注册的可执行行与运行时实际命中行。 */
static TZrSize zr_debug_coverage_add_line(ZrDebugCoverage *coverage,
                                          const SZrFunction *function,
                                          TZrUInt32 line,
                                          TZrBool executable,
                                          TZrBool executed) {
    TZrSize existingIndex;
    TZrSize index;
    ZrDebugCoverageLine *entry;

    if (coverage == ZR_NULL || function == ZR_NULL || line == 0u) {
        return (TZrSize)-1;
    }

    existingIndex = zr_debug_coverage_find_line(coverage, function, line);
    if (existingIndex != (TZrSize)-1) {
        coverage->lines[existingIndex].executable = (TZrBool)(coverage->lines[existingIndex].executable || executable);
        coverage->lines[existingIndex].executed = (TZrBool)(coverage->lines[existingIndex].executed || executed);
        return existingIndex;
    }

    if (!zr_debug_coverage_reserve_lines(coverage, coverage->line_count + 1u)) {
        return (TZrSize)-1;
    }

    index = coverage->line_count++;
    entry = &coverage->lines[index];
    memset(entry, 0, sizeof(*entry));
    entry->function = function;
    entry->line = line;
    entry->executable = executable;
    entry->executed = executed;
    zr_debug_coverage_copy_text(entry->name,
                                sizeof(entry->name),
                                zr_debug_coverage_function_name(function));
    zr_debug_coverage_copy_text(entry->source,
                                sizeof(entry->source),
                                zr_debug_coverage_function_source(function));
    return index;
}

/* LINE hook 的事件行号可能为空，借当前激活帧补全来源后才记入报告。 */
static TZrBool zr_debug_coverage_capture_location(SZrState *state,
                                                  const SZrDebugInfo *debugInfo,
                                                  ZrDebugCoverageLocation *outLocation) {
    SZrDebugActivation activation;
    SZrDebugInfo info;

    if (state == ZR_NULL || outLocation == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outLocation, 0, sizeof(*outLocation));
    memset(&activation, 0, sizeof(activation));
    if (!ZrCore_Debug_GetStack(state, 0u, &activation) || activation.function == ZR_NULL) {
        return ZR_FALSE;
    }

    outLocation->function = activation.function;
    outLocation->line = debugInfo != ZR_NULL ? (TZrUInt32)debugInfo->currentLine : 0u;
    outLocation->name = zr_debug_coverage_function_name(activation.function);
    outLocation->source = zr_debug_coverage_function_source(activation.function);

    memset(&info, 0, sizeof(info));
    if (ZrCore_Debug_GetInfo(state,
                             &activation,
                             (EZrDebugInfoType)(ZR_DEBUG_INFO_FUNCTION_NAME |
                                                ZR_DEBUG_INFO_SOURCE_FILE |
                                                ZR_DEBUG_INFO_LINE_NUMBER),
                             &info)) {
        if (info.name != ZR_NULL) {
            outLocation->name = info.name;
        }
        if (info.source != ZR_NULL) {
            outLocation->source = info.source;
        }
        if (outLocation->line == 0u) {
            outLocation->line = (TZrUInt32)info.currentLine;
        }
    }

    return (TZrBool)(outLocation->line != 0u);
}

static void zr_debug_coverage_record_line(ZrDebugCoverage *coverage,
                                          SZrState *state,
                                          const SZrDebugInfo *debugInfo) {
    ZrDebugCoverageLocation location;
    TZrSize lineIndex;

    if (coverage == ZR_NULL || debugInfo == ZR_NULL || debugInfo->event != ZR_DEBUG_HOOK_EVENT_LINE) {
        return;
    }
    if (!zr_debug_coverage_capture_location(state, debugInfo, &location)) {
        return;
    }

    lineIndex = zr_debug_coverage_add_line(coverage, location.function, location.line, ZR_TRUE, ZR_TRUE);
    /* TODO: LINE hook 中分配失败只跳过该行，报告仍像完整采集；核对 CLI 是否
     * 需要 incomplete 状态及可控内存失败测试，避免将少报解释成未执行。 */
    if (lineIndex == (TZrSize)-1) {
        return;
    }
    zr_debug_coverage_copy_text(coverage->lines[lineIndex].name,
                                sizeof(coverage->lines[lineIndex].name),
                                location.name);
    zr_debug_coverage_copy_text(coverage->lines[lineIndex].source,
                                sizeof(coverage->lines[lineIndex].source),
                                location.source);
}

/* Coverage 作为临时 hook 包装层，先记录自身 LINE 事件，再按原 mask 转发。 */
static void zr_debug_coverage_hook(SZrState *state, SZrDebugInfo *debugInfo) {
    ZrDebugCoverage *coverage = zr_debug_coverage_find_active(state);

    if (coverage != ZR_NULL && debugInfo != ZR_NULL && debugInfo->event == ZR_DEBUG_HOOK_EVENT_LINE) {
        zr_debug_coverage_record_line(coverage, state, debugInfo);
    }

    if (coverage != ZR_NULL &&
        coverage->previous_hook != ZR_NULL &&
        debugInfo != ZR_NULL &&
        (coverage->previous_mask & zr_debug_coverage_event_mask(debugInfo->event)) != 0u) {
        coverage->previous_hook(state, debugInfo);
    }
}

ZR_DEBUG_API void ZrDebug_Coverage_Init(ZrDebugCoverage *coverage) {
    if (coverage == ZR_NULL) {
        return;
    }
    memset(coverage, 0, sizeof(*coverage));
}

ZR_DEBUG_API void ZrDebug_Coverage_Reset(ZrDebugCoverage *coverage) {
    if (coverage == ZR_NULL) {
        return;
    }
    coverage->line_count = 0u;
}

/* CLI 在执行入口前注册静态可执行行，以便报告包含未命中的行。 */
ZR_DEBUG_API TZrBool ZrDebug_Coverage_RegisterFunction(ZrDebugCoverage *coverage,
                                                       const struct SZrFunction *function) {
    TZrSize activeLineCount;
    TZrUInt32 *activeLines;
    TZrSize index;

    if (coverage == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    activeLineCount = ZrCore_Debug_GetActiveLines(function, ZR_NULL, 0u);
    if (activeLineCount == 0u) {
        return ZR_TRUE;
    }

    activeLines = (TZrUInt32 *)malloc(sizeof(*activeLines) * activeLineCount);
    if (activeLines == ZR_NULL) {
        return ZR_FALSE;
    }

    activeLineCount = ZrCore_Debug_GetActiveLines(function, activeLines, activeLineCount);
    for (index = 0u; index < activeLineCount; index++) {
        if (zr_debug_coverage_add_line(coverage, function, activeLines[index], ZR_TRUE, ZR_FALSE) == (TZrSize)-1) {
            free(activeLines);
            return ZR_FALSE;
        }
    }

    free(activeLines);
    return ZR_TRUE;
}

ZR_DEBUG_API TZrBool ZrDebug_Coverage_RegisterFunctionTree(ZrDebugCoverage *coverage,
                                                           const struct SZrFunction *function) {
    TZrUInt32 index;

    if (coverage == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrDebug_Coverage_RegisterFunction(coverage, function)) {
        return ZR_FALSE;
    }

    for (index = 0u; index < function->childFunctionLength; index++) {
        if (!ZrDebug_Coverage_RegisterFunctionTree(coverage, &function->childFunctionList[index])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/* 与 VM 共享唯一 debug hook 槽位；保留旧配置后扩展 LINE 事件。 */
/* TODO: 若活动期间其它组件调用 ZrCore_Debug_SetHook，Stop 会恢复过期快照并覆盖
 * 新 hook；核对嵌入式宿主是否允许动态换 hook，再定义串行化或栈式所有权。 */
ZR_DEBUG_API TZrBool ZrDebug_Coverage_Start(ZrDebugCoverage *coverage, struct SZrState *state) {
    if (coverage == ZR_NULL || state == ZR_NULL || coverage->active) {
        return ZR_FALSE;
    }

    coverage->state = state;
    coverage->previous_hook = ZrCore_Debug_GetHook(state);
    coverage->previous_mask = ZrCore_Debug_GetHookMask(state);
    coverage->previous_count = ZrCore_Debug_GetHookCount(state);
    if (coverage->previous_hook == ZR_NULL && coverage->previous_mask != 0u) {
        coverage->state = ZR_NULL;
        coverage->previous_mask = 0u;
        coverage->previous_count = 0u;
        return ZR_FALSE;
    }
    if (!zr_debug_coverage_register_active(coverage)) {
        coverage->state = ZR_NULL;
        coverage->previous_hook = ZR_NULL;
        coverage->previous_mask = 0u;
        coverage->previous_count = 0u;
        return ZR_FALSE;
    }

    /* hook 一旦安装即可从任意 LINE 事件回查会话，故先发布 active 链表状态。 */
    coverage->active = ZR_TRUE;
    ZrCore_Debug_SetHook(state,
                         zr_debug_coverage_hook,
                         coverage->previous_mask | ZR_DEBUG_HOOK_MASK_LINE,
                         coverage->previous_count);
    return ZR_TRUE;
}

/* CLI 写报告前先停止采集，确保结果数组不再被 hook 改动。 */
ZR_DEBUG_API void ZrDebug_Coverage_Stop(ZrDebugCoverage *coverage) {
    SZrState *state;

    if (coverage == ZR_NULL || !coverage->active) {
        return;
    }

    state = coverage->state;
    if (state != ZR_NULL) {
        ZrCore_Debug_SetHook(state, coverage->previous_hook, coverage->previous_mask, coverage->previous_count);
    }
    zr_debug_coverage_unregister_active(coverage);
    coverage->state = ZR_NULL;
    coverage->previous_hook = ZR_NULL;
    coverage->previous_mask = 0u;
    coverage->previous_count = 0u;
    coverage->active = ZR_FALSE;
}

ZR_DEBUG_API void ZrDebug_Coverage_Destroy(ZrDebugCoverage *coverage) {
    if (coverage == ZR_NULL) {
        return;
    }
    if (coverage->active) {
        ZrDebug_Coverage_Stop(coverage);
    }
    free(coverage->lines);
    ZrDebug_Coverage_Init(coverage);
}

ZR_DEBUG_API TZrSize ZrDebug_Coverage_GetLineCount(const ZrDebugCoverage *coverage) {
    return coverage != ZR_NULL ? coverage->line_count : 0u;
}

ZR_DEBUG_API const ZrDebugCoverageLine *ZrDebug_Coverage_GetLine(const ZrDebugCoverage *coverage, TZrSize index) {
    if (coverage == ZR_NULL || index >= coverage->line_count) {
        return ZR_NULL;
    }
    return &coverage->lines[index];
}
