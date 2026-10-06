//
// 画像存储生命周期、稳定名称映射及退出时的 JSON 报告输出。
//

#include "zr_vm_core/profile.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
/* 每个线程只缓存当前 global 的借用 profile 指针；实际记录存储由 global 持有。 */
/**
 * ## Summary
 * 缓存调用OS线程的当前global画像借用。
 *
 * ## Value model
 * 当前OS线程的画像借用指针，初始/解绑值为空；存储是TLS，目标runtime仍由global拥有。
 *
 * ## Invariants and lifecycle
 * SetCurrentState替换；Shutdown只清本线程同值绑定；TLS不保活runtime/GC对象。MSVC热路径经Current导出函数读取，其他编译器可直接读取数据声明；线程局部指针不保活目标。
 *
 * ## References
 * 当前线程画像指针由此声明或定义，Current和记录inline读取它。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:637`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:701`。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/profile.h:702`。
 *
 */
ZR_PROFILE_THREAD_LOCAL SZrProfileRuntime *g_zr_profile_current = ZR_NULL;
/* 以下名称表供计数数组索引和 JSON 报告共用；各表顺序须与 profile.h 的对应 enum 一致。 */
/**
 * ## Summary
 * 为helper报告名称提供只读索引映射。
 *
 * ## Value model
 * 静态只读字符串指针数组，位置与对应enum或opcode声明序列一一对应，数组上限来自相同枚举。
 *
 * ## Invariants and lifecycle
 * 长度为ZR_PROFILE_HELPER_ENUM_MAX；顺序须与对应enum一致，字符串全程静态存活。名称返回者只借用静态存储，调用者不得改写或释放。
 *
 * ## References
 * 画像计数数组与名称数组按枚举顺序配对供报告写入。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:750`。
 *
 */
static const TZrChar *const CZrProfileHelperNames[ZR_PROFILE_HELPER_ENUM_MAX] = {
        "value_copy",
        "value_reset_null",
        "stack_get_value",
        "precall",
        "get_member",
        "set_member",
        "get_by_index",
        "set_by_index",
        "value_construct",
        "frame_value_slot_direct",
        "frame_value_slot_checked",
        "frame_value_parameter_copy_direct",
        "frame_value_parameter_copy_checked",
        "frame_value_parameter_copy_empty",
        "frame_value_parameter_layout_visit",
        "frame_value_drop_direct",
        "frame_value_drop_checked",
        "frame_value_initialization_direct",
        "frame_value_initialization_checked",
        "frame_value_copy_probe"
};

/**
 * ## Summary
 * 为慢路径报告名称提供只读索引映射。
 *
 * ## Value model
 * 静态只读字符串指针数组，位置与对应enum或opcode声明序列一一对应，数组上限来自相同枚举。
 *
 * ## Invariants and lifecycle
 * 长度为ZR_PROFILE_SLOWPATH_ENUM_MAX；顺序须与对应enum一致，字符串全程静态存活。名称返回者只借用静态存储，调用者不得改写或释放。
 *
 * ## References
 * 画像计数数组与名称数组按枚举顺序配对供报告写入。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:773`。
 *
 */
static const TZrChar *const CZrProfileSlowPathNames[ZR_PROFILE_SLOWPATH_ENUM_MAX] = {
        "meta_fallback",
        "callsite_cache_lookup",
        "callsite_cache_miss",
        "protect_e",
        "protect_eh",
        "protect_esh",
        "meta_call_prepare"
};

/**
 * ## Summary
 * 为相邻加载/算术探针名称提供只读索引映射。
 *
 * ## Value model
 * 静态只读字符串指针数组，位置与对应enum或opcode声明序列一一对应，数组上限来自相同枚举。
 *
 * ## Invariants and lifecycle
 * 长度为ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX；顺序须与对应enum一致，字符串全程静态存活。名称返回者只借用静态存储，调用者不得改写或释放。
 *
 * ## References
 * 画像计数数组与名称数组按枚举顺序配对供报告写入。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:796`。
 *
 */
static const TZrChar *const CZrProfileQuickeningProbeNames[ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX] = {
        "get_stack_typed_arithmetic",
        "get_constant_typed_arithmetic"
};

/**
 * ## Summary
 * 为内存事件计量名称提供只读索引映射。
 *
 * ## Value model
 * 静态只读字符串指针数组，位置与对应enum或opcode声明序列一一对应，数组上限来自相同枚举。
 *
 * ## Invariants and lifecycle
 * 长度为ZR_PROFILE_MEMORY_ENUM_MAX；顺序须与对应enum一致，字符串全程静态存活。名称返回者只借用静态存储，调用者不得改写或释放。
 *
 * ## References
 * 画像计数数组与名称数组按枚举顺序配对供报告写入。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:821`。
 *
 */
