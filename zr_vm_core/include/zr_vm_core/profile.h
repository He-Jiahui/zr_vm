//
// 按 global 汇总可选事件画像；环境开关在初始化时读取，计数和报告代码始终编译。
//

#ifndef ZR_VM_CORE_PROFILE_H
/**
 * ## Summary
 * 防止重复包含profile声明。
 *
 * ## Value model
 * 头文件包含防重定义哨兵，无运行时值或资源。
 *
 * ## Invariants and lifecycle
 * 无运行时状态；宏仅为头文件包含保护。
 *
 * ## References
 * 头文件防重定义哨兵让包含者只看到一份画像声明。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/profile.h:5`。
 *
 */
#define ZR_VM_CORE_PROFILE_H

#include "zr_vm_core/conf.h"
#include "zr_vm_common/zr_instruction_conf.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

/**
 * ## Summary
 * 允许profile声明引用SZrGlobalState而无需引入完整运行时布局。
 *
 * ## Value model
 * 仅前向声明SZrGlobalState的不完整struct tag，用于画像接口的借用指针签名；完整布局由对应global/state头定义。
 *
 * ## Invariants and lifecycle
 * 此声明不创建对象或引入所有权；具体有效期见接受该指针的GlobalInit/Shutdown或FromState文档。
 *
 * ## References
 * 对象或执行接口以此不完整类型传递借用指针，完整定义在对应公开头文件。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/object/object.c:577`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/object/object.c:600`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/object/object.c:1553`。
 *
 */
struct SZrGlobalState;
/**
 * ## Summary
 * 允许profile声明引用SZrState而无需引入完整运行时布局。
 *
 * ## Value model
 * 仅前向声明SZrState的不完整struct tag，用于画像接口的借用指针签名；完整布局由对应global/state头定义。
 *
 * ## Invariants and lifecycle
 * 此声明不创建对象或引入所有权；具体有效期见接受该指针的GlobalInit/Shutdown或FromState文档。
 *
 * ## References
 * 对象或执行接口以此不完整类型传递借用指针，完整定义在对应公开头文件。
 * - 真实用途位置：`tests/core/test_object_shape_transition_cache.c:17`。
 * - 真实用途位置：`tests/core/test_object_shape_transition_cache.c:34`。
 * - 真实用途位置：`tests/core/test_object_shape_transition_cache.c:90`。
 *
 */
struct SZrState;
/** @brief 统一 profile 指针的线程局部存储修饰符。 */
/**
 * ## Summary
 * 统一当前画像的OS线程局部修饰符。
 *
 * ## Value model
 * 复用ZR_THREAD_LOCAL的平台存储修饰符，用于当前线程的画像借用指针。
 *
 * ## Invariants and lifecycle
 * 展开为ZR_THREAD_LOCAL，修饰借用指针不使其指向的数据线程私有。
 *
 * ## References
 * 平台线程局部修饰符用于保存当前线程的画像借用指针。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:32`。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/profile.h:422`。
 *
 */
#if defined(__ELF__) && (defined(__GNUC__) || defined(__clang__))
/* One pointer in the loader's static TLS reserve; avoids dynamic TLS calls
 * throughout shared-library value/staging helpers. */
