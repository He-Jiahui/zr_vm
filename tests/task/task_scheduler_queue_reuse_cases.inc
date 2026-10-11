#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_library/task_runtime.h"

/* Compile the private implementation unchanged to exercise rejection and OOM
 * boundaries without exposing test entry points in the product ABI. */
/**
 * ## Summary
 * 让测试翻译单元与生产 private helper 使用同一 queue 字段名，验证真实字段拒写及 replacement 发布。
 * ## Constraints and lifecycle
 * 静态只读 C 字符串；必须先于 private include 定义。它是当前实现细节，不是对外 Scheduler ABI。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:17；tests/task/task_scheduler_queue_reuse_cases.inc:35；zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc:30；zr_vm_library/src/zr_vm_library/task_runtime.c:44
 */
static const TZrChar *kTaskQueueField = "__zr_task_queue";
/**
 * ## Summary
 * 提供与生产耗尽/重建路径一致的 head 字段名，避免另建只供测试使用的队列模型。
 * ## Constraints and lifecycle
 * 静态借用字符串；head 的5→0和 memberVersion 变化由真实核心 setter 观察，不直接写 pair 值。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:26；tests/task/task_scheduler_queue_reuse_cases.inc:663；zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc:133；zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc:153
 */
static const TZrChar *kTaskQueueHeadField = "__zr_task_queue_head";
/**
 * ## Summary
 * 在本测试 TU 直接编译原生产队列 helper，开放内部拒写、pin 清理和 OOM 观察入口而不增加产品导出。
 * ## Constraints and lifecycle
 * 依赖父 TU 已包含的 core/native API 及前置 key 定义；此处不是另一套 queue provider，生产片段保持原样。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:17；tests/task/task_scheduler_queue_reuse_cases.inc:26；tests/task/task_scheduler_queue_reuse_cases.inc:35；zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc:11；zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc:132；tests/task/test_task_job_scheduler.c:459
 */
#include "../../zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc"

/** 跨多次源执行保存旧 Task 的独立 GC 根，供 completed 结果和重入 inner 结果再读。
 * 仅在 queue_reuse_task_saved 为真时 Resolve/Release；不得把借用对象地址当根，也不得覆盖尚未释放的句柄。
 */
static SZrGcRootHandle queue_reuse_saved_task;
/** 记录当前测试是否承担 saved_task 根的释放责任；不是 Task 完成状态。
 * 每个使用场景开始重置，根成功后置真，销毁状态前按此标志释放；本片段只支持父 main 串行执行。
 */
static TZrBool queue_reuse_task_saved;
/** 为随后源执行的宿主 enqueue 保留未提交的 inner Job。
 * capture_job 先建 Job 根再尝试 Scheduler 根；后者失败仍须按 job_saved 释放前者。
 */
static SZrGcRootHandle queue_reuse_saved_job;
/** 保留 Job 将提交到的同域 Scheduler，建立外层回调与内层提交之间的连接。
 * Resolve 返回借用对象，根在状态销毁前单独释放；仅 queue_reuse_scheduler_saved 为真时承担根责任。
 */
static SZrGcRootHandle queue_reuse_saved_scheduler;
/** 记录 capture_job 部分成功时已创建的 Job 根，支持失败后的独立清理。
 * 不能用 capture_job 的最终 bool 替代此标志；测试开始清零，cleanup 按实际创建情况释放。
 */
static TZrBool queue_reuse_job_saved;
/** 记录第二个 Scheduler 根是否成功创建，与 Job 根责任分开。
 * 两根创建不是原子事务；最终回调成功等于此标志，Job 根可能在失败时仍存在。
 */
static TZrBool queue_reuse_scheduler_saved;
/** 保存内层 schedule 返回时所观察到的 QUEUED 状态，供外层回调结束后的断言使用。
 * 仅建 Task 根成功后读 status；它不同于 enqueue_inner 的返回 bool，也不表示 queue 数组身份。
 */
static TZrBool queue_reuse_inner_was_queued;

/**
 * ## Summary
 * 由 task.queue.test.captureJob 在源执行边界保留 inner Job 及同域 Scheduler，供后续 outer Job 回调再提交。
 * ## Parameters
 * @param[in] context 当前同步原生上下文；参数0为 Job、参数1为 Scheduler，均从该状态借用。
 * @param[out] result 有效输出槽；仅参数读取失败时不写，进入根创建后写整数1，即使任一根创建失败也可能返回 false。
 * ## Returns
 * @return 两个根均创建成功时 true；false 不代表零资源取得，调用方按两个 saved 标志清理。
 * ## Constraints and lifecycle
 * 要求当前测试尚无这两根；Job 根先建立，Scheduler 根失败可能留下 job_saved=true，外层必须独立释放。result 写1不替代返回 bool。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:83；tests/task/task_scheduler_queue_reuse_cases.inc:87；tests/task/task_scheduler_queue_reuse_cases.inc:89；tests/task/task_scheduler_queue_reuse_cases.inc:91；tests/task/task_scheduler_queue_reuse_cases.inc:523；tests/task/task_scheduler_queue_reuse_cases.inc:524；zr_vm_core/include/zr_vm_core/gc_domain.h:180
 */
static TZrBool queue_reuse_capture_job(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *job = ZR_NULL;
    SZrObject *scheduler = ZR_NULL;
    if (!ZrLib_CallContext_ReadObject(context, 0u, &job) ||
        !ZrLib_CallContext_ReadObject(context, 1u, &scheduler)) return ZR_FALSE;
    /** 分别记录两根创建，使Scheduler根失败时仍能释放先前Job根。
     * 最终bool只代表第二根；两个saved标志分别决定cleanup责任。
     */
    queue_reuse_job_saved = ZrCore_GcRootHandle_Create(context->state, ZR_CAST_RAW_OBJECT_AS_SUPER(job), &queue_reuse_saved_job);
    queue_reuse_scheduler_saved = queue_reuse_job_saved &&
            ZrCore_GcRootHandle_Create(context->state, ZR_CAST_RAW_OBJECT_AS_SUPER(scheduler), &queue_reuse_saved_scheduler);
    ZrLib_Value_SetInt(context->state, result, 1);
    return queue_reuse_scheduler_saved;
}

/**
 * ## Summary
 * 由注入源在 result 已读后固定旧 Task，使后续 schedule 或 cooperative 用例能观察它仍然 completed。
 * ## Parameters
 * @param[in] context 同步上下文，参数0为借用 Task 对象。
 * @param[out] result 成功时写整数1，失败时不写。
 * ## Returns
 * @return 新根成功并记录释放责任时 true；已有根、参数读取或根创建失败时 false。
 * ## Constraints and lifecycle
 * 单个活动 Task 根槽不可重入覆盖；创建成功后由测试在状态销毁前 Release，不由 VM 返回值自动接管。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:109；tests/task/task_scheduler_queue_reuse_cases.inc:110；tests/task/task_scheduler_queue_reuse_cases.inc:113；tests/task/task_scheduler_queue_reuse_cases.inc:411；tests/task/task_scheduler_queue_reuse_cases.inc:623；zr_vm_core/include/zr_vm_core/gc_domain.h:180
 */