static const TZrChar *const CZrProfileMemoryMetricNames[ZR_PROFILE_MEMORY_ENUM_MAX] = {
        "allocation_count",
        "allocation_bytes",
        "value_copy_bytes",
        "write_barrier_count",
        "minor_collection_count",
        "full_collection_count",
        "mark_object_count",
        "rewrite_object_count",
        "promoted_bytes",
        "raw_int_hit_count",
        "node_map_materialization_count",
        "raw_node_sync_count",
        "member_cache_hit_count",
        "member_cache_miss_count",
        "member_cache_invalidation_count",
        "scan_bytes",
        "member_cache_monomorphic_hit_count",
        "member_cache_polymorphic_hit_count",
        "member_cache_megamorphic_hit_count",
        "member_cache_meta_fallback_count"
};

/**
 * ## Summary
 * 将权威opcode列表项变成名称字符串。
 *
 * ## Parameters
 * @param[in] OPCODE ZR_INSTRUCTION_DECLARE传入的opcode标识符token；用于stringize名称，非运行时表达式或资源。
 *
 * ## Returns
 * 展开为一个字符串常量及初始化逗号，交付CZrProfileInstructionNames元素；不执行运行时调用。
 *
 * ## Constraints and lifecycle
 * 只在名称数组的ZR_INSTRUCTION_DECLARE展开期间定义，随后undef；序列不得独立于opcode枚举重排。
 *
 * ## References
 * opcode名称数组在此把权威指令标识展开为字符串项。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:191`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:193`。
 *
 */
#define ZR_PROFILE_INSTRUCTION_NAME_DECLARE(OPCODE) #OPCODE,
/**
 * ## Summary
 * 为opcode拼写名称提供只读索引映射。
 *
 * ## Value model
 * 静态只读字符串指针数组，位置与对应enum或opcode声明序列一一对应，数组上限来自相同枚举。
 *
 * ## Invariants and lifecycle
 * 长度为ZR_INSTRUCTION_ENUM(ENUM_MAX)；顺序须与对应enum一致，字符串全程静态存活。名称返回者只借用静态存储，调用者不得改写或释放。
 *
 * ## References
 * 画像计数数组与名称数组按枚举顺序配对供报告写入。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:845`。
 *
 */