#define ZR_PROFILE_THREAD_LOCAL ZR_THREAD_LOCAL __attribute__((tls_model("initial-exec")))
#else
#define ZR_PROFILE_THREAD_LOCAL ZR_THREAD_LOCAL
#endif
/** @brief helperCounts 下标；新增计数项须保留已有数值和报告名称。 */
/**
 * ## Summary
 * 为helperCounts规定稳定的索引分类。
 *
 * ## Value model
 * 从首项0开始的枚举值为相应报告数组下标；合法事件范围[0, ZR_PROFILE_HELPER_ENUM_MAX)，ENUM_MAX仅作数组上限而非事件。
 * 每项单位、埋点边界、尝试/成功区别见各枚举项注释；新增项须保留已有编号和静态名称顺序。
 *
 * ## Invariants and lifecycle
 * 序号与profile.c同序名称表和报告循环一致；ENUM_MAX只作长度。纯值无分配、借用指针或释放责任；记录函数的范围检查不能由名称getter替代。
 *
 * ## References
 * 记录入口、名称转换和报告数组使用同一事件编号。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/object/object.c:511`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/execution/execution_member_access.c:91`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:336`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
typedef enum EZrProfileHelperKind {
    /** @brief 值复制事件；部分快路会直接补计，专用复制入口同时可计字节。@note 统计的是所埋点的值复制，不是任意内存 memcpy。 */
    ZR_PROFILE_HELPER_VALUE_COPY = 0,
    /** @brief 公开空值重置及解释器等价快路事件。@note 公开 ResetAsNull 还会另增 VALUE_CONSTRUCT，不能把两项相加当对象数。 */
    ZR_PROFILE_HELPER_VALUE_RESET_NULL,
    /** @brief 带画像的栈槽取值入口或其内联等价事件。@note NoProfile 栈读取不计；参数 staging 按所替代读值行为补计。 */
    ZR_PROFILE_HELPER_STACK_GET_VALUE,
    /** @brief 调用帧准备入口及 known-call 快路事件。@note 统计准备尝试，不保证目标已经执行或成功返回。 */
    ZR_PROFILE_HELPER_PRECALL,
    /** @brief 成员读取帮助路径及 PIC 等价快路事件。@note 不是全部源码成员表达式次数，未经过该埋点的执行层不计。 */
    ZR_PROFILE_HELPER_GET_MEMBER,
    /** @brief 成员写入帮助路径及 PIC 等价快路事件。@note 与 VALUE_COPY 分项；写入前的准备尝试也可能计入。 */
    ZR_PROFILE_HELPER_SET_MEMBER,
    /** @brief 索引读取帮助路径及解释器等价快路事件。@note 计埋点经过次数；不能据此推断一次读取一定成功。 */
    ZR_PROFILE_HELPER_GET_BY_INDEX,
    /** @brief 索引写入帮助路径及解释器等价快路事件。@note 计帮助路径经过次数，不是容器长度或分配量。 */
    ZR_PROFILE_HELPER_SET_BY_INDEX,
    /** @brief 公开值初始化、空值重置和 FAST_SET 等价事件。@note 记录值槽物化，不等于 GC 管理对象分配。 */
    ZR_PROFILE_HELPER_VALUE_CONSTRUCT,
    /** @brief 帧值槽按 direct 摘要定位的计数。@note dispatch try-direct 只在命中时计；通用 locator 按分支选择计。 */
    ZR_PROFILE_HELPER_FRAME_VALUE_SLOT_DIRECT,
    /** @brief 帧值槽需要检查布局的定位计数。@note 与 direct 分开，描述定位路径而非结果值类型。 */
    ZR_PROFILE_HELPER_FRAME_VALUE_SLOT_CHECKED,
    /** @brief 按 direct 布局复制参数的逐项计数。@note 计参数项，不是函数调用总数；staging 镜像写有自己的复制补计。 */
    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_DIRECT,
    /** @brief 按检查布局复制参数的逐项计数。@note 检查路径与 direct 参数复制互斥分类，失败前的已埋点工作仍可能保留。 */
    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_CHECKED,
    /** @brief 参数数目为零的复制请求计数。@note 无需遍历参数；不隐含一次值复制。 */
    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_COPY_EMPTY,
    /** @brief 参数复制/预检期间访问布局项的工作量。@note 同一参数可能在多轮预检中重复访问，不等于复制参数数。 */
    ZR_PROFILE_HELPER_FRAME_VALUE_PARAMETER_LAYOUT_VISIT,
    /** @brief 整帧值存储采用 direct 清理路径的计数。@note 按进入清理路径计一次，不按每个被释放值计。 */
    ZR_PROFILE_HELPER_FRAME_VALUE_DROP_DIRECT,
    /** @brief 整帧值存储采用检查布局清理路径的计数。@note 无 direct 摘要时走检查路径，不能当独立对象析构次数。 */
    ZR_PROFILE_HELPER_FRAME_VALUE_DROP_CHECKED,
    /** @brief 初始化帧布局存储时采用 direct 摘要的计数。@note 按初始化请求分支计，不按每个清空槽计。 */
    ZR_PROFILE_HELPER_FRAME_VALUE_INITIALIZATION_DIRECT,
    /** @brief 初始化帧布局存储时采用检查布局的计数。@note 调用前提供合法 frame，计数器本身不验证布局。 */
    ZR_PROFILE_HELPER_FRAME_VALUE_INITIALIZATION_CHECKED,
    /** @brief 尝试把值复制到帧槽的探测入口计数。@note 在参数检查前记录，包含未命中的探测；direct 摘要可绕过探测。 */
    ZR_PROFILE_HELPER_FRAME_VALUE_COPY_PROBE,
    /** @brief helper 数组长度和报告循环上界。@note 不是事件种类，不得传给任何记录接口。 */
    ZR_PROFILE_HELPER_ENUM_MAX
} EZrProfileHelperKind;
/** @brief slowPathCounts 下标；新增计数项须保留已有数值和报告名称。 */
/**
 * ## Summary
 * 为slowPathCounts规定稳定的索引分类。
 *
 * ## Value model
 * 从首项0开始的枚举值为相应报告数组下标；合法事件范围[0, ZR_PROFILE_SLOWPATH_ENUM_MAX)，ENUM_MAX仅作数组上限而非事件。
 * 每项单位、埋点边界、尝试/成功区别见各枚举项注释；新增项须保留已有编号和静态名称顺序。
 *
 * ## Invariants and lifecycle
 * 序号与profile.c同序名称表和报告循环一致；ENUM_MAX只作长度。纯值无分配、借用指针或释放责任；记录函数的范围检查不能由名称getter替代。
 *
 * ## References
 * 记录入口、名称转换和报告数组使用同一事件编号。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:359`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:772`。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/profile.h:572`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
typedef enum EZrProfileSlowPathKind {
    /** @brief 建立元操作回退 scratch 上下文的计数。@note 描述进入回退准备，不保证随后元调用成功。 */
    ZR_PROFILE_SLOWPATH_META_FALLBACK = 0,
    /** @brief 通用 meta call-site cache 查询入口计数。@note member 命中快路刻意绕过该通用查询计数。 */
    ZR_PROFILE_SLOWPATH_CALLSITE_CACHE_LOOKUP,
    /** @brief call-site cache 缺失、越界或种类不匹配的计数。@note 多个 lookup helper 共享此指标，不等于全部成员 PIC 未命中。 */
    ZR_PROFILE_SLOWPATH_CALLSITE_CACHE_MISS,
    /** @brief 保存 PC 并发布 stackTop 的保护路径计数。@note 记录保护包裹经过次数，不表示已经抛异常。 */
    ZR_PROFILE_SLOWPATH_PROTECT_E,
    /** @brief 保存 PC 并在表达式后刷新 base/trap 的保护路径计数。@note 该包裹不预先发布 stackTop，不能与 ESH 混为一种状态发布。 */
    ZR_PROFILE_SLOWPATH_PROTECT_EH,
    /** @brief 发布 PC/stackTop 并在表达式后刷新 base/trap 的保护路径计数。@note 计完整保护包裹经过次数，不是异常次数。 */
    ZR_PROFILE_SLOWPATH_PROTECT_ESH,
    /** @brief 为元调用调整栈调用窗口的准备计数。@note 与 META_FALLBACK 分别描述回退上下文和参数移动。 */
    ZR_PROFILE_SLOWPATH_META_CALL_PREPARE,
    /** @brief slow-path 数组长度和报告循环上界。@note 不是慢路径事件，不得作记录下标。 */
    ZR_PROFILE_SLOWPATH_ENUM_MAX
} EZrProfileSlowPathKind;
/** @brief quickeningProbeCounts 下标；当前随 recordInstructions 采样，无独立开关。 */
/**
 * ## Summary
 * 为quickeningProbeCounts规定稳定的索引分类。
 *
 * ## Value model
 * 从首项0开始的枚举值为相应报告数组下标；合法事件范围[0, ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX)，ENUM_MAX仅作数组上限而非事件。
 * 每项单位、埋点边界、尝试/成功区别见各枚举项注释；新增项须保留已有编号和静态名称顺序。
 *
 * ## Invariants and lifecycle
 * 序号与profile.c同序名称表和报告循环一致；ENUM_MAX只作长度。纯值无分配、借用指针或释放责任；记录函数的范围检查不能由名称getter替代。
 *
 * ## References
 * 记录入口、名称转换和报告数组使用同一事件编号。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:382`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:795`。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/profile.h:592`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
typedef enum EZrProfileQuickeningProbeKind {
    /** @brief 同帧连续 GET_STACK 加载槽被下一条有符号算术读取的探针命中。@note 仅 recordInstructions 派发分支采样；不表示已经改写指令。 */
    ZR_PROFILE_QUICKENING_PROBE_GET_STACK_TYPED_ARITHMETIC = 0,
    /** @brief 同帧连续 GET_CONSTANT 加载槽被下一条有符号算术读取的探针命中。@note 要求上一 PC 与当前 PC 相邻且槽被算术消费，不统计任意常量加载。 */
    ZR_PROFILE_QUICKENING_PROBE_GET_CONSTANT_TYPED_ARITHMETIC,
    /** @brief quickening 探针数组长度和报告循环上界。@note 不是可采样探针，不得作记录下标。 */
    ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX
} EZrProfileQuickeningProbeKind;
/** @brief memoryMetricCounts 下标；写入受 recordMemory 开关控制。 */
/**
 * ## Summary
 * 为memoryMetricCounts规定稳定的索引分类。
 *
 * ## Value model
 * 从首项0开始的枚举值为相应报告数组下标；合法事件范围[0, ZR_PROFILE_MEMORY_ENUM_MAX)，ENUM_MAX仅作数组上限而非事件。
 * 每项单位、埋点边界、尝试/成功区别见各枚举项注释；新增项须保留已有编号和静态名称顺序。
 *
 * ## Invariants and lifecycle
 * 序号与profile.c同序名称表和报告循环一致；ENUM_MAX只作长度。纯值无分配、借用指针或释放责任；记录函数的范围检查不能由名称getter替代。
 *
 * ## References
 * 记录入口、名称转换和报告数组使用同一事件编号。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/execution/execution_member_access.c:73`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:405`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:820`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
typedef enum EZrProfileMemoryMetricKind {
    /** @brief 成功创建 GC 管理 raw object 的次数。@note 普通分配和 region 分配共同计；native allocator 辅助内存不在内。 */
    ZR_PROFILE_MEMORY_ALLOCATION_COUNT = 0,
    /** @brief 上述 raw object 创建时的基础分配字节累计。@note 不是进程内存或另外分配的 payload 总量。 */
    ZR_PROFILE_MEMORY_ALLOCATION_BYTES,
    /** @brief 所埋点值复制的 SZrTypeValue 字节累计。@note 部分 dispatch 等价复制只补 helper，故不能认为覆盖所有值复制。 */
    ZR_PROFILE_MEMORY_VALUE_COPY_BYTES,
    /** @brief 公开 GC 写屏障入口经过次数。@note 在执行值屏障前计入，不能等同实际加入 remembered set 的次数。 */
    ZR_PROFILE_MEMORY_WRITE_BARRIER_COUNT,
    /** @brief 代际收集调度选择 minor 分支的次数。@note 不把此值解释成暂停样本数。 */
    ZR_PROFILE_MEMORY_MINOR_COLLECTION_COUNT,
    /** @brief 显式 full、增量 full 与强制 major 入口计数。@note 不等同整个进程全部 GC 周期，也不保证包含所有 major 路径。 */
    ZR_PROFILE_MEMORY_FULL_COLLECTION_COUNT,
    /** @brief mark helper 开始处理一个未被过滤对象的次数。@note 工作量计数，不是 live heap 唯一对象数。 */
    ZR_PROFILE_MEMORY_MARK_OBJECT_COUNT,
    /** @brief 转发重写 helper 处理对象的次数。@note 描述引用重写工作，不是被移动对象字节数。 */
    ZR_PROFILE_MEMORY_REWRITE_OBJECT_COUNT,
    /** @brief minor 晋升成功或原地晋升时的对象基础字节。@note 不包括辅助 payload 大小；移动和原地晋升共用计量。 */
    ZR_PROFILE_MEMORY_PROMOTED_BYTES,
    /** @brief raw canonical 整数读取或迭代快路命中次数。@note 仅实际读到 raw 值的埋点，不是所有整数运算次数。 */
    ZR_PROFILE_MEMORY_RAW_INT_HIT_COUNT,
    /** @brief raw 整数节点物化 helper 的进入次数。@note 与 RAW_NODE_SYNC 一起在参数验证前计，可能包含失败尝试。 */
    ZR_PROFILE_MEMORY_NODE_MAP_MATERIALIZATION_COUNT,
    /** @brief raw 整数存储到 node map 的同步 helper 进入次数。@note 当前与物化计数同处增加，不是已成功同步的节点数。 */
    ZR_PROFILE_MEMORY_RAW_NODE_SYNC_COUNT,
    /** @brief 成员 get/set 缓存快路返回成功时的次数。@note 某些更内联 dispatch 路径另有埋点选择，不能当所有成员访问总数。 */
    ZR_PROFILE_MEMORY_MEMBER_CACHE_HIT_COUNT,
    /** @brief 成员缓存快路均未命中后进入名称回退的次数。@note 与 call-site 种类错误的 slow-path miss 分开。 */
    ZR_PROFILE_MEMORY_MEMBER_CACHE_MISS_COUNT,
    /** @brief 清理 call-site PIC 槽的埋点经过次数。@note TLS 路径归属当前绑定 global；不是 GC 对象回收数。 */
    ZR_PROFILE_MEMORY_MEMBER_CACHE_INVALIDATION_COUNT,
    /** @brief GC scan helper 扫描对象基础大小的累计。@note 不含另外分配的 node-map 条目或 payload。 */
    ZR_PROFILE_MEMORY_SCAN_BYTES,
    /** @brief 成功成员缓存命中时 picSlotCount 不超过1的细分计数。@note 按槽数区间分类，不额外检查 PIC 状态枚举；不是独立 lookup 次数。 */
    ZR_PROFILE_MEMORY_MEMBER_CACHE_MONOMORPHIC_HIT_COUNT,
    /** @brief 成功成员缓存命中时 picSlotCount 大于1且未满容量的细分计数。@note 按命中当时槽数区间分类，不按对象生命周期分类。 */
    ZR_PROFILE_MEMORY_MEMBER_CACHE_POLYMORPHIC_HIT_COUNT,
    /** @brief 成功成员缓存命中时 picSlotCount 达到或超过容量的细分计数。@note 当前选择代码以槽数区间的 else 归类，不检查 mega 状态枚举。 */
    ZR_PROFILE_MEMORY_MEMBER_CACHE_MEGAMORPHIC_HIT_COUNT,
    /** @brief cached meta call 准备路径的后段经过计数。@note 埋点前已做部分准备检查；不是所有 meta 调用或成功完成次数。 */
    ZR_PROFILE_MEMORY_MEMBER_CACHE_META_FALLBACK_COUNT,
    /** @brief memory 数组长度和报告循环上界。@note 不是内存事件，不能传给记录 API。 */
    ZR_PROFILE_MEMORY_ENUM_MAX
} EZrProfileMemoryMetricKind;
/** @brief 暂停时长样本环容量；样本满后覆盖最旧项。 */
/**
 * ## Summary
 * 固定暂停样本环的容量。
 *
 * ## Value model
 * 固定TZrUInt32值256；pause样本数组与取模/饱和计数使用同一容量。
 *
 * ## Invariants and lifecycle
 * 256项，只限制样本保留窗口不限制累计量。
 *
 * ## References
 * 暂停累计量和最近样本环使用同一个固定容量。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:452`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:462`。
 * - 真实用途位置：`tests/core/test_value_construction_profile.c:179`。
 *
 */