static TZrBool queue_reuse_capture_task(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *task = ZR_NULL;
    if (queue_reuse_task_saved || !ZrLib_CallContext_ReadObject(context, 0u, &task) ||
        !ZrCore_GcRootHandle_Create(context->state, ZR_CAST_RAW_OBJECT_AS_SUPER(task), &queue_reuse_saved_task)) {
        return ZR_FALSE;
    }
    queue_reuse_task_saved = ZR_TRUE;
    ZrLib_Value_SetInt(context->state, result, 1);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 将已保存 Task 根解析为 VM 值，供后续独立源读取同一 Task 的 result。
 * ## Parameters
 * @param[in] context 属于根所在状态的当前原生调用上下文。
 * @param[out] result 成功时发布 borrowed Task 对象值；Resolve 失败不写。
 * ## Returns
 * @return 存在活动 Task 根且成功解析时 true；否则 false，不消耗原根。
 * ## Constraints and lifecycle
 * 同域且保存根仍有效；输出对象值不取代根，也不延长跨状态销毁的生命周期。Resolve 的裸指针仅立即使用。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:133；tests/task/task_scheduler_queue_reuse_cases.inc:134；tests/task/task_scheduler_queue_reuse_cases.inc:135；tests/task/task_scheduler_queue_reuse_cases.inc:411；tests/task/task_scheduler_queue_reuse_cases.inc:522；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1324；zr_vm_core/include/zr_vm_core/gc_domain.h:204
 */
static TZrBool queue_reuse_get_task(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrRawObject *task = ZR_NULL;
    if (!queue_reuse_task_saved ||
        !ZrCore_GcRootHandle_Resolve(context->state, &queue_reuse_saved_task, &task)) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, ZR_CAST_OBJECT(context->state, task), ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

/**
 * ## Summary
 * 为定义在描述符后面的私有状态观察器提供前置声明，让 enqueue_inner 在当前回调点读取 Task 状态。
 * ## Parameters
 * @param[in] state 根所在的当前测试状态，调用期间借用。
 * ## Returns
 * @return 已存 Task 私有整数状态；根无效、字段缺失或类型不符返回-1。
 * ## Constraints and lifecycle
 * 调用者必须沿用 saved_task 的所属状态；这是测试内部观察接口，不是库公开 Task.status API。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:151；tests/task/task_scheduler_queue_reuse_cases.inc:178；tests/task/task_scheduler_queue_reuse_cases.inc:285；tests/task/task_scheduler_queue_reuse_cases.inc:289；tests/task/task_scheduler_queue_reuse_cases.inc:299
 */
static TZrInt64 queue_reuse_saved_status(SZrState *state);

/**
 * ## Summary
 * 在 outer Job 的原生回调中将预存 inner Job 提交给同一 Scheduler，观察嵌套 pump 被延后的排队状态。
 * ## Parameters
 * @param[in] context 同域同步原生上下文，不接受 ZR 参数；实际输入来自先前保存的两个根。
 * @param[out] result 提交并尝试保存 Task 后写13；根创建失败时也可能已经写入。
 * ## Returns
 * @return 提交成功且 Task 根成功时 true；QUEUED 是 inner_was_queued 的独立观察，不等同于本 bool。
 * ## Constraints and lifecycle
 * Job/Scheduler 根由上游 captureJob 建立；本测试保证 Task 槽为空。ScheduleJob 可能已消费 Job，后续 Task 根失败不撤销提交。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:170；tests/task/task_scheduler_queue_reuse_cases.inc:172；tests/task/task_scheduler_queue_reuse_cases.inc:177；tests/task/task_scheduler_queue_reuse_cases.inc:178；tests/task/task_scheduler_queue_reuse_cases.inc:180；zr_vm_library/src/zr_vm_library/task_runtime.c:731；zr_vm_library/src/zr_vm_library/task_runtime.c:1066
 */
static TZrBool queue_reuse_enqueue_inner(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrRawObject *job = ZR_NULL;
    SZrRawObject *scheduler = ZR_NULL;
    SZrTypeValue task;
    if (!ZrCore_GcRootHandle_Resolve(context->state, &queue_reuse_saved_job, &job) ||
        !ZrCore_GcRootHandle_Resolve(context->state, &queue_reuse_saved_scheduler, &scheduler) ||
        !ZrLibrary_TaskRuntime_ScheduleJob(context->state, ZR_CAST_OBJECT(context->state, scheduler),
                                         ZR_CAST_OBJECT(context->state, job), &task)) return ZR_FALSE;
    /** 先保存Task根再读提交时状态，将QUEUED快照与outer的整数13分开。
     * ScheduleJob成功不等于任务已执行；Task根失败时result仍可能写13。
     */
    queue_reuse_task_saved = ZrCore_GcRootHandle_Create(context->state, task.value.object, &queue_reuse_saved_task);
    queue_reuse_inner_was_queued = queue_reuse_task_saved && queue_reuse_saved_status(context->state) == ZR_VM_TASK_STATUS_QUEUED;
    ZrLib_Value_SetInt(context->state, result, 13);
    return queue_reuse_task_saved;
}

/**
 * ## Summary
 * 给 captureTask 的注册描述符提供单个 object 参数契约，使源侧可以传递真实 Task 对象。
 * ## Constraints and lifecycle
 * 静态借用表；object 注解不等于回调已检查 Task 类型或拥有对象。根保存由 capture_task 完成。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:192；tests/task/task_scheduler_queue_reuse_cases.inc:225；tests/task/task_scheduler_queue_reuse_cases.inc:109；tests/task/task_scheduler_queue_reuse_cases.inc:110
 */
static const ZrLibParameterDescriptor queue_reuse_capture_parameters[] = {
        {.name = "task", .typeName = "object"},
};
/**
 * ## Summary
 * 表达 captureJob 所需的 Job<int>/Scheduler 两个源侧参数，并连接到两个独立根。
 * ## Constraints and lifecycle
 * 静态生命周期；实际读取仍由回调逐项检查，不把注解当成根创建成功。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:203；tests/task/task_scheduler_queue_reuse_cases.inc:204；tests/task/task_scheduler_queue_reuse_cases.inc:246；tests/task/task_scheduler_queue_reuse_cases.inc:83；tests/task/task_scheduler_queue_reuse_cases.inc:89
 */
static const ZrLibParameterDescriptor queue_reuse_job_parameters[] = {
        {.name = "job", .typeName = "zr.task.Job<int>"},
        {.name = "scheduler", .typeName = "zr.task.Scheduler"},
};
/**
 * ## Summary
 * 汇集测试宿主函数 ABI，让三个场景通过真实 native registry 从注入源进入 C 回调。
 * ## Constraints and lifecycle
 * descriptor 和参数数组均静态存活，四项由模块 functionCount 消费；每个回调仍分别负责结果和根清理契约。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:214；tests/task/task_scheduler_queue_reuse_cases.inc:269；tests/task/task_scheduler_queue_reuse_cases.inc:395；tests/task/task_scheduler_queue_reuse_cases.inc:507；tests/task/task_scheduler_queue_reuse_cases.inc:592
 */
static const ZrLibFunctionDescriptor queue_reuse_host_functions[] = {
        /**
         * ## Summary
         * 把一个源侧 Task 交给 capture_task 建根，返回 int 哨兵。
         * ## Constraints and lifecycle
         * 一个参数，静态参数表；true 回调返回与哨兵1分别消费，不能重复捕获活动根。
         * ## References
         * tests/task/task_scheduler_queue_reuse_cases.inc:224；tests/task/task_scheduler_queue_reuse_cases.inc:225；tests/task/task_scheduler_queue_reuse_cases.inc:109
         */
        {.name = "captureTask", .minArgumentCount = 1u, .maxArgumentCount = 1u,
         .callback = queue_reuse_capture_task, .returnTypeName = "int",
         .parameters = queue_reuse_capture_parameters, .parameterCount = 1u},
        /**
         * ## Summary
         * 无源侧参数地取回已有 Task 根，以 Task<int> 值继续调用 result。
         * ## Constraints and lifecycle
         * 无参数；返回值借用根对象，不消费或释放根。
         * ## References
         * tests/task/task_scheduler_queue_reuse_cases.inc:235；tests/task/task_scheduler_queue_reuse_cases.inc:134；tests/task/task_scheduler_queue_reuse_cases.inc:135
         */
        {.name = "savedTask", .minArgumentCount = 0u, .maxArgumentCount = 0u,
         .callback = queue_reuse_get_task, .returnTypeName = "zr.task.Task<int>"},
        /**
         * ## Summary
         * 将 Job<int> 和 Scheduler 交给 capture_job，为跨源提交建立两个根。
         * ## Constraints and lifecycle
         * 两个参数；partial root 由两个独立标志清理，失败不能当作未取得 Job 根。
         * ## References
         * tests/task/task_scheduler_queue_reuse_cases.inc:244；tests/task/task_scheduler_queue_reuse_cases.inc:246；tests/task/task_scheduler_queue_reuse_cases.inc:87；tests/task/task_scheduler_queue_reuse_cases.inc:89
         */
        {.name = "captureJob", .minArgumentCount = 2u, .maxArgumentCount = 2u,
         .callback = queue_reuse_capture_job, .returnTypeName = "int",
         .parameters = queue_reuse_job_parameters, .parameterCount = 2u},
        /**
         * ## Summary
         * 从 outer Job 内进入 enqueue_inner，产生 inner Task 并保存 QUEUED 快照。
         * ## Constraints and lifecycle
         * 无参数；返回int13用于 outer结果，原生bool只表示调用与Task根成功。
         * ## References
         * tests/task/task_scheduler_queue_reuse_cases.inc:256；tests/task/task_scheduler_queue_reuse_cases.inc:172；tests/task/task_scheduler_queue_reuse_cases.inc:178
         */
        {.name = "enqueueInner", .minArgumentCount = 0u, .maxArgumentCount = 0u,
         .callback = queue_reuse_enqueue_inner, .returnTypeName = "int"},
};
/**
 * ## Summary
 * 按真实插件 ABI 发布 task.queue.test，供三类源用例导入测试宿主函数。
 * ## Constraints and lifecycle
 * 模块及全部描述符静态存活；注册不拥有 saved 根，测试仍须在 State_Destroy 前释放实际创建的根。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:267；tests/task/task_scheduler_queue_reuse_cases.inc:268；tests/task/task_scheduler_queue_reuse_cases.inc:269；tests/task/task_scheduler_queue_reuse_cases.inc:395；tests/task/task_scheduler_queue_reuse_cases.inc:507；tests/task/task_scheduler_queue_reuse_cases.inc:592
 */
static const ZrLibModuleDescriptor queue_reuse_host_module = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "task.queue.test",
        .functions = queue_reuse_host_functions, .functionCount = 4u,
        .moduleVersion = "1.0.0", .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
};

/**
 * ## Summary
 * 扫描已保存 Task 的当前私有整数状态，为 completed 不重放与重入 QUEUED 快照提供不建 key 的观察器。
 * ## Parameters
 * @param[in] state 保存 Task 根所在状态，调用期间借用。
 * ## Returns
 * @return __zr_task_status 的整数值；根无效、字段缺失或非整数时-1，不能将-1解释为合法Task状态。
 * ## Constraints and lifecycle
 * 只在同一串行测试状态立即借用 nodeMap；不分配字符串 key，也不暴露稳定公开状态接口。私有字段名变化需同步更新此测试。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:289；tests/task/task_scheduler_queue_reuse_cases.inc:291；tests/task/task_scheduler_queue_reuse_cases.inc:298；tests/task/task_scheduler_queue_reuse_cases.inc:299；tests/task/task_scheduler_queue_reuse_cases.inc:302；zr_vm_core/include/zr_vm_core/gc_domain.h:204
 */
static TZrInt64 queue_reuse_saved_status(SZrState *state) {
    SZrRawObject *raw = ZR_NULL;
    SZrObject *task;
    TZrSize bucket;
    if (!queue_reuse_task_saved || !ZrCore_GcRootHandle_Resolve(state, &queue_reuse_saved_task, &raw)) return -1;
    task = ZR_CAST_OBJECT(state, raw);
    for (bucket = 0; bucket < task->nodeMap.capacity; ++bucket) {
        SZrHashKeyValuePair *pair;
        for (pair = task->nodeMap.buckets[bucket]; pair != ZR_NULL; pair = pair->next) {
            const SZrString *key;
            if (pair->key.type != ZR_VALUE_TYPE_STRING || pair->key.value.object == ZR_NULL) continue;
            key = (const SZrString *)pair->key.value.object;
            if (ZrCore_String_GetByteLength(key) == strlen("__zr_task_status") &&
                memcmp(ZrCore_String_GetNativeString(key), "__zr_task_status", strlen("__zr_task_status")) == 0 &&
                ZR_VALUE_IS_TYPE_INT(pair->value.type)) return pair->value.value.nativeObject.nativeInt64;
        }
    }
    return -1;
}

/**
 * ## Summary
 * 通过三次独立源执行确认耗尽队列后的新 Job 不重放旧 completed Task，其原结果仍为7，新结果为9。
 * ## Returns
 * @return 无；局部数值与状态观察交给清理之后的 Unity 断言，失败计入父 main 的汇总退出码。
 * ## Constraints and lifecycle
 * 旧 Task 由宿主根跨编译/执行保存，最后先 Release 和 State_Destroy 再 Unity 断言；不直接证明两次 queue 的数组身份。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:396；tests/task/task_scheduler_queue_reuse_cases.inc:400；tests/task/task_scheduler_queue_reuse_cases.inc:404；tests/task/task_scheduler_queue_reuse_cases.inc:411；tests/task/task_scheduler_queue_reuse_cases.inc:414；tests/task/task_scheduler_queue_reuse_cases.inc:419；tests/task/task_scheduler_queue_reuse_cases.inc:422
 */
static void test_scheduler_reuse_preserves_completed_job_result(void) {
    /**
     * ## Summary
     * 先同步完成返回7的Job，再通过captureTask保留其Task，让之后队列重建仍能观察旧结果。
     * ## Constraints and lifecycle
     * 源文本只在compile期间借用；Job函数由源码创建，saved根由宿主回调承担。
     * ## References
     * tests/task/task_scheduler_queue_reuse_cases.inc:337；tests/task/task_scheduler_queue_reuse_cases.inc:339；tests/task/task_scheduler_queue_reuse_cases.inc:340；tests/task/task_scheduler_queue_reuse_cases.inc:396
     */
    static const char *firstSource =
            "var task = import(\"zr.task\");\n"
            "var host = import(\"task.queue.test\");\n"
    /**
     * ## Summary
     * 由真实Job调度执行的注入函数，以结果7区分该Job与其他队列项。
     * ## Returns
     * @return 成功执行时返回整数7；此固定值是区分队列项的观察值。
     * ## Constraints and lifecycle
     * 注释放在C字符串外，保持源字节；函数由VM/当前状态管理，不是宿主native callback。
     * ## References
     * tests/task/task_scheduler_queue_reuse_cases.inc:337；tests/task/task_scheduler_queue_reuse_cases.inc:338；tests/task/task_scheduler_queue_reuse_cases.inc:339
     */
            "var firstJob = init task.Job<int>(fn() => { return 7; });\n"
            "var first = task.currentScheduler.schedule<int>(firstJob);\n"
            "var value = first.result();\n"
            "var captured = host.captureTask(first);\n"
            "return value;\n";
    /**
     * ## Summary
     * 用新的Job返回9，触发上一队列耗尽后的调度，和旧Task结果进行区分。
     * ## Constraints and lifecycle
     * 不同源编译创建不同Job；本模板不保存新Task根，仅本次执行返回值被读取。
     * ## References
     * tests/task/task_scheduler_queue_reuse_cases.inc:362；tests/task/task_scheduler_queue_reuse_cases.inc:363；tests/task/task_scheduler_queue_reuse_cases.inc:364；tests/task/task_scheduler_queue_reuse_cases.inc:400
     */
    static const char *secondSource =
            "var task = import(\"zr.task\");\n"
    /**
     * ## Summary
     * 由真实Job调度执行的注入函数，以结果9区分该Job与其他队列项。
     * ## Returns
     * @return 成功执行时返回整数9；此固定值是区分队列项的观察值。
     * ## Constraints and lifecycle
     * 注释放在C字符串外，保持源字节；函数由VM/当前状态管理，不是宿主native callback。
     * ## References
     * tests/task/task_scheduler_queue_reuse_cases.inc:362；tests/task/task_scheduler_queue_reuse_cases.inc:363；tests/task/task_scheduler_queue_reuse_cases.inc:364
     */
            "var secondJob = init task.Job<int>(fn() => { return 9; });\n"
            "var second = task.currentScheduler.schedule<int>(secondJob);\n"
            "return second.result();\n";
    /**
     * ## Summary
     * 在第二次调度后通过savedTask取回首个Task，再读取结果，识别重放或结果污染。
     * ## Constraints and lifecycle
     * 依赖firstSource已创建且未释放的根；旧Task不再重新提交。
     * ## References
     * tests/task/task_scheduler_queue_reuse_cases.inc:375；tests/task/task_scheduler_queue_reuse_cases.inc:377；tests/task/task_scheduler_queue_reuse_cases.inc:404
     */
    static const char *lastSource =
            "var task = import(\"zr.task\");\n"
            "var host = import(\"task.queue.test\");\n"
            "var first = host.savedTask();\n"
            "return first.result();\n";
    SZrState *state = create_task_job_scheduler_test_state();
    SZrFunction *function;
    TZrBool created = state != ZR_NULL;
    TZrBool firstReady = ZR_FALSE;
    TZrBool secondReady = ZR_FALSE;
    TZrBool afterRead = ZR_FALSE;
    TZrInt64 before = 0;
    TZrInt64 secondResult = 0;
    TZrInt64 after = 0;
    TZrInt64 beforeStatus = -1;
    TZrInt64 afterStatus = -1;
    TZrBool registered = ZR_FALSE;

    memset(&queue_reuse_saved_task, 0, sizeof(queue_reuse_saved_task));
    queue_reuse_task_saved = ZR_FALSE;

    if (created) {
        registered = ZrLibrary_NativeRegistry_RegisterModule(state->global, &queue_reuse_host_module);
        function = compile_task_job_scheduler_source(state, firstSource, "task_scheduler_first_job.zr");
        firstReady = registered && function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &before);
        if (firstReady) {
            beforeStatus = queue_reuse_saved_status(state);
            function = compile_task_job_scheduler_source(state, secondSource, "task_scheduler_second_job.zr");
            secondReady = function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &secondResult);
            afterStatus = queue_reuse_saved_status(state);
            if (secondReady && afterStatus == ZR_VM_TASK_STATUS_COMPLETED) {
                function = compile_task_job_scheduler_source(state, lastSource, "task_scheduler_saved_job.zr");
                afterRead = function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &after);
            }
        }
    /** 先释放旧Task根并销毁状态，再断言跨三次源执行的观察值。
     * 减少Unity失败长跳转跳过资源清理；先前局部数值可在状态销毁后读取。
     */
        if (queue_reuse_task_saved) ZrCore_GcRootHandle_Release(state, &queue_reuse_saved_task);
        queue_reuse_task_saved = ZR_FALSE;
    }
    ZrTests_State_Destroy(state);
    TEST_ASSERT_TRUE(created);
    TEST_ASSERT_TRUE(registered && firstReady);
    TEST_ASSERT_EQUAL_INT64(7, before);
    TEST_ASSERT_EQUAL_INT64(ZR_VM_TASK_STATUS_COMPLETED, beforeStatus);
    TEST_ASSERT_EQUAL_INT64_MESSAGE(ZR_VM_TASK_STATUS_COMPLETED, afterStatus, "new scheduling replayed the old completed Task");
    TEST_ASSERT_TRUE(secondReady && afterRead);
    TEST_ASSERT_EQUAL_INT64(9, secondResult);
    TEST_ASSERT_EQUAL_INT64(7, after);
}