static const TZrChar *const CZrProfileInstructionNames[ZR_INSTRUCTION_ENUM(ENUM_MAX)] = {
        ZR_INSTRUCTION_DECLARE(ZR_PROFILE_INSTRUCTION_NAME_DECLARE)
};
#undef ZR_PROFILE_INSTRUCTION_NAME_DECLARE
/* 未设置或空值关闭；只有 0、false、FALSE 关闭，其余非空环境值都开启。 */
/**
 * ## Summary
 * 解释四个环境开关的启用状态。
 *
 * ## Parameters
 * @param[in] value 可空借用的零结尾环境值；非空指针在读取期间有效，函数不留存或改写它。
 *
 * ## Returns
 * @return 空/空串及精确0、false、FALSE为false，其余非空值为true；调用方据此决定是否分配与记录。
 *
 * ## Constraints and lifecycle
 * 不解析一般布尔文本或数值；环境缓冲区只在本调用借用。
 *
 * ## References
 * GlobalInit在此把PROFILE环境值转换为记录开关。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:584`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:585`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:586`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static TZrBool profile_env_enabled(const TZrChar *value) {
    if (value == ZR_NULL || value[0] == '\0') {
        return ZR_FALSE;
    }

    return !(strcmp(value, "0") == 0 || strcmp(value, "false") == 0 || strcmp(value, "FALSE") == 0);
}
/* 环境字符串复制到 profileRuntime 自有存储，避免报告依赖后续 getenv 缓冲区状态。 */
/**
 * ## Summary
 * 复制OUT/CASE/MODE，摆脱getenv存储有效期。
 *
 * ## Parameters
 * @param[in] name 非空零结尾环境变量名；当前调用者是固定OUT/CASE/MODE名称，name读取期间有效。
 *
 * ## Returns
 * @return 非空环境值的native malloc副本，调用方负责free；未设置/空值/分配失败返回空，不能由空区分这些原因。
 *
 * ## Constraints and lifecycle
 * 不转移getenv缓冲区；复制长度含终止符，报告只借用副本到Shutdown；不进行JSON转义。
 *
 * ## References
 * GlobalInit在此复制OUT、CASE和MODE环境文本并交给runtime所有。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:600`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:601`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:602`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static TZrChar *profile_dup_env(const TZrChar *name) {
    const TZrChar *value = getenv(name);
    TZrSize length;
    TZrChar *copy;

    if (value == ZR_NULL || value[0] == '\0') {
        return ZR_NULL;
    }

    length = strlen(value);
    copy = (TZrChar *)malloc(length + 1u);
    if (copy == ZR_NULL) {
        return ZR_NULL;
    }
    memcpy(copy, value, length + 1u);
    return copy;
}
/* 按 enum 索引输出非零计数；稳定名称由对应 Getter 提供。 */
/**
 * ## Summary
 * 用匹配的名称回调输出一个计数数组的非零项。
 *
 * ## Parameters
 * @param[in,out] file 非空已打开可写FILE借用；调用期间有效，函数不fclose。
 * @param[in] sectionName 非空零结尾静态段名；当前调用者传instructions/helpers/slowpaths/quickening_probes/memory。
 * @param[in] count 匹配values与名称类别的枚举上限；当前各上限能被uint32 index表示。
 * @param[in] nameGetter 非空同步名称回调；输入index来自[0,count)，返回非空零结尾只读名称，至少保持到本次fprintf完成；函数不保留或释放。
 * @param[in] values 至少count项的只读uint64数组借用；与nameGetter类别配对，整个遍历期间有效且上游已停止写入。
 *
 * ## Returns
 * 向file写一段非零计数数组；零项不派发名称回调，逗号依据已输出项连接；I/O失败不通过返回值交付。
 *
 * ## Constraints and lifecycle
 * 真正回调派发是nameGetter(index)，注册实参并非直接调用。报告前须结束写入；只消费数组与静态名称，不复制画像。
 *
 * ## References
 * 报告writer在此遍历非零计数并同步派发名称回调。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:513`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:519`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:525`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static void profile_write_counts(FILE *file,
                                 const TZrChar *sectionName,
                                 TZrUInt64 count,
                                 const TZrChar *(*nameGetter)(TZrUInt32),
                                 const TZrUInt64 *values) {
    TZrUInt32 index;
    TZrBool emitted = ZR_FALSE;

    fprintf(file, "  \"%s\": [\n", sectionName);
    for (index = 0; index < count; index++) {
        if (values[index] == 0u) {
            continue;
        }
        if (emitted) {
            fputs(",\n", file);
        }
        fprintf(file,
                "    {\"name\": \"%s\", \"count\": %" PRIu64 "}",
                nameGetter(index),
                values[index]);
        emitted = ZR_TRUE;
    }
    if (emitted) {
        fputc('\n', file);
    }
    fputs("  ]", file);
}

/**
 * ## Summary
 * 将报告数组的uint32索引适配为HelperKindName所需enum。
 *
 * ## Parameters
 * @param[in] index helpers段非零helperCounts项的索引，注册的count=ZR_PROFILE_HELPER_ENUM_MAX保证位于合法enum范围；纯值不转移资源。
 *
 * ## Returns
 * @return 经EZrProfileHelperKind转换后由ZrCore_Profile_HelperKindName取得的静态只读名称，调用方不free，保持到报告消费结束。
 *
 * ## Constraints and lifecycle
 * profile_write_report以helperCounts配对此指针；profile_write_counts同步nameGetter(index)派发。此适配器只能与ZR_PROFILE_HELPER_ENUM_MAX及helperCounts配对，nameGetter(index)仅在values[index]非零时调用，返回的静态名称不转移。
 *
 * ## References
 * 报告writer在此注册名称适配器，并由profile_write_counts同步派发。
 * - 真实调用链：注册 `zr_vm_core/src/zr_vm_core/profile.c:522` 的 helpers 名称适配器；`zr_vm_core/src/zr_vm_core/profile.c:306` 以 `nameGetter(index)` 派发，为非零 helperCounts 项写报告名称。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static const TZrChar *profile_helper_name_getter(TZrUInt32 index) {
    return ZrCore_Profile_HelperKindName((EZrProfileHelperKind)index);
}

/**
 * ## Summary
 * 将报告数组的uint32索引适配为SlowPathKindName所需enum。
 *
 * ## Parameters
 * @param[in] index slowpaths段非零slowPathCounts项的索引，注册的count=ZR_PROFILE_SLOWPATH_ENUM_MAX保证位于合法enum范围；纯值不转移资源。
 *
 * ## Returns
 * @return 经EZrProfileSlowPathKind转换后由ZrCore_Profile_SlowPathKindName取得的静态只读名称，调用方不free，保持到报告消费结束。
 *
 * ## Constraints and lifecycle
 * profile_write_report以slowPathCounts配对此指针；profile_write_counts同步nameGetter(index)派发。此适配器只能与ZR_PROFILE_SLOWPATH_ENUM_MAX及slowPathCounts配对，nameGetter(index)仅在values[index]非零时调用，返回的静态名称不转移。
 *
 * ## References
 * 报告writer在此注册名称适配器，并由profile_write_counts同步派发。
 * - 真实调用链：注册 `zr_vm_core/src/zr_vm_core/profile.c:528` 的 slowpaths 名称适配器；`zr_vm_core/src/zr_vm_core/profile.c:306` 以 `nameGetter(index)` 派发，为非零 slowPathCounts 项写报告名称。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static const TZrChar *profile_slow_path_name_getter(TZrUInt32 index) {
    return ZrCore_Profile_SlowPathKindName((EZrProfileSlowPathKind)index);
}

/**
 * ## Summary
 * 将报告数组的uint32索引适配为QuickeningProbeKindName所需enum。
 *
 * ## Parameters
 * @param[in] index quickening_probes段非零quickeningProbeCounts项的索引，注册的count=ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX保证位于合法enum范围；纯值不转移资源。
 *
 * ## Returns
 * @return 经EZrProfileQuickeningProbeKind转换后由ZrCore_Profile_QuickeningProbeKindName取得的静态只读名称，调用方不free，保持到报告消费结束。
 *
 * ## Constraints and lifecycle
 * profile_write_report以quickeningProbeCounts配对此指针；profile_write_counts同步nameGetter(index)派发。此适配器只能与ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX及quickeningProbeCounts配对，nameGetter(index)仅在values[index]非零时调用，返回的静态名称不转移。
 *
 * ## References
 * 报告writer在此注册名称适配器，并由profile_write_counts同步派发。
 * - 真实调用链：注册 `zr_vm_core/src/zr_vm_core/profile.c:534` 的 quickening_probes 名称适配器；`zr_vm_core/src/zr_vm_core/profile.c:306` 以 `nameGetter(index)` 派发，为非零 quickeningProbeCounts 项写报告名称。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static const TZrChar *profile_quickening_probe_name_getter(TZrUInt32 index) {
    return ZrCore_Profile_QuickeningProbeKindName((EZrProfileQuickeningProbeKind)index);
}

/**
 * ## Summary
 * 将报告数组的uint32索引适配为MemoryMetricKindName所需enum。
 *
 * ## Parameters
 * @param[in] index memory段非零memoryMetricCounts项的索引，注册的count=ZR_PROFILE_MEMORY_ENUM_MAX保证位于合法enum范围；纯值不转移资源。
 *
 * ## Returns
 * @return 经EZrProfileMemoryMetricKind转换后由ZrCore_Profile_MemoryMetricKindName取得的静态只读名称，调用方不free，保持到报告消费结束。
 *
 * ## Constraints and lifecycle
 * profile_write_report以memoryMetricCounts配对此指针；profile_write_counts同步nameGetter(index)派发。此适配器只能与ZR_PROFILE_MEMORY_ENUM_MAX及memoryMetricCounts配对，nameGetter(index)仅在values[index]非零时调用，返回的静态名称不转移。
 *
 * ## References
 * 报告writer在此注册名称适配器，并由profile_write_counts同步派发。
 * - 真实调用链：注册 `zr_vm_core/src/zr_vm_core/profile.c:540` 的 memory 名称适配器；`zr_vm_core/src/zr_vm_core/profile.c:306` 以 `nameGetter(index)` 派发，为非零 memoryMetricCounts 项写报告名称。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static const TZrChar *profile_memory_metric_name_getter(TZrUInt32 index) {
    return ZrCore_Profile_MemoryMetricKindName((EZrProfileMemoryMetricKind)index);
}

/**
 * ## Summary
 * 将报告数组的uint32索引适配为InstructionName所需enum。
 *
 * ## Parameters
 * @param[in] index instructions段非零instructionCounts项的索引，注册的count=ZR_INSTRUCTION_ENUM(ENUM_MAX)保证位于合法enum范围；纯值不转移资源。
 *
 * ## Returns
 * @return 经EZrInstructionCode转换后由ZrCore_Profile_InstructionName取得的静态只读名称，调用方不free，保持到报告消费结束。
 *
 * ## Constraints and lifecycle
 * profile_write_report以instructionCounts配对此指针；profile_write_counts同步nameGetter(index)派发。此适配器只能与ZR_INSTRUCTION_ENUM(ENUM_MAX)及instructionCounts配对，nameGetter(index)仅在values[index]非零时调用，返回的静态名称不转移。
 *
 * ## References
 * 报告writer在此注册名称适配器，并由profile_write_counts同步派发。
 * - 真实调用链：注册 `zr_vm_core/src/zr_vm_core/profile.c:516` 的 instructions 名称适配器；`zr_vm_core/src/zr_vm_core/profile.c:306` 以 `nameGetter(index)` 派发，为非零 instructionCounts 项写报告名称。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static const TZrChar *profile_instruction_name_getter(TZrUInt32 index) {
    return ZrCore_Profile_InstructionName((EZrInstructionCode)index);
}
/* 环形样本满时从下一写入位置开始输出，恢复 oldest-to-newest 顺序。 */
/**
 * ## Summary
 * 输出暂停累计量和按时间恢复的有界样本环。
 *
 * ## Parameters
 * @param[in,out] file 非空可写FILE借用，调用期间有效，不在此关闭。
 * @param[in] runtime 非空只读global画像借用；pauseSampleCount<=256、pauseSampleNext<256，读遍历期间不得并发修改或释放。
 *
 * ## Returns
 * 写累计pause字段与有效样本；满环从next输出最旧项，未满从0开始；只输出最近容量窗口。
 *
 * ## Constraints and lifecycle
 * 统计单位微秒，未满窗口外的项不消费；报告不读钟或锁，不使上游solewriter TODO成立。
 *
 * ## References
 * 报告writer在此输出暂停累计量和固定容量的最近样本。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:543`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static void profile_write_pause_samples(FILE *file, const SZrProfileRuntime *runtime) {
    TZrUInt32 startIndex = runtime->pauseSampleCount == ZR_PROFILE_PAUSE_SAMPLE_CAPACITY
                                   ? runtime->pauseSampleNext
                                   : 0u;

    fputs("  \"pause\": {\n", file);
    fprintf(file, "    \"count\": %" PRIu64 ",\n", runtime->pauseCount);
    fprintf(file, "    \"total_us\": %" PRIu64 ",\n", runtime->pauseTotalUs);
    fprintf(file, "    \"max_us\": %" PRIu64 ",\n", runtime->pauseMaxUs);
    fputs("    \"samples_us\": [", file);
    for (TZrUInt32 index = 0u; index < runtime->pauseSampleCount; index++) {
        TZrUInt32 sampleIndex = (startIndex + index) % ZR_PROFILE_PAUSE_SAMPLE_CAPACITY;
        fprintf(file, "%s%" PRIu64, index == 0u ? "" : ", ", runtime->pauseSamples[sampleIndex]);
    }
    fputs("]\n  }", file);
}
/* BUG: 启用任一PROFILE开关并提供合法OUT时，CASE/MODE的非空环境值被原样复制，
 * GlobalState_Free可达此处；例如CASE含双引号会经下面%s生成无效JSON（静态可达链，未运行探针）。
 * TODO: OUT复制失败、fopen失败或写入/fclose失败没有向调用方报告；
 * 下一步核对run_performance_suite和benchmark_task3_case_assembly是否要求画像采集失败使基准失败。 */