#define ZR_PROFILE_PAUSE_SAMPLE_CAPACITY ((TZrUInt32)256u)
/** @brief 单个 GlobalState 的 profile 计数、开关、身份文本副本和有界暂停样本环。 */
/** @note 存储由 global 持有，TLS/state 仅借用；malloc副本不参与VM GC。内存计数的原子加不能为其余字段或销毁提供线程同步。 */
/**
 * ## Summary
 * 承载global画像开关、计数、环境文本和有界暂停样本。
 *
 * ## Fields
 * - `recordInstructions`：解释器逐指令计数与相邻 load/typed-arithmetic 探针开关；从 INSTRUCTIONS 环境值初始化；不会开启其他三类计数。
 * - `recordSlowPaths`：slowPathCounts 记录开关；从 SLOWPATHS 环境值初始化；保护路径计数不是异常报告。
 * - `recordHelpers`：helperCounts 记录开关；从 HELPERS 环境值初始化；普通自增路径并不自动串行化。
 * - `recordMemory`：memoryMetricCounts 与 pause 环记录开关；从 MEMORY 环境值初始化；不会让 profile 自行读时钟。
 * - `hasOutputPath`：输出路径副本是否建立成功；与 outputPath 非空共同决定是否输出；OUT 独自存在不会创建runtime。
 * - `hasCaseName`：caseName 副本是否建立成功；未设置、空值或复制失败使报告 case 使用空字符串。
 * - `hasModeName`：modeName 副本是否建立成功；未设置、空值或复制失败使报告 mode 使用空字符串。
 * - `instructionCounts`：按 opcode 统计解释器经过的指令；只在 recordInstructions 分支普通自增；不是 elapsed time 或 AOT 操作数。
 * - `helperCounts`：按帮助路径种类统计埋点工作量；同时存在普通自增与专用复制入口原子加；整个数组不因局部atomic而线程安全。
 * - `slowPathCounts`：按回退/保护种类统计经过次数；recordSlowPaths 控制普通自增；不含独立错误状态。
 * - `quickeningProbeCounts`：相邻 load 与有符号算术消费机会的探针计数；跟随 recordInstructions；探针命中不执行quickening改写。
 * - `memoryMetricCounts`：内存/对象/GC工作量数组；公共记录接口采用 relaxed atomic 加法；输出须在写入者退出后读取。
 * - `pauseCount`：全部已埋点 GC 阶段样本数；包含 concurrent-major remark 和 compaction；不代表全部 GC 暂停次数。
 * - `pauseTotalUs`：已埋点阶段传入微秒值的总和；无溢出饱和处理；样本覆盖不减少总和。
 * - `pauseMaxUs`：已埋点阶段传入微秒值的最大值；初值0；profile 接收持续时间而不自行读取时钟。
 * - `pauseSamples`：最近固定容量的 GC 阶段微秒样本；被后续样本环形覆盖；原始数组位置不是跨wrap的时间顺序。
 * - `pauseSampleCount`：pauseSamples 中已写入的有效项数；最多256；不是全量 pauseCount。
 * - `pauseSampleNext`：pauseSamples 下次覆盖的位置；始终按容量取模；环满时也是最旧有效样本的位置。
 * - `outputPath`：OUT 环境值的 malloc 字符串副本；由 GlobalShutdown free；wb 写报告会覆盖同路径文件。
 * - `caseName`：CASE 环境值的 malloc 身份标签副本；不作JSON转义；free前只借用于报告。
 * - `modeName`：MODE 环境值的 malloc 执行模式标签副本；不作JSON转义；缺失时报告用空字符串。
 *
 * ## Invariants and lifecycle
 * 正常GlobalInit以calloc零初始化；开关和文本准备完成后再发布到global->profileRuntime，GlobalShutdown先报告后释放native文本与画像。
 * TLS、state和执行上下文只借用，不登记GC根。helper/instruction/slowpath/probe普通计数与pause普通字段不由内存原子加法同步；TODO:从共享domain实际调度和GC调用入口核对并行写入/solewriter协调。
 * pauseSampleCount<=ZR_PROFILE_PAUSE_SAMPLE_CAPACITY，pauseSampleNext<容量；满环next指最旧项，累计量不因覆盖回退。字段/布局是当前C实现，未声明稳定外部ABI。
 *
 * ## References
 * global持有此运行时对象；对象、执行和GC路径通过字段记录画像事件。
 * - 真实用途位置：`tests/core/test_object_shape_transition_cache.c:213`。
 * - 真实用途位置：`tests/core/test_argument_staging_vm.c:39`。
 * - 真实用途位置：`tests/core/test_argument_staging_vm.c:40`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
typedef struct SZrProfileRuntime {
    /** @brief 解释器逐指令计数与相邻 load/typed-arithmetic 探针开关。@note 从 INSTRUCTIONS 环境值初始化；不会开启其他三类计数。 */
    TZrBool recordInstructions;
    /** @brief slowPathCounts 记录开关。@note 从 SLOWPATHS 环境值初始化；保护路径计数不是异常报告。 */
    TZrBool recordSlowPaths;
    /** @brief helperCounts 记录开关。@note 从 HELPERS 环境值初始化；普通自增路径并不自动串行化。 */
    TZrBool recordHelpers;
    /** @brief memoryMetricCounts 与 pause 环记录开关。@note 从 MEMORY 环境值初始化；不会让 profile 自行读时钟。 */
    TZrBool recordMemory;
    /** @brief 输出路径副本是否建立成功。@note 与 outputPath 非空共同决定是否输出；OUT 独自存在不会创建runtime。 */
    TZrBool hasOutputPath;
    /** @brief caseName 副本是否建立成功。@note 未设置、空值或复制失败使报告 case 使用空字符串。 */
    TZrBool hasCaseName;
    /** @brief modeName 副本是否建立成功。@note 未设置、空值或复制失败使报告 mode 使用空字符串。 */
    TZrBool hasModeName;
    /** @brief 按 opcode 统计解释器经过的指令。@note 只在 recordInstructions 分支普通自增；不是 elapsed time 或 AOT 操作数。 */
    TZrUInt64 instructionCounts[ZR_INSTRUCTION_ENUM(ENUM_MAX)];
    /** @brief 按帮助路径种类统计埋点工作量。@note 同时存在普通自增与专用复制入口原子加；整个数组不因局部atomic而线程安全。 */
    TZrUInt64 helperCounts[ZR_PROFILE_HELPER_ENUM_MAX];
    /** @brief 按回退/保护种类统计经过次数。@note recordSlowPaths 控制普通自增；不含独立错误状态。 */
    TZrUInt64 slowPathCounts[ZR_PROFILE_SLOWPATH_ENUM_MAX];
    /** @brief 相邻 load 与有符号算术消费机会的探针计数。@note 跟随 recordInstructions；探针命中不执行quickening改写。 */
    TZrUInt64 quickeningProbeCounts[ZR_PROFILE_QUICKENING_PROBE_ENUM_MAX];
    /** @brief 内存/对象/GC工作量数组。@note 公共记录接口采用 relaxed atomic 加法；输出须在写入者退出后读取。 */
    TZrUInt64 memoryMetricCounts[ZR_PROFILE_MEMORY_ENUM_MAX];
    /** @brief 全部已埋点 GC 阶段样本数。@note 包含 concurrent-major remark 和 compaction；不代表全部 GC 暂停次数。 */
    TZrUInt64 pauseCount;
    /** @brief 已埋点阶段传入微秒值的总和。@note 无溢出饱和处理；样本覆盖不减少总和。 */
    TZrUInt64 pauseTotalUs;
    /** @brief 已埋点阶段传入微秒值的最大值。@note 初值0；profile 接收持续时间而不自行读取时钟。 */
    TZrUInt64 pauseMaxUs;
    /** @brief 最近固定容量的 GC 阶段微秒样本。@note 被后续样本环形覆盖；原始数组位置不是跨wrap的时间顺序。 */
    TZrUInt64 pauseSamples[ZR_PROFILE_PAUSE_SAMPLE_CAPACITY];
    /** @brief pauseSamples 中已写入的有效项数。@note 最多256；不是全量 pauseCount。 */
    TZrUInt32 pauseSampleCount;
    /** @brief pauseSamples 下次覆盖的位置。@note 始终按容量取模；环满时也是最旧有效样本的位置。 */
    TZrUInt32 pauseSampleNext;
    /** @brief OUT 环境值的 malloc 字符串副本。@note 由 GlobalShutdown free；wb 写报告会覆盖同路径文件。 */
    TZrChar *outputPath;
    /** @brief CASE 环境值的 malloc 身份标签副本。@note 不作JSON转义；free前只借用于报告。 */
    TZrChar *caseName;
    /** @brief MODE 环境值的 malloc 执行模式标签副本。@note 不作JSON转义；缺失时报告用空字符串。 */
    TZrChar *modeName;
} SZrProfileRuntime;
/** @brief 当前 OS 线程所绑定的 profileRuntime 借用指针；由 SetCurrentState 维护。 */
/* MSVC 的 Current 热路径经导出函数取TLS；其他编译器直接使用下方数据符号。 */
#if defined(_MSC_VER)
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
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:637`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:701`。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/profile.h:702`。
 *
 */
extern ZR_PROFILE_THREAD_LOCAL SZrProfileRuntime *g_zr_profile_current;
#else
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
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:637`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:701`。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/profile.h:702`。
 *
 */