/**
 * ## Summary
 * 在外层 Job 内通过宿主提交 inner Job，确认嵌套 submit 返回时仍QUEUED，随后外层退出得到13、inner最终得到19。
 * ## Returns
 * @return 无；局部数值与状态观察交给清理之后的 Unity 断言，失败计入父 main 的汇总退出码。
 * ## Constraints and lifecycle
 * 同Scheduler非递归pump的时序由 queued快照观察，三根独立清理在断言前完成；不把最终inner结果当作提交时已完成。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:508；tests/task/task_scheduler_queue_reuse_cases.inc:511；tests/task/task_scheduler_queue_reuse_cases.inc:513；tests/task/task_scheduler_queue_reuse_cases.inc:515；tests/task/task_scheduler_queue_reuse_cases.inc:522；tests/task/task_scheduler_queue_reuse_cases.inc:524；tests/task/task_scheduler_queue_reuse_cases.inc:527；tests/task/task_scheduler_queue_reuse_cases.inc:529；zr_vm_library/src/zr_vm_library/task_runtime.c:731
 */
static void test_scheduler_keeps_reentrant_enqueue_until_outer_job_returns(void) {
    /**
     * ## Summary
     * 创建返回19的inner Job但暂不schedule，通过captureJob保存Job和当前Scheduler供outer回调使用。
     * ## Constraints and lifecycle
     * captureJob两根可能部分成功；外层cleanup不能只依据整个源是否成功。
     * ## References
     * tests/task/task_scheduler_queue_reuse_cases.inc:457；tests/task/task_scheduler_queue_reuse_cases.inc:458；tests/task/task_scheduler_queue_reuse_cases.inc:508
     */
    static const char *innerSource =
            "var task = import(\"zr.task\");\n"
            "var host = import(\"task.queue.test\");\n"
    /**
     * ## Summary
     * 由真实Job调度执行的注入函数，以结果19区分该Job与其他队列项。
     * ## Returns
     * @return 成功执行时返回整数19；此固定值是区分队列项的观察值。
     * ## Constraints and lifecycle
     * 注释放在C字符串外，保持源字节；函数由VM/当前状态管理，不是宿主native callback。先captureJob保存，后由enqueue_inner提交；最终由resultSource观察。
     * ## References
     * tests/task/task_scheduler_queue_reuse_cases.inc:457；tests/task/task_scheduler_queue_reuse_cases.inc:172；tests/task/task_scheduler_queue_reuse_cases.inc:515
     */
            "var job = init task.Job<int>(fn() => { return 19; });\n"
            "return host.captureJob(job, task.currentScheduler);\n";
    /**
     * ## Summary
     * 执行outer Job时进入enqueueInner，在外层pump仍活动的时刻完成内层提交。
     * ## Constraints and lifecycle
     * outer返回13与inner最终19不同；QUEUED快照在C回调内取得，不由这个源的返回值代替。
     * ## References
     * tests/task/task_scheduler_queue_reuse_cases.inc:480；tests/task/task_scheduler_queue_reuse_cases.inc:481；tests/task/task_scheduler_queue_reuse_cases.inc:483；tests/task/task_scheduler_queue_reuse_cases.inc:484；tests/task/task_scheduler_queue_reuse_cases.inc:511
     */
    static const char *outerSource =
            "var task = import(\"zr.task\");\n"
            "var host = import(\"task.queue.test\");\n"
    /**
     * ## Summary
     * 在outer Job执行期间调用enqueueInner并返回13，使内层Job提交发生在活动pump内部。
     * ## Returns
     * @return 返回宿主enqueueInner的13，内层Job另有最终19。
     * ## Constraints and lifecycle
     * 注释放在C字符串外，保持源字节；函数由VM/当前状态管理，不是宿主native callback。
     * ## References
     * tests/task/task_scheduler_queue_reuse_cases.inc:480；tests/task/task_scheduler_queue_reuse_cases.inc:483；tests/task/task_scheduler_queue_reuse_cases.inc:484
     */
            "var outerJob = init task.Job<int>(fn() => {\n"
            "  return host.enqueueInner();\n"
            "});\n"
            "var outer = task.currentScheduler.schedule<int>(outerJob);\n"
            "return outer.result();\n";
    /**
     * ## Summary
     * 在outer源已返回后取回inner Task并等待result，确认之前排队的工作随后实际完成。
     * ## Constraints and lifecycle
     * 依赖inner Task根有效；仅观察最终19，不证明enqueue时已经完成。
     * ## References
     * tests/task/task_scheduler_queue_reuse_cases.inc:495；tests/task/task_scheduler_queue_reuse_cases.inc:496；tests/task/task_scheduler_queue_reuse_cases.inc:515
     */
    static const char *resultSource =
            "var host = import(\"task.queue.test\");\n"
            "var inner = host.savedTask();\n"
            "return inner.result();\n";
    SZrState *state = create_task_job_scheduler_test_state();
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;
    TZrBool executed = ZR_FALSE;
    TZrBool outerExecuted = ZR_FALSE;
    TZrInt64 outerResult = 0;
    TZrBool queued = ZR_FALSE;
    queue_reuse_task_saved = ZR_FALSE;
    queue_reuse_job_saved = queue_reuse_scheduler_saved = ZR_FALSE;
    queue_reuse_inner_was_queued = ZR_FALSE;
    if (state != ZR_NULL && ZrLibrary_NativeRegistry_RegisterModule(state->global, &queue_reuse_host_module)) {
        function = compile_task_job_scheduler_source(state, innerSource, "task_scheduler_reentrant_inner.zr");
        executed = function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &result);
        if (executed) {
            function = compile_task_job_scheduler_source(state, outerSource, "task_scheduler_reentrant_outer.zr");
            outerExecuted = function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &outerResult);
            queued = queue_reuse_inner_was_queued;
            if (outerExecuted) {
                function = compile_task_job_scheduler_source(state, resultSource, "task_scheduler_reentrant_result.zr");
                executed = function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &result);
            }
        }
    /** 按实际创建标志分别释放Task、Job和Scheduler根，包含captureJob部分失败。
     * 先销毁状态再Unity断言；不以整个源的成功bool替代各根责任。
     */
        if (queue_reuse_task_saved) ZrCore_GcRootHandle_Release(state, &queue_reuse_saved_task);
        if (queue_reuse_job_saved) ZrCore_GcRootHandle_Release(state, &queue_reuse_saved_job);
        if (queue_reuse_scheduler_saved) ZrCore_GcRootHandle_Release(state, &queue_reuse_saved_scheduler);
    }
    queue_reuse_task_saved = ZR_FALSE;
    ZrTests_State_Destroy(state);
    TEST_ASSERT_TRUE_MESSAGE(executed, "reentrant scheduler source failed");
    TEST_ASSERT_TRUE(outerExecuted && queued);
    TEST_ASSERT_EQUAL_INT64(13, outerResult);
    TEST_ASSERT_EQUAL_INT64(19, result);
}