/**
 * ## Summary
 * 销毁global时输出环境标签、开关、五类计数与暂停摘要。
 *
 * ## Parameters
 * @param[in] runtime 可空只读画像借用；非空时outputPath、caseName、modeName为画像native自有文本，报告期间必须有效。
 *
 * ## Returns
 * 空画像/未请求OUT/无路径/fopen失败时直接返回；成功open后尝试写JSON并fclose，I/O错误不传播。
 *
 * ## Constraints and lifecycle
 * BUG:合法非空CASE/MODE中的引号/控制字节可经Global_New→Init、Destroy→Global_Free→Shutdown到原始%s形成无效JSON，已证实静态可达，runtime未运行。TODO:核对performance suite与benchmark采集失败策略。报告在释放文本之前调用。
 *
 * ## References
 * GlobalShutdown在释放runtime文本前在此写出JSON报告。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:642`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static void profile_write_report(const SZrProfileRuntime *runtime) {
    FILE *file;

    if (runtime == ZR_NULL || !runtime->hasOutputPath || runtime->outputPath == ZR_NULL) {
        return;
    }

    file = fopen(runtime->outputPath, "wb");
    if (file == ZR_NULL) {
        return;
    }

    fputs("{\n", file);
    fprintf(file,
            "  \"case\": \"%s\",\n",
            runtime->hasCaseName && runtime->caseName != ZR_NULL ? runtime->caseName : "");
    fprintf(file,
            "  \"mode\": \"%s\",\n",
            runtime->hasModeName && runtime->modeName != ZR_NULL ? runtime->modeName : "");
    fprintf(file, "  \"record_instructions\": %s,\n", runtime->recordInstructions ? "true" : "false");
    fprintf(file, "  \"record_slowpaths\": %s,\n", runtime->recordSlowPaths ? "true" : "false");
    fprintf(file, "  \"record_helpers\": %s,\n", runtime->recordHelpers ? "true" : "false");
    fprintf(file, "  \"record_memory\": %s,\n", runtime->recordMemory ? "true" : "false");
    profile_write_counts(file,
                         "instructions",
                         ZR_INSTRUCTION_ENUM(ENUM_MAX),
                         profile_instruction_name_getter,
                         runtime->instructionCounts);
    fputs(",\n", file);
    profile_write_counts(file,
                         "helpers",
                         ZR_PROFILE_HELPER_ENUM_MAX,
                         profile_helper_name_getter,
                         runtime->helperCounts);
    fputs(",\n", file);
    profile_write_counts(file,
                         "slowpaths",
                         ZR_PROFILE_SLOWPATH_ENUM_MAX,
                         profile_slow_path_name_getter,
                         runtime->slowPathCounts);
    fputs(",\n", file);
    profile_write_counts(file,
                         "quickening_probes",
                         ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX,
                         profile_quickening_probe_name_getter,
                         runtime->quickeningProbeCounts);
    fputs(",\n", file);
    profile_write_counts(file,
                         "memory",
                         ZR_PROFILE_MEMORY_ENUM_MAX,
                         profile_memory_metric_name_getter,
                         runtime->memoryMetricCounts);
    fputs(",\n", file);
    profile_write_pause_samples(file, runtime);
    fputs("\n}\n", file);
    fclose(file);
}

/* 仅新global的初始化路径调用；OUT只是输出位置，四类计数均关闭时不会仅因OUT分配画像。 */
/**
 * ## Summary
 * 为新 global 建立按环境选择的可选计数存储。
 *
 * ## Parameters
 * @param[in,out] global 可空；非空时须为尚未初始化画像的新global，函数将画像发布到profileRuntime；global由调用方持有并活到Shutdown。
 *
 * ## Returns
 * 完成后profileRuntime为native calloc画像或空；四开关全关/calloc失败保持空；环境文本复制失败仅使对应has标志为false。
 *
 * ## Constraints and lifecycle
 * 仅初始化一次，不得覆盖仍存活画像；OUT本身不开启记录，画像及文本由GlobalShutdown释放，未进入VM GC。
 *
 * ## References
 * global创建后在此初始化可选画像及其环境文本。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/global.c:394`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
void ZrCore_Profile_GlobalInit(SZrGlobalState *global) {
    SZrProfileRuntime *runtime;
    const TZrChar *recordInstructions;
    const TZrChar *recordSlowPaths;
    const TZrChar *recordHelpers;
    const TZrChar *recordMemory;

    if (global == ZR_NULL) {
        return;
    }

    global->profileRuntime = ZR_NULL;
    recordInstructions = getenv("ZR_VM_PROFILE_INSTRUCTIONS");
    recordSlowPaths = getenv("ZR_VM_PROFILE_SLOWPATHS");
    recordHelpers = getenv("ZR_VM_PROFILE_HELPERS");
    recordMemory = getenv("ZR_VM_PROFILE_MEMORY");
    if (!profile_env_enabled(recordInstructions) &&
        !profile_env_enabled(recordSlowPaths) &&
        !profile_env_enabled(recordHelpers) &&
        !profile_env_enabled(recordMemory)) {
        return;
    }

    runtime = (SZrProfileRuntime *)calloc(1u, sizeof(*runtime));
    if (runtime == ZR_NULL) {
        return;
    }

    runtime->recordInstructions = profile_env_enabled(recordInstructions);
    runtime->recordSlowPaths = profile_env_enabled(recordSlowPaths);
    runtime->recordHelpers = profile_env_enabled(recordHelpers);
    runtime->recordMemory = profile_env_enabled(recordMemory);
    runtime->outputPath = profile_dup_env("ZR_VM_PROFILE_OUT");
    runtime->caseName = profile_dup_env("ZR_VM_PROFILE_CASE");
    runtime->modeName = profile_dup_env("ZR_VM_PROFILE_MODE");
    runtime->hasOutputPath = (TZrBool)(runtime->outputPath != ZR_NULL);
    runtime->hasCaseName = (TZrBool)(runtime->caseName != ZR_NULL);
    runtime->hasModeName = (TZrBool)(runtime->modeName != ZR_NULL);
    global->profileRuntime = runtime;
}

/**
 * ## Summary
 * 在 global 退出时输出报告并释放计数存储与环境副本。
 *
 * ## Parameters
 * @param[in,out] global 可空；非空时释放其画像并清空profileRuntime，global本身不在此释放。
 *
 * ## Returns
 * 空global/画像无副作用；否则尝试报告、释放三个文本和runtime、清空global指针；报告失败不传播。
 *
 * ## Constraints and lifecycle
 * 所有写入者必须先退出；只解除调用线程等于该runtime的TLS借用，不能跨线程解除。
 *
 * ## References
 * global退出在此触发报告并释放画像所有权。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/global.c:756`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
void ZrCore_Profile_GlobalShutdown(SZrGlobalState *global) {
    SZrProfileRuntime *runtime;

    if (global == ZR_NULL || global->profileRuntime == ZR_NULL) {
        return;
    }

    runtime = global->profileRuntime;
    /* 仅解除本线程借用；其他线程必须已退出或自行解绑，free不会跨线程清理TLS。 */
    if (g_zr_profile_current == runtime) {
        g_zr_profile_current = ZR_NULL;
    }

    /* 先报告后释放环境文本，报告不会借用已释放标签；global的GC清理在上游已完成。 */
    profile_write_report(runtime);
    free(runtime->outputPath);
    free(runtime->caseName);
    free(runtime->modeName);
    free(runtime);
    global->profileRuntime = ZR_NULL;
}