ZR_CORE_API ZR_PROFILE_THREAD_LOCAL SZrProfileRuntime *g_zr_profile_current;
#endif

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
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/global.c:394`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
ZR_CORE_API void ZrCore_Profile_GlobalInit(struct SZrGlobalState *global); /**< @brief 按环境开关创建可选的 global profile；对一个 global 只初始化一次。 */
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
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/global.c:756`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
ZR_CORE_API void ZrCore_Profile_GlobalShutdown(struct SZrGlobalState *global); /**< @brief 写出可选报告并释放 profile；调用前须停止该 global 的执行线程。 */
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
ZR_CORE_API void ZrCore_Profile_SetCurrentState(struct SZrState *state); /**< @brief 将调用线程绑定到 state 所属 profile；空 state/global 会清除绑定。 */
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
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/function/function_argument_staging.c:51`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/function/function_argument_staging.c:177`。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/profile.h:700`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
ZR_CORE_API SZrProfileRuntime *ZrCore_Profile_Current(void); /**< @brief 返回当前线程绑定的借用 profileRuntime；未启用时为空。 */
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
ZR_CORE_API SZrProfileRuntime *ZrCore_Profile_FromState(struct SZrState *state); /**< @brief 按 state 查找借用 profileRuntime，不读写 TLS 绑定。 */
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
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:336`。
 * - 真实用途位置：`tests/core/test_value_construction_profile.c:48`。
 * - 真实用途位置：`tests/core/test_frame_slot_layout_lookup.c:360`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