/**
 * ## Summary
 * 在旧 Task 已完成后执行 yieldNow 与原生 delay(3)/result，确认 cooperative 推进不改变旧 Task 的结果状态。
 * ## Returns
 * @return 无；局部数值与状态观察交给清理之后的 Unity 断言，失败计入父 main 的汇总退出码。
 * ## Constraints and lifecycle
 * turns为本地重新入队次数；不是计时器或OS worker验证。BUG: 原生Task.result收到未初始化result槽，正常COMPLETED的typed-null字段仍经Value_Copy读取目标ownership元数据，可能误释资源或崩溃；仅静态证明，字符串和程序未修复。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:593；tests/task/task_scheduler_queue_reuse_cases.inc:602；tests/task/task_scheduler_queue_reuse_cases.inc:604；tests/task/task_scheduler_queue_reuse_cases.inc:612；tests/task/task_scheduler_queue_reuse_cases.inc:616；tests/task/task_scheduler_queue_reuse_cases.inc:619；tests/task/task_scheduler_queue_reuse_cases.inc:621；tests/task/task_scheduler_queue_reuse_cases.inc:623；tests/task/task_scheduler_queue_reuse_cases.inc:626；zr_vm_library/src/zr_vm_library/task_runtime.c:795；zr_vm_library/src/zr_vm_library/task_runtime.c:812；zr_vm_library/src/zr_vm_library/task_runtime.c:813；zr_vm_library/src/zr_vm_library/task_runtime.c:710；zr_vm_library/src/zr_vm_library/task_runtime.c:713；zr_vm_library/src/zr_vm_library/task_runtime.c:1162；zr_vm_library/src/zr_vm_library/task_runtime.c:835；zr_vm_library/src/zr_vm_library/task_runtime.c:836；zr_vm_library/src/zr_vm_library/task_runtime.c:837；zr_vm_library/src/zr_vm_library/task_runtime.c:216；zr_vm_library/src/zr_vm_library/task_runtime.c:217；zr_vm_core/include/zr_vm_core/value.h:151；zr_vm_core/include/zr_vm_core/value.h:161；zr_vm_core/include/zr_vm_core/value.h:229
 */