/**
 * ## Summary
 * 把 state 所属 global 的画像借用到调用线程。
 *
 * ## Parameters
 * @param[in] state 可空借用；非空时从state->global取画像，global可空；state/global至少保持有效到本次查询完成。
 *
 * ## Returns
 * 把所属global画像借入调用线程TLS；空state/global清空TLS，不保存或恢复旧绑定。
 *
 * ## Constraints and lifecycle
 * 不保活global，不注册GC根；画像有效期不超过对应Shutdown；切换global须在实际执行线程重新绑定。
 *
 * ## References
 * 执行线程切换state时在此绑定当前线程画像。
 * - 真实用途位置：`tests/core/test_object_shape_transition_cache.c:226`。
 * - 真实用途位置：`tests/core/test_object_shape_transition_cache.c:275`。
 * - 真实用途位置：`tests/core/test_argument_staging_vm.c:10`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
void ZrCore_Profile_SetCurrentState(SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        g_zr_profile_current = ZR_NULL;
        return;
    }

    g_zr_profile_current = state->global->profileRuntime;
}

/**
 * ## Summary
 * 取得调用线程当前绑定的画像借用指针。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 调用线程绑定的画像借用指针，未绑定/未启用时空；调用方不得free，也不得越过该global的Shutdown继续访问。
 *
 * ## Constraints and lifecycle
 * 读TLS不加锁、不保活画像；无自动旧绑定恢复或跨线程清理。
 *
 * ## References
 * 参数暂存和inline埋点在此读取当前线程画像。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/function_argument_staging.c:51`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/function_argument_staging.c:177`。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/profile.h:700`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
SZrProfileRuntime *ZrCore_Profile_Current(void) { return g_zr_profile_current; }

/**
 * ## Summary
 * 从显式 state 查询所属 global 的画像借用指针。
 *
 * ## Parameters
 * @param[in] state 可空借用；state->global可空，非空对象需在查询期间有效。
 *
 * ## Returns
 * @return 所属global画像借用指针，空state/global/未启用时空；不得free，访问期不超过对应Shutdown。
 *
 * ## Constraints and lifecycle
 * 不读写TLS或登记GC根；非空返回不保证并发Shutdown安全。
 *
 * ## References
 * safepoint和显式state埋点在此取得所属画像。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/execution/execution_safepoint.c:98`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/execution/execution_safepoint.c:147`。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/profile.h:734`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
SZrProfileRuntime *ZrCore_Profile_FromState(SZrState *state) {
    return (state != ZR_NULL && state->global != ZR_NULL) ? state->global->profileRuntime : ZR_NULL;
}

/**
 * ## Summary
 * 取得 helper 报告所用的只读稳定名称。
 *
 * ## Parameters
 * @param[in] kind EZrProfileHelperKind纯值；[0, ZR_PROFILE_HELPER_ENUM_MAX)映射稳定名称，范围外映射unknown；查询不要求画像开启。
 *
 * ## Returns
 * @return helpers报告/调用方使用的静态只读名称；非法值返回unknown，调用方不释放。
 *
 * ## Constraints and lifecycle
 * 按同序名称数组读取，无可变资源或指针转移；返回unknown只是名称查询结果，不验证记录器的数组索引。
 *
 * ## References
 * 报告写入和测试通过此函数把helper事件编号转换为静态名称。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:336`。
 * - 真实用途位置：`tests/core/test_value_construction_profile.c:48`。
 * - 真实用途位置：`tests/core/test_frame_slot_layout_lookup.c:360`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
const TZrChar *ZrCore_Profile_HelperKindName(EZrProfileHelperKind kind) {
    return (kind >= 0 && kind < ZR_PROFILE_HELPER_ENUM_MAX) ? CZrProfileHelperNames[kind] : "unknown";
}

/**
 * ## Summary
 * 取得回退/保护路径报告所用的只读名称。
 *
 * ## Parameters
 * @param[in] kind EZrProfileSlowPathKind纯值；[0, ZR_PROFILE_SLOWPATH_ENUM_MAX)映射稳定名称，范围外映射unknown；查询不要求画像开启。
 *
 * ## Returns
 * @return slowpaths报告/调用方使用的静态只读名称；非法值返回unknown，调用方不释放。
 *
 * ## Constraints and lifecycle
 * 按同序名称数组读取，无可变资源或指针转移；返回unknown只是名称查询结果，不验证记录器的数组索引。
 *
 * ## References
 * 报告写入通过此函数把slowpath事件编号转换为静态名称。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:359`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
const TZrChar *ZrCore_Profile_SlowPathKindName(EZrProfileSlowPathKind kind) {
    return (kind >= 0 && kind < ZR_PROFILE_SLOWPATH_ENUM_MAX) ? CZrProfileSlowPathNames[kind] : "unknown";
}

/**
 * ## Summary
 * 取得相邻 load/算术机会探针的只读名称。
 *
 * ## Parameters
 * @param[in] kind EZrProfileQuickeningProbeKind纯值；[0, ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX)映射稳定名称，范围外映射unknown；查询不要求画像开启。
 *
 * ## Returns
 * @return quickening_probes报告/调用方使用的静态只读名称；非法值返回unknown，调用方不释放。
 *
 * ## Constraints and lifecycle
 * 按同序名称数组读取，无可变资源或指针转移；返回unknown只是名称查询结果，不验证记录器的数组索引。
 *
 * ## References
 * 报告写入通过此函数把quickening探针编号转换为静态名称。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:382`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
const TZrChar *ZrCore_Profile_QuickeningProbeKindName(EZrProfileQuickeningProbeKind kind) {
    return (kind >= 0 && kind < ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX) ? CZrProfileQuickeningProbeNames[kind]
                                                                      : "unknown";
}

/**
 * ## Summary
 * 取得内存/对象/GC计量报告的只读名称。
 *
 * ## Parameters
 * @param[in] kind EZrProfileMemoryMetricKind纯值；[0, ZR_PROFILE_MEMORY_ENUM_MAX)映射稳定名称，范围外映射unknown；查询不要求画像开启。
 *
 * ## Returns
 * @return memory报告/调用方使用的静态只读名称；非法值返回unknown，调用方不释放。
 *
 * ## Constraints and lifecycle
 * 按同序名称数组读取，无可变资源或指针转移；返回unknown只是名称查询结果，不验证记录器的数组索引。
 *
 * ## References
 * 报告写入和测试通过此函数把memory事件编号转换为静态名称。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:405`。
 * - 真实用途位置：`tests/core/test_value_construction_profile.c:142`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
const TZrChar *ZrCore_Profile_MemoryMetricKindName(EZrProfileMemoryMetricKind kind) {
    return (kind >= 0 && kind < ZR_PROFILE_MEMORY_ENUM_MAX) ? CZrProfileMemoryMetricNames[kind] : "unknown";
}

/**
 * ## Summary
 * 取得 opcode 的文本拼写，供画像和反汇编共用。
 *
 * ## Parameters
 * @param[in] opcode EZrInstructionCode纯值；[0, ZR_INSTRUCTION_ENUM(ENUM_MAX))映射稳定名称，范围外映射unknown；查询不要求画像开启。
 *
 * ## Returns
 * @return instructions报告/调用方使用的静态只读名称；非法值返回unknown，调用方不释放。
 *
 * ## Constraints and lifecycle
 * 按同序名称数组读取，无可变资源或指针转移；返回unknown只是名称查询结果，不验证记录器的数组索引。
 *
 * ## References
 * 反汇编和报告写入通过此函数把opcode转换为静态名称。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/debug.c:786`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/profile.c:428`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
const TZrChar *ZrCore_Profile_InstructionName(EZrInstructionCode opcode) {
    return (opcode >= 0 && opcode < ZR_INSTRUCTION_ENUM(ENUM_MAX)) ? CZrProfileInstructionNames[opcode] : "unknown";
}