ZR_CORE_API const TZrChar *ZrCore_Profile_HelperKindName(EZrProfileHelperKind kind); /**< @brief 返回稳定 helper 报告名；非法值返回 "unknown"。 */
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
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:359`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
ZR_CORE_API const TZrChar *ZrCore_Profile_SlowPathKindName(EZrProfileSlowPathKind kind); /**< @brief 返回稳定 slow-path 报告名；非法值返回 "unknown"。 */
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
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:382`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
ZR_CORE_API const TZrChar *ZrCore_Profile_QuickeningProbeKindName(EZrProfileQuickeningProbeKind kind); /**< @brief 返回稳定 quickening 报告名；非法值返回 "unknown"。 */
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
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:405`。
 * - 真实用途位置：`tests/core/test_value_construction_profile.c:142`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
ZR_CORE_API const TZrChar *ZrCore_Profile_MemoryMetricKindName(EZrProfileMemoryMetricKind kind); /**< @brief 返回稳定内存指标报告名；非法值返回 "unknown"。 */
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
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/debug/debug.c:786`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/runtime/profile.c:428`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
ZR_CORE_API const TZrChar *ZrCore_Profile_InstructionName(EZrInstructionCode opcode); /**< @brief 返回指令枚举对应报告名；非法值返回 "unknown"。 */
/* 热路径调用方传入有效枚举下标；此处不做边界检查。
 * TODO: 同global的helper/instruction/quickening/slow-path存在普通自增，且部分VALUE_COPY会混用原子加。
 * 已知State_MutatorLaunch允许同global次state；仍须从实际共享域调度入口核对上层串行化与同项并发，
 * 再确定数据竞争证据；暂停环另须核对GC协调路径唯一写入者约定。 */
/**
 * @brief 给单个共享计数做原子累加。
 * @note value 须指向有效且满足原子操作对齐要求的64位计数；GNU/Clang 分支为 relaxed，仅保证该计数加法，不发布 runtime 指针或开关。
 */
/**
 * ## Summary
 * 给单个共享计数做原子累加。
 *
 * ## Parameters
 * @param[in,out] value 非空、满足平台原子对齐的有效64位计数指针；调用期间可写，不转移所有权。
 * @param[in] amount 加入的无符号事件量，可为0；单位由被计数指标确定，不解释为时间。
 *
 * ## Returns
 * 只原子累加所指计数，丢弃旧值；不会发布画像指针或改变记录开关。
 *
 * ## Constraints and lifecycle
 * MSVC使用Interlocked；GNU/Clang使用__ATOMIC_RELAXED。只保证单次目标加法，不保护生命周期或混合普通访问。
 *
 * ## References
 * 各记录入口在此对共享memory计数做平台原子加法。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/profile.h:898`。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/profile.h:934`。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/profile.h:971`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static ZR_FORCE_INLINE void ZrCore_Profile_AtomicAdd(TZrUInt64 *value, TZrUInt64 amount) {
#if defined(_MSC_VER)
    (void)_InterlockedExchangeAdd64((volatile long long *)value, (long long)amount);
#else
    __atomic_fetch_add(value, amount, __ATOMIC_RELAXED);
#endif
}