static void test_scheduler_reuses_queue_for_cooperative_turns(void) {
    /**
     * ## Summary
     * 为cooperative场景单独建立并保存一个结果7的completed Task，之后检查它未被yield/delay重放。
     * ## Constraints and lifecycle
     * 这是独立状态中的模板定义，不能与第一个测试同文本模板合并为同一个审查单元。
     * ## References
     * tests/task/task_scheduler_queue_reuse_cases.inc:566；tests/task/task_scheduler_queue_reuse_cases.inc:568；tests/task/task_scheduler_queue_reuse_cases.inc:569；tests/task/task_scheduler_queue_reuse_cases.inc:593
     */
    static const char *firstSource =
            "var task = import(\"zr.task\");\n"
            "var host = import(\"task.queue.test\");\n"
    /**
     * ## Summary
     * 由真实Job调度执行的注入函数，以结果7区分该Job与其他队列项。
     * ## Returns
     * @return 成功执行时返回整数7；此固定值是区分队列项的观察值。
     * ## Constraints and lifecycle
     * 注释放在C字符串外，保持源字节；函数由VM/当前状态管理，不是宿主native callback。
     * ## References
     * tests/task/task_scheduler_queue_reuse_cases.inc:566；tests/task/task_scheduler_queue_reuse_cases.inc:567；tests/task/task_scheduler_queue_reuse_cases.inc:568
     */
            "var job = init task.Job<int>(fn() => { return 7; });\n"
            "var first = task.currentScheduler.schedule<int>(job);\n"
            "var value = first.result();\n"
            "var captured = host.captureTask(first);\n"
            "return value;\n";
    /**
     * ## Summary
     * 通过源级yieldNow/result推进本地cooperative任务并返回23，之后和原生delay回调交叉观察。
     * ## Constraints and lifecycle
     * 这里没有注入fn；turns语义不等于墙钟等待，值23只表示源执行返回。
     * ## References
     * tests/task/task_scheduler_queue_reuse_cases.inc:581；tests/task/task_scheduler_queue_reuse_cases.inc:582；tests/task/task_scheduler_queue_reuse_cases.inc:583；tests/task/task_scheduler_queue_reuse_cases.inc:604
     */
    static const char *yieldSource =
            "var task = import(\"zr.task\");\n"
            "var yielded = task.yieldNow();\n"
            "yielded.result();\n"
            "return 23;\n";
    SZrState *state = create_task_job_scheduler_test_state();
    TZrBool ready = ZR_FALSE;
    TZrBool yielded = ZR_FALSE;
    TZrBool delayed = ZR_FALSE;
    TZrInt64 first = 0;
    TZrInt64 yieldResult = 0;
    TZrInt64 status = -1;
    queue_reuse_task_saved = ZR_FALSE;
    if (state != ZR_NULL && ZrLibrary_NativeRegistry_RegisterModule(state->global, &queue_reuse_host_module)) {
        SZrFunction *function = compile_task_job_scheduler_source(state, firstSource, "task_scheduler_before_cooperative.zr");
        ready = function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &first);
        if (ready) {
            const ZrLibModuleDescriptor *module;
            const ZrLibFunctionDescriptor *delay;
            const ZrLibMethodDescriptor *resultMethod;
            ZrLibCallContext context = {0};
            SZrTypeValue turns;
            SZrTypeValue task;
            SZrTypeValue result;
            function = compile_task_job_scheduler_source(state, yieldSource, "task_scheduler_repeated_yield.zr");
            yielded = function != ZR_NULL && ZrTests_Function_ExecuteExpectInt64(state, function, &yieldResult);
            module = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.task");
            delay = find_function_descriptor(module, "delay");
            resultMethod = find_method_descriptor(find_type_descriptor(module, "Task"), "result");
            ZrLib_Value_SetInt(state, &turns, 3);
    /** 以显式native上下文调用delay(3)再切换self调用Task.result，交叉检查源级yield与原生ABI。
     * BUG: result未初始化即交给Task.result；COMPLETED的null字段仍以Value_Copy覆盖，覆盖前读取随机ownership元数据，存在误释放/崩溃风险。callback返回后的type或true检查不能保护此读取；本轮只备注。
     */
            context.state = state;
            context.argumentValues = &turns;
            context.argumentCount = 1u;
            if (yielded && delay != ZR_NULL && resultMethod != ZR_NULL && delay->callback(&context, &task)) {
                context.selfValue = &task;
                context.argumentValues = ZR_NULL;
                context.argumentCount = 0u;
                delayed = resultMethod->callback(&context, &result) && result.type == ZR_VALUE_TYPE_NULL;
            }
            status = queue_reuse_saved_status(state);
        }
        if (queue_reuse_task_saved) ZrCore_GcRootHandle_Release(state, &queue_reuse_saved_task);
    }
    queue_reuse_task_saved = ZR_FALSE;
    ZrTests_State_Destroy(state);
    TEST_ASSERT_TRUE(ready && yielded && delayed);
    TEST_ASSERT_EQUAL_INT64(7, first);
    TEST_ASSERT_EQUAL_INT64(23, yieldResult);
    TEST_ASSERT_EQUAL_INT64(ZR_VM_TASK_STATUS_COMPLETED, status);
}

/* These objects use real hash pairs, member descriptors and core setters. */
/**
 * ## Summary
 * 以真实核心对象、哈希字段和可选只读descriptor构造private队列测试fixture，避免用假setter替代产品拒写路径。
 * ## Parameters
 * @param[in] state 非空测试状态，与输出根同域，调用期间借用。
 * @param[in] readonlyField 可空C字符串；非空时加入不可写字段descriptor，不转移文本所有权。
 * @param[in] withQueue 为true时先建立一个真实array，否则queue字段初始化null。
 * @param[out] root 未持根的输出槽；成功创建根后由调用方独立Release。
 * @param[out] rootCreated 非空输出，说明根是否取得，不等同于后续fixture全部构造成功。
 * ## Returns
 * @return 构造流程完成时返回借用Scheduler；后续分配失败可返回NULL但保留rootCreated=true。
 * ## Constraints and lifecycle
 * rootCreated在Scheduler根成功后即为true，随后创建array/prototype失败返回NULL仍由调用方Release。void字段setter没有成功确认，非NULL返回本身不证明queue/head已存在。
 * TODO: 核查两次void字段写入的失败状态并验证head/queue pair存在；后续测试目前将非NULL scheduler当作可解引用pair的充分条件。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:661；tests/task/task_scheduler_queue_reuse_cases.inc:663；tests/task/task_scheduler_queue_reuse_cases.inc:666；tests/task/task_scheduler_queue_reuse_cases.inc:671；tests/task/task_scheduler_queue_reuse_cases.inc:677；tests/task/task_scheduler_queue_reuse_cases.inc:681；tests/task/task_scheduler_queue_reuse_cases.inc:730；tests/task/task_scheduler_queue_reuse_cases.inc:838；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1385
 */