/**
 * @brief 把一个帮助路径事件记到当前线程绑定的 global。
 * @note kind 须为区间[0, HELPER_ENUM_MAX)内的合法种类；仅检查 runtime/recordHelpers，直接普通自增，不检查下标或建立互斥。
 */
/**
 * ## Summary
 * 把一个帮助路径事件记到当前线程绑定的 global。
 *
 * ## Parameters
 * @param[in] kind EZrProfileHelperKind纯值必须位于[0, ZR_PROFILE_HELPER_ENUM_MAX)；本记录器不检查范围。
 *
 * ## Returns
 * 画像非空且recordHelpers开启时普通自增helperCounts对应项一次；关闭时不写，未返回事件成功状态。
 *
 * ## Constraints and lifecycle
 * 画像须活到写入结束；普通写不具线程同步。TODO:从共享domain调度与同项写入入口核对legal并发重叠及上层同步。
 *
 * ## References
 * 对象值、栈和帧路径在此记录helper事件。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/value.h:74`。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/stack.h:128`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/function/function_argument_staging.c:159`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static ZR_FORCE_INLINE void ZrCore_Profile_RecordHelperCurrent(EZrProfileHelperKind kind) {
#if defined(_MSC_VER)
    SZrProfileRuntime *runtime = ZrCore_Profile_Current();
#else
    SZrProfileRuntime *runtime = g_zr_profile_current;
#endif
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordHelpers)) {
        runtime->helperCounts[kind]++;
    }
}

/**
 * @brief 把一个帮助路径事件记到显式 state 所属 global。
 * @note kind 须为区间[0, HELPER_ENUM_MAX)内的合法种类；不依赖 TLS，普通自增，不建立互斥。
 */
/**
 * ## Summary
 * 把一个帮助路径事件记到显式 state 所属 global。
 *
 * ## Parameters
 * @param[in] state 可空借用；所属global可空，画像只借用且不保活；state/global在查询期间有效。
 * @param[in] kind EZrProfileHelperKind纯值必须位于[0, ZR_PROFILE_HELPER_ENUM_MAX)；本记录器不检查范围。
 *
 * ## Returns
 * 画像非空且recordHelpers开启时普通自增helperCounts对应项一次；关闭时不写，未返回事件成功状态。
 *
 * ## Constraints and lifecycle
 * 画像须活到写入结束；普通写不具线程同步。TODO:从共享domain调度与同项写入入口核对legal并发重叠及上层同步。
 *
 * ## References
 * 对象值、栈和帧路径在此记录helper事件。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:1746`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static ZR_FORCE_INLINE void ZrCore_Profile_RecordHelperFromState(struct SZrState *state, EZrProfileHelperKind kind) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordHelpers)) {
        runtime->helperCounts[kind]++;
    }
}

/**
 * @brief 把一次回退或保护路径经过记到当前 TLS 画像。
 * @note kind 须为区间[0, SLOWPATH_ENUM_MAX)内的合法种类；开关关闭或未绑定为空操作，计数不是异常次数。
 */
/**
 * ## Summary
 * 把一次回退或保护路径经过记到当前 TLS 画像。
 *
 * ## Parameters
 * @param[in] kind EZrProfileSlowPathKind纯值必须位于[0, ZR_PROFILE_SLOWPATH_ENUM_MAX)；本记录器不检查范围。
 *
 * ## Returns
 * 画像非空且recordSlowPaths开启时普通自增slowPathCounts对应项一次；关闭时不写，未返回事件成功状态。
 *
 * ## Constraints and lifecycle
 * 画像须活到写入结束；普通写不具线程同步。TODO:从共享domain调度与同项写入入口核对legal并发重叠及上层同步。
 *
 * ## References
 * 执行回退和保护路径在此记录slowpath事件。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/execution/execution_control.c:101`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/execution/execution_tail_call.c:22`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/execution/execution_member_access.c:169`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static ZR_FORCE_INLINE void ZrCore_Profile_RecordSlowPathCurrent(EZrProfileSlowPathKind kind) {
#if defined(_MSC_VER)
    SZrProfileRuntime *runtime = ZrCore_Profile_Current();
#else
    SZrProfileRuntime *runtime = g_zr_profile_current;
#endif
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordSlowPaths)) {
        runtime->slowPathCounts[kind]++;
    }
}

/**
 * @brief 给显式 state 所属 global 的指定 opcode 计一次。
 * @note opcode 须为有效指令，不得传 ENUM_MAX；普通自增。TODO: 全仓首方未发现调用，解释器直接累加数组；核对仓外嵌入者后确认保留用途。
 */