static SZrObject *queue_reuse_private_scheduler(SZrState *state, const char *readonlyField,
                                               TZrBool withQueue, SZrGcRootHandle *root,
                                               TZrBool *rootCreated) {
    SZrObject *scheduler = ZrCore_Object_New(state, ZR_NULL);
    SZrObjectPrototype *prototype;
    SZrTypeValue value;
    SZrMemberDescriptor descriptor = {0};
    /** 取得Scheduler根后才创建字段、array和prototype，后续失败不抹去根的清理责任。
     * rootCreated与构造返回值是两个维度，返回NULL也可能已持根。
     */
    *rootCreated = scheduler != ZR_NULL && ZrCore_GcRootHandle_Create(state, ZR_CAST_RAW_OBJECT_AS_SUPER(scheduler), root);
    if (!*rootCreated) return ZR_NULL;
    ZrLib_Value_SetInt(state, &value, 5);
    ZrLib_Object_SetFieldCString(state, scheduler, kTaskQueueHeadField, &value);
    ZrLib_Value_SetNull(&value);
    if (withQueue) {
        SZrObject *queue = ZrLib_Array_New(state);
        if (queue == ZR_NULL) return ZR_NULL;
        ZrLib_Value_SetObject(state, &value, queue, ZR_VALUE_TYPE_ARRAY);
    }
    ZrLib_Object_SetFieldCString(state, scheduler, kTaskQueueField, &value);
    if (readonlyField != ZR_NULL) {
        SZrString *name = ZrCore_String_Create(state, "queue-test", strlen("queue-test"));
        prototype = ZrCore_ObjectPrototype_New(state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        descriptor.name = ZrCore_String_Create(state, (TZrNativeString)readonlyField, strlen(readonlyField));
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
        if (prototype == ZR_NULL || descriptor.name == ZR_NULL ||
            !ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor)) return ZR_NULL;
        scheduler->prototype = prototype;
    }
    return scheduler;
}

/**
 * ## Summary
 * 以四个新状态构造真实字段/descriptor，分别观察head拒写、已有queue拒绝detach、空queue拒绝attach和正常replacement版本。
 * ## Returns
 * @return 无；局部数值与状态观察交给清理之后的 Unity 断言，失败计入父 main 的汇总退出码。
 * ## Constraints and lifecycle
 * 每模式根与状态在断言前清理；只读字段来自真实prototype descriptor。字段存在与借用pair跨分配有效性是当前待核查前提。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:701；tests/task/task_scheduler_queue_reuse_cases.inc:709；tests/task/task_scheduler_queue_reuse_cases.inc:716；tests/task/task_scheduler_queue_reuse_cases.inc:719；tests/task/task_scheduler_queue_reuse_cases.inc:721；tests/task/task_scheduler_queue_reuse_cases.inc:727；tests/task/task_scheduler_queue_reuse_cases.inc:730；tests/task/task_scheduler_queue_reuse_cases.inc:731；tests/task/task_scheduler_queue_reuse_cases.inc:733；tests/task/task_scheduler_queue_reuse_cases.inc:736
 */
static void test_scheduler_checked_fields_reject_writes_without_replaying(void) {
    TZrUInt32 mode;
    TZrBool outcomes[4] = {ZR_FALSE, ZR_FALSE, ZR_FALSE, ZR_FALSE};
    for (mode = 0; mode < 4u; ++mode) {
        SZrState *state = create_task_job_scheduler_test_state();
        SZrGcRootHandle root;
        TZrBool rooted = ZR_FALSE;
        SZrObject *scheduler = state == ZR_NULL ? ZR_NULL : queue_reuse_private_scheduler(
                state, mode == 0u ? kTaskQueueHeadField : mode < 3u ? kTaskQueueField : ZR_NULL,
                mode == 1u, &root, &rooted);
        if (scheduler != ZR_NULL) {
    /** 保存字段/版本和pin基线，比较拒写或replacement之后的真实状态。
     * pair/head是private helper借用地址，queue创建可能分配，root并不自动保证旧字段地址继续有效。
     * TODO: 按生产helper的借用约束核查跨分配后的pair/head有效性，或在queue/detach之后重查；当前测试未重查，尚未证明必然发生搬迁或UAF。
     */
            SZrHashKeyValuePair *head = task_runtime_scheduler_pair(scheduler, kTaskQueueHeadField);
            SZrHashKeyValuePair *pair = task_runtime_scheduler_pair(scheduler, kTaskQueueField);
            TZrUInt32 version = scheduler->memberVersion;
            TZrUInt32 pins = scheduler->super.garbageCollectMark.pinFlags;
            TZrBool ignored = ZrCore_GarbageCollector_IsObjectIgnored(state->global, &scheduler->super);
            TZrSize ignoredCount = state->global->garbageCollector->ignoredObjectCount;
            if (mode == 1u) {
                outcomes[mode] = !task_runtime_scheduler_detach_queue(state, scheduler, ZR_CAST_OBJECT(state, pair->value.value.object)) &&
                        pair->value.type == ZR_VALUE_TYPE_ARRAY && head->value.value.nativeObject.nativeInt64 == 5 &&
                        scheduler->memberVersion == version;
            } else {
                SZrObject *queue = task_runtime_scheduler_queue(state, scheduler);
                outcomes[mode] = mode == 3u ? queue != ZR_NULL && pair->value.value.object == &queue->super :
                        queue == ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NULL;
                outcomes[mode] = outcomes[mode] && head->value.value.nativeObject.nativeInt64 == (mode == 0u ? 5 : 0) &&
                        scheduler->super.garbageCollectMark.pinFlags == pins &&
                        ZrCore_GarbageCollector_IsObjectIgnored(state->global, &scheduler->super) == ignored &&
                        state->global->garbageCollector->ignoredObjectCount == ignoredCount &&
                        scheduler->memberVersion == version + (mode == 0u ? 0u : mode == 2u ? 1u : 2u);
            }
        }
        if (rooted) ZrCore_GcRootHandle_Release(state, &root);
        ZrTests_State_Destroy(state);
    }
    TEST_ASSERT_TRUE_MESSAGE(outcomes[0], "rejected head reset changed fields");
    TEST_ASSERT_TRUE_MESSAGE(outcomes[1], "rejected detach lost queue or reset head");
    TEST_ASSERT_TRUE_MESSAGE(outcomes[2], "rejected attach published queue or leaked scheduler pin");
    TEST_ASSERT_TRUE_MESSAGE(outcomes[3], "successful attach did not update versions or release pin");
}

/**
 * ## Summary
 * 将有限对象分配拒绝策略与原allocator及其参数放在同一同步交接记录中，以便恢复实际上游路径后重试。
 * ## Constraints and lifecycle
 * 栈上记录只在当前Global的临时allocator替换期间有效，不允许并发或异步保存；恢复函数与参数必须成对，先于cleanup和State_Destroy。
 * ## Fields
 * upstream/argument为原分配路径；failures为匹配拒绝计数；reject为临时启用标志。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:751；tests/task/task_scheduler_queue_reuse_cases.inc:757；tests/task/task_scheduler_queue_reuse_cases.inc:779；tests/task/task_scheduler_queue_reuse_cases.inc:814；tests/task/task_scheduler_queue_reuse_cases.inc:817；tests/task/task_scheduler_queue_reuse_cases.inc:822
 */