/**
 * ## Summary
 * 给显式 state 所属 global 的指定 opcode 计一次。
 *
 * ## Parameters
 * @param[in] state 可空借用；所属global可空，画像只借用且不保活；state/global在查询期间有效。
 * @param[in] opcode EZrInstructionCode纯值必须位于[0, ZR_INSTRUCTION_ENUM(ENUM_MAX))；本记录器不检查范围。
 *
 * ## Returns
 * 画像非空且recordInstructions开启时普通自增instructionCounts对应项一次；关闭时不写，未返回事件成功状态。
 *
 * ## Constraints and lifecycle
 * 画像须活到写入结束；普通写不具线程同步。TODO:从共享domain调度与同项写入入口核对legal并发重叠及上层同步。 TODO:仓内suffix入口未发现调用；下一核查仓外调用及API保留目的。
 *
 * ## References
 * TODO: 当前全仓首方未发现本接口调用；下一步核查仓外嵌入调用及 API 保留目的。本接口调用的画像查询见 `zr_vm_core/include/zr_vm_core/profile.h:530`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static ZR_FORCE_INLINE void ZrCore_Profile_RecordInstructionFromState(struct SZrState *state, EZrInstructionCode opcode) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordInstructions)) {
        runtime->instructionCounts[opcode]++;
    }
}

/**
 * @brief 给显式 state 所属 global 的慢路径种类计一次。
 * @note kind 须为有效慢路径；普通自增。TODO: 全仓首方未发现调用，当前慢路径埋点用 Current；下一步核对仓外嵌入调用及TLS切换约定。
 */
/**
 * ## Summary
 * 给显式 state 所属 global 的慢路径种类计一次。
 *
 * ## Parameters
 * @param[in] state 可空借用；所属global可空，画像只借用且不保活；state/global在查询期间有效。
 * @param[in] kind EZrProfileSlowPathKind纯值必须位于[0, ZR_PROFILE_SLOWPATH_ENUM_MAX)；本记录器不检查范围。
 *
 * ## Returns
 * 画像非空且recordSlowPaths开启时普通自增slowPathCounts对应项一次；关闭时不写，未返回事件成功状态。
 *
 * ## Constraints and lifecycle
 * 画像须活到写入结束；普通写不具线程同步。TODO:从共享domain调度与同项写入入口核对legal并发重叠及上层同步。 TODO:仓内suffix入口未发现调用；下一核查仓外调用及API保留目的。
 *
 * ## References
 * TODO: 当前全仓首方未发现本接口调用；下一步核查仓外嵌入调用及 API 保留目的。本接口调用的画像查询见 `zr_vm_core/include/zr_vm_core/profile.h:530`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static ZR_FORCE_INLINE void ZrCore_Profile_RecordSlowPathFromState(struct SZrState *state,
                                                                   EZrProfileSlowPathKind kind) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordSlowPaths)) {
        runtime->slowPathCounts[kind]++;
    }
}

/**
 * @brief 查询显式 state 是否开启内存事件记录。
 * @note 空 state/global 或 runtime 返回 false。TODO: 全仓首方未发现调用；核对仓外消费者是否用它跳过昂贵的采样准备。
 */
/**
 * ## Summary
 * 查询显式 state 是否开启内存事件记录。
 *
 * ## Parameters
 * @param[in] state 可空借用，所属global可空；查询期间对象有效。
 *
 * ## Returns
 * @return 画像非空且recordMemory开启时true，否则false；查询结果不保活画像或预留写权限。
 *
 * ## Constraints and lifecycle
 * TODO:全仓首方suffix入口未发现调用；下一核查仓外嵌入接口与是否刻意保留，不能断言死API。
 *
 * ## References
 * TODO: 当前全仓首方未发现本接口调用；下一步核查仓外嵌入调用及 API 保留目的。本接口调用的画像查询见 `zr_vm_core/include/zr_vm_core/profile.h:530`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static ZR_FORCE_INLINE TZrBool ZrCore_Profile_MemoryEnabledFromState(struct SZrState *state) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    return (TZrBool)(runtime != ZR_NULL && runtime->recordMemory);
}

/**
 * @brief 向当前 TLS 画像原子累加指定内存事件量。
 * @note kind 须为有效内存指标；amount 的单位由对应事件决定，AtomicAdd 不保护 runtime 生命周期。
 */
/**
 * ## Summary
 * 向当前 TLS 画像原子累加指定内存事件量。
 *
 * ## Parameters
 * @param[in] kind 有效EZrProfileMemoryMetricKind纯值，范围[0, ZR_PROFILE_MEMORY_ENUM_MAX)；函数不做边界检查。
 * @param[in] amount 非负无符号事件量，可为0；bytes、次数等单位由kind与埋点约定，可能是尝试事件。
 *
 * ## Returns
 * 画像非空且recordMemory开启时，对memoryMetricCounts[kind]做平台原子累加；否则无计数写入。
 *
 * ## Constraints and lifecycle
 * TLS或state只借用global画像；AtomicAdd不保护画像寿命，记录不分配或读钟。
 *
 * ## References
 * 对象和GC路径在此按memory事件记录次数或字节量。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/object/object.c:412`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/execution/execution_member_access.c:431`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h:358`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static ZR_FORCE_INLINE void ZrCore_Profile_RecordMemoryCurrent(EZrProfileMemoryMetricKind kind,
                                                               TZrUInt64 amount) {
#if defined(_MSC_VER)
    SZrProfileRuntime *runtime = ZrCore_Profile_Current();
#else
    SZrProfileRuntime *runtime = g_zr_profile_current;
#endif
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordMemory)) {
        ZrCore_Profile_AtomicAdd(&runtime->memoryMetricCounts[kind], amount);
    }
}

/**
 * @brief 向显式 state 所属画像原子累加指定内存事件量。
 * @note 不依赖 TLS；kind 须为有效内存指标，state/global/runtime须在整个调用期间存活。
 */
/**
 * ## Summary
 * 向显式 state 所属画像原子累加指定内存事件量。
 *
 * ## Parameters
 * @param[in] state 可空借用；global可空，画像访问期不得越过Shutdown。
 * @param[in] kind 有效EZrProfileMemoryMetricKind纯值，范围[0, ZR_PROFILE_MEMORY_ENUM_MAX)；函数不做边界检查。
 * @param[in] amount 非负无符号事件量，可为0；bytes、次数等单位由kind与埋点约定，可能是尝试事件。
 *
 * ## Returns
 * 画像非空且recordMemory开启时，对memoryMetricCounts[kind]做平台原子累加；否则无计数写入。
 *
 * ## Constraints and lifecycle
 * TLS或state只借用global画像；AtomicAdd不保护画像寿命，记录不分配或读钟。
 *
 * ## References
 * 对象和GC路径在此按memory事件记录次数或字节量。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/gc/gc_mark.c:936`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/gc/gc_mark.c:1018`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/gc/gc_object.c:1112`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static ZR_FORCE_INLINE void ZrCore_Profile_RecordMemoryFromState(struct SZrState *state,
                                                                 EZrProfileMemoryMetricKind kind,
                                                                 TZrUInt64 amount) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordMemory)) {
        ZrCore_Profile_AtomicAdd(&runtime->memoryMetricCounts[kind], amount);
    }
}

/**
 * @brief 将一次值复制分别记入当前画像的helper次数和内存字节。
 * @note 两个开关分别控制原子累加；bytes 由调用方传入，不计算深复制对象大小。
 */
/**
 * ## Summary
 * 将一次值复制分别记入当前画像的helper次数和内存字节。
 *
 * ## Parameters
 * @param[in] bytes 本次记录的复制字节事件量，可为0；Value_Copy传sizeof(SZrTypeValue)，不是测得的原生分配量。
 *
 * ## Returns
 * 两个gate独立：recordHelpers累加VALUE_COPY次数1；recordMemory累加VALUE_COPY_BYTES；任一关闭只跳过对应计数。
 *
 * ## Constraints and lifecycle
 * Current由value.h的Value_Copy在真正复制前调用；FromState由object_record_helper使用。记录表示路径事件，不保证随后复制成功；不覆盖所有dispatch复制。
 *
 * ## References
 * 值复制路径在此分别记录复制次数和字节事件。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/execution/execution_member_access.c:2433`。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/value.h:221`。
 * - 真实用途位置：`zr_vm_core/include/zr_vm_core/value.h:297`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static ZR_FORCE_INLINE void ZrCore_Profile_RecordValueCopyCurrent(TZrUInt64 bytes) {
#if defined(_MSC_VER)
    SZrProfileRuntime *runtime = ZrCore_Profile_Current();
#else
    SZrProfileRuntime *runtime = g_zr_profile_current;
#endif
    if (ZR_UNLIKELY(runtime != ZR_NULL)) {
        if (runtime->recordHelpers) {
            ZrCore_Profile_AtomicAdd(&runtime->helperCounts[ZR_PROFILE_HELPER_VALUE_COPY], 1u);
        }
        if (runtime->recordMemory) {
            ZrCore_Profile_AtomicAdd(&runtime->memoryMetricCounts[ZR_PROFILE_MEMORY_VALUE_COPY_BYTES], bytes);
        }
    }
}

/**
 * @brief 将一次值复制分别记入显式state画像的helper次数和内存字节。
 * @note 对象复制 helper 用此入口避免依赖 TLS；两个开关独立，runtime须保持存活。
 */
/**
 * ## Summary
 * 将一次值复制分别记入显式state画像的helper次数和内存字节。
 *
 * ## Parameters
 * @param[in] state 可空借用；对象helper通过显式state定位画像，避免依赖TLS；画像写入期有效。
 * @param[in] bytes 本次记录的复制字节事件量，可为0；Value_Copy传sizeof(SZrTypeValue)，不是测得的原生分配量。
 *
 * ## Returns
 * 两个gate独立：recordHelpers累加VALUE_COPY次数1；recordMemory累加VALUE_COPY_BYTES；任一关闭只跳过对应计数。
 *
 * ## Constraints and lifecycle
 * Current由value.h的Value_Copy在真正复制前调用；FromState由object_record_helper使用。记录表示路径事件，不保证随后复制成功；不覆盖所有dispatch复制。
 *
 * ## References
 * 值复制路径在此分别记录复制次数和字节事件。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/object/object.c:513`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static ZR_FORCE_INLINE void ZrCore_Profile_RecordValueCopyFromState(struct SZrState *state,
                                                                    TZrUInt64 bytes) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL)) {
        if (runtime->recordHelpers) {
            ZrCore_Profile_AtomicAdd(&runtime->helperCounts[ZR_PROFILE_HELPER_VALUE_COPY], 1u);
        }
        if (runtime->recordMemory) {
            ZrCore_Profile_AtomicAdd(&runtime->memoryMetricCounts[ZR_PROFILE_MEMORY_VALUE_COPY_BYTES], bytes);
        }
    }
}
/* recordMemory 开启时由 GC 周期协调路径记录微秒值；累计量保留全部事件，样本环只保留最近的固定容量窗口。 */
/**
 * @brief 记录调用方给出的GC阶段微秒值及最近样本。
 * @note recordMemory 控制；调用方须串行更新暂停环。当前生产入口只覆盖 concurrent-major remark 与 compaction，不是完整 GC 暂停统计。
 */
/**
 * ## Summary
 * 记录调用方给出的GC阶段微秒值及最近样本。
 *
 * ## Parameters
 * @param[in] state 可空借用；所属global可空；非空画像由global持有而非本函数所有。
 * @param[in] durationUs 已经由调用方测量的无符号微秒事件量，可为0；本函数不读时钟。
 *
 * ## Returns
 * recordMemory开启时更新全量次数/总时长/最大值，同时追加或覆盖固定256样本窗口；关闭时不写。
 *
 * ## Constraints and lifecycle
 * 字段为普通写，调用方必须串行。TODO:核对concurrent-major remark与compaction入口的sole-writer协调；不是所有GC暂停或process暂停统计。
 *
 * ## References
 * GC周期在此写入暂停累计量和最近样本。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:2960`。
 * - 真实用途位置：`zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:3028`。
 * - 真实用途位置：`tests/core/test_value_construction_profile.c:181`。
 * - [Profile内存与GC指标模块文档](../../../docs/core-runtime/profile-memory-metrics.md)：计量范围、画像归属和GC暂停限制。
 *
 */
static ZR_FORCE_INLINE void ZrCore_Profile_RecordPauseFromState(struct SZrState *state,
                                                                TZrUInt64 durationUs) {
    SZrProfileRuntime *runtime = ZrCore_Profile_FromState(state);
    if (ZR_UNLIKELY(runtime != ZR_NULL && runtime->recordMemory)) {
        TZrUInt32 sampleIndex = runtime->pauseSampleNext;

        runtime->pauseCount++;
        runtime->pauseTotalUs += durationUs;
        if (runtime->pauseMaxUs < durationUs) {
            runtime->pauseMaxUs = durationUs;
        }
        runtime->pauseSamples[sampleIndex] = durationUs;
        runtime->pauseSampleNext = (sampleIndex + 1u) % ZR_PROFILE_PAUSE_SAMPLE_CAPACITY;
        if (runtime->pauseSampleCount < ZR_PROFILE_PAUSE_SAMPLE_CAPACITY) {
            runtime->pauseSampleCount++;
        }
    }
}

#endif