typedef struct SZrQueueReuseAllocationFailure {
    /** 保存真实上游分配函数，非拒绝请求继续转发并在退出临时替换时恢复。 */
    FZrAllocator upstream;
    /** 保存与upstream配套的原参数；不同于包装器自身的failure指针。 */
    TZrPtr argument;
    /** 计数符合谓词而被拒绝的请求，最多两次，不计普通转发请求。 */
    TZrUInt32 failures;
    /** 仅控制同步建队窗口中的拒绝；恢复allocator前关掉，不代表内存错误已清理。 */
    TZrBool reject;
} SZrQueueReuseAllocationFailure;

/**
 * ## Summary
 * 作为临时FZrAllocator拒绝前两次符合对象谓词的请求，其他请求交回真实上游，允许错误规范化继续分配。
 * ## Parameters
 * @param[in,out] argument 当前栈上failure记录，必须含有效upstream及其配套argument。
 * @param[in] pointer 转发的旧分配地址；仅为NULL时匹配拒绝，不接管其释放责任。
 * @param[in] originalSize 原分配大小，拒绝谓词不读取，原样交上游。
 * @param[in] newSize 请求大小，仅sizeof(SZrObject)参与拒绝。
 * @param[in] flag 原生分配类别，仅OBJECT参与拒绝。
 * ## Returns
 * @return 匹配且尚不足两次时NULL；其他情况原样返回真实上游结果，包括它自己的失败。
 * ## Constraints and lifecycle
 * 只检查reject、次数、pointer=NULL、newSize=sizeof(SZrObject)及OBJECT标志，不检查originalSize或调用来源；wrapper不释放上游结果。
 * TODO: 现有两次拒绝意图覆盖初次请求和GC重试，但仅凭谓词/次数不能证明都来自queue array；若需此信用，核对实际allocator调用来源。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:779；tests/task/task_scheduler_queue_reuse_cases.inc:782；tests/task/task_scheduler_queue_reuse_cases.inc:783；tests/task/task_scheduler_queue_reuse_cases.inc:784；tests/task/task_scheduler_queue_reuse_cases.inc:786；tests/task/task_scheduler_queue_reuse_cases.inc:817；tests/task/task_scheduler_queue_reuse_cases.inc:820
 */
static TZrPtr queue_reuse_fail_queue_allocation(TZrPtr argument, TZrPtr pointer,
                                               TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    SZrQueueReuseAllocationFailure *failure = (SZrQueueReuseAllocationFailure *)argument;
    /* Reject the request and its GC retry; allow Error normalization to allocate. */
    if (failure->reject && failure->failures < 2u && pointer == ZR_NULL &&
        newSize == sizeof(SZrObject) && flag == ZR_MEMORY_NATIVE_TYPE_OBJECT) {
        ++failure->failures;
        return ZR_NULL;
    }
    return failure->upstream(failure->argument, pointer, originalSize, newSize, flag);
}

/**
 * ## Summary
 * 在已建scheduler后限量拒绝对象分配，观察建队失败、pin/ignored/queue清理和恢复上游allocator后的重试。
 * ## Returns
 * @return 无；局部数值与状态观察交给清理之后的 Unity 断言，失败计入父 main 的汇总退出码。
 * ## Constraints and lifecycle
 * allocator替换只覆盖该Global的同步调用且在异常清理/重试/销毁前恢复；两次匹配拒绝不唯一证明queue分配来源。
 * ## References
 * tests/task/task_scheduler_queue_reuse_cases.inc:803；tests/task/task_scheduler_queue_reuse_cases.inc:817；tests/task/task_scheduler_queue_reuse_cases.inc:819；tests/task/task_scheduler_queue_reuse_cases.inc:820；tests/task/task_scheduler_queue_reuse_cases.inc:822；tests/task/task_scheduler_queue_reuse_cases.inc:828；tests/task/task_scheduler_queue_reuse_cases.inc:831；tests/task/task_scheduler_queue_reuse_cases.inc:833；tests/task/task_scheduler_queue_reuse_cases.inc:838；tests/task/task_scheduler_queue_reuse_cases.inc:839；tests/task/task_scheduler_queue_reuse_cases.inc:842
 */
static void test_scheduler_queue_oom_releases_pin_and_can_retry(void) {
    SZrState *state = create_task_job_scheduler_test_state();
    SZrGcRootHandle root;
    TZrBool rooted = ZR_FALSE;
    SZrObject *scheduler = state == ZR_NULL ? ZR_NULL : queue_reuse_private_scheduler(state, ZR_NULL, ZR_FALSE, &root, &rooted);
    SZrQueueReuseAllocationFailure failure = {0};
    TZrBool rejected = ZR_FALSE;
    TZrBool cleaned = ZR_FALSE;
    TZrBool retried = ZR_FALSE;
    if (scheduler != ZR_NULL) {
        TZrUInt32 pins = scheduler->super.garbageCollectMark.pinFlags;
        TZrBool ignored = ZrCore_GarbageCollector_IsObjectIgnored(state->global, &scheduler->super);
    /** 仅在同步建队窗口替换allocator，并在错误清理、重试、根释放和状态销毁前恢复原函数及参数。
     * 包装器argument指向栈上failure；两个原allocator字段必须配套恢复，不能让cleanup继续引用该临时策略。
     */
        failure.upstream = state->global->upstreamAllocator;
        failure.argument = state->global->upstreamAllocationArguments;
        failure.reject = ZR_TRUE;
        state->global->upstreamAllocator = queue_reuse_fail_queue_allocation;
        state->global->upstreamAllocationArguments = &failure;
        rejected = task_runtime_scheduler_queue(state, scheduler) == ZR_NULL &&
                failure.failures == 2u && state->threadStatus == ZR_THREAD_STATUS_MEMORY_ERROR;
        failure.reject = ZR_FALSE;
        state->global->upstreamAllocator = failure.upstream;
        state->global->upstreamAllocationArguments = failure.argument;
    /** 在真实OOM后观察pinFlags、ignored标志及queue为空，再清除异常并重试。
     * 当前cleaned未比较head、memberVersion或ignoredObjectCount，重试成功也不代替这些失败后不变量。
     * TODO: 明确失败后head归零和memberVersion变化的契约，并增加相应观察；若要求ignored登记计数恢复，也须记录前后计数，不能只依据BOOL。
     */
        cleaned = scheduler->super.garbageCollectMark.pinFlags == pins &&
                ZrCore_GarbageCollector_IsObjectIgnored(state->global, &scheduler->super) == ignored &&
                task_runtime_scheduler_pair(scheduler, kTaskQueueField)->value.type == ZR_VALUE_TYPE_NULL;
        ZrCore_Exception_ClearCurrent(state);
        state->threadStatus = ZR_THREAD_STATUS_FINE;
        retried = task_runtime_scheduler_queue(state, scheduler) != ZR_NULL;
    }
    /** 重试后先释放Scheduler根和状态，再报告rejected/cleaned/retried三个独立观察。
     * Unity断言位于恢复allocator与VM清理之后；三个bool各有有限范围，不形成全队列runtime验收。
     */
    if (rooted) ZrCore_GcRootHandle_Release(state, &root);
    ZrTests_State_Destroy(state);
    TEST_ASSERT_TRUE_MESSAGE(rejected, "queue allocation OOM was not observed");
    TEST_ASSERT_TRUE_MESSAGE(cleaned, "queue OOM leaked pin or published queue");
    TEST_ASSERT_TRUE_MESSAGE(retried, "queue creation could not retry after OOM");
}
