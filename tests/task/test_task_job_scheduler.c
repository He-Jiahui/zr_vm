#include <string.h>

#include "unity.h"
#include "test_support.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_parser.h"

/* 注册核心 task 运行时及 parser，供源码编译与描述符回调共用。 */
/**
 * ## Summary
 * 为每个独立Job/Scheduler用例建立VM、注册zr.task静态描述符，再安装当前Global的源码编译入口，让源用例与直接descriptor调用使用同一提供者。
 * ## Returns
 * @return 成功时返回测试独占的VM状态；失败NULL。描述符是静态借用，不由测试释放；VM资源随Destroy归还。
 * ## Constraints and lifecycle
 * 状态/Global或builtins注册失败会销毁已得状态并返回NULL。Parser注册是void设置compileSource回调，不执行源代码；成功状态仍由每个用例最终Destroy。
 * ## References
 * tests/task/test_task_job_scheduler.c:23；tests/task/test_task_job_scheduler.c:28；tests/task/test_task_job_scheduler.c:29；tests/task/test_task_job_scheduler.c:33；tests/task/test_task_job_scheduler.c:35；zr_vm_library/src/zr_vm_library/task/task_runtime.c:1389；zr_vm_library/src/zr_vm_library/task/task_runtime.c:1393；zr_vm_parser/src/zr_vm_parser/compiler/compiler.c:1458；tests/harness/runtime_support.c:410
 */
static SZrState *create_task_job_scheduler_test_state(void) {
    SZrState *state = ZrTests_State_Create(ZR_NULL);

    /** 只有具有效Global和task注册的状态才安装Source_Compile，避免负测在无提供者fixture上直接运行。
     * 创建失败清理后返回NULL；parser注册是回调赋值，不是另一函数执行或运行验收。
     */
    if (state == ZR_NULL || state->global == ZR_NULL || !ZrCore_TaskRuntime_RegisterBuiltins(state->global)) {
        ZrTests_State_Destroy(state);
        return ZR_NULL;
    }

    ZrParser_ToGlobalState_Register(state);

    return state;
}

/* 描述符指针依附原生模块注册表，调用方不能在 state 销毁后持有。 */
/**
 * ## Summary
 * 给descriptor形状和直接ABI测试按精确名称借出types项，保留真实注册表元数据和callback身份，不构造替代接口。
 * ## Parameters
 * @param[in] module 当前用例已取得的借用descriptor；本函数只读取其表，可为空。
 * @param[in] typeName 查找的NUL结尾精确名称，调用期间借用；可为空。
 * ## Returns
 * @return 首个匹配的原表地址，未找到或空输入返回NULL；调用者检查后立即使用，不释放、修改或将其当作VM对象根。
 * ## Constraints and lifecycle
 * 容器或名称NULL返回NULL；调用者须提供有效types/typeCount表，本函数不验证指针与计数一致性。返回值借用原表，当前builtins为静态表；查找不验证完整ABI，也不创建GC根。
 * ## References
 * tests/task/test_task_job_scheduler.c:52；tests/task/test_task_job_scheduler.c:60；tests/task/test_task_job_scheduler.c:61；tests/task/test_task_job_scheduler.c:62；zr_vm_library/src/zr_vm_library/task/task_runtime.c:1367；zr_vm_library/src/zr_vm_library/task/task_runtime.c:1369
 */
static const ZrLibTypeDescriptor *find_type_descriptor(const ZrLibModuleDescriptor *module,
                                                        const char *typeName) {
    TZrSize index;

    if (module == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < module->typeCount; index++) {
        if (module->types[index].name != ZR_NULL && strcmp(module->types[index].name, typeName) == 0) {
            return &module->types[index];
        }
    }

    return ZR_NULL;
}

/* 通过公共源码编译入口验证 Job/Scheduler 语义，而非手造字节码。 */
/**
 * ## Summary
 * 把静态ZR场景经公共Source_Compile编译，给负测提供编译期拒绝入口，给正常和queue用例提供真实可执行Function。
 * ## Parameters
 * @param[in] state 当前独立VM，借用且须有对应注册表与编译支持。
 * @param[in] source 当前测试ZR源文本，借用至同步编译返回；函数对象不是对文本的所有权转移。
 * @param[in] name SourceName诊断标识，借用至String_Create完成。
 * ## Returns
 * @return 成功为当前VM管理的函数地址，调用者立即交执行支持；NULL仅表示某项构建/编译失败，不单独证明消费或must-use诊断。状态销毁后不可使用。
 * ## Constraints and lifecycle
 * state/source/name非空；source/name都是NUL结尾C字符串，strlen使嵌入NUL之后内容不参与编译。SourceName分配失败与解析/编译失败均可返回NULL；这里只返回指针，不输出结构化诊断。TODO: 需要区分预期拒绝时读取当前编译诊断；当前成功调用方同步立即消费Function；若未来要跨后续分配保存它，需核查显式GC保护，不能把裸返回地址当根。
 * ## References
 * tests/task/test_task_job_scheduler.c:88；tests/task/test_task_job_scheduler.c:92；tests/task/test_task_job_scheduler.c:93；tests/task/test_task_job_scheduler.c:96；zr_vm_parser/src/zr_vm_parser/compiler/compiler.c:1387
 */
static SZrFunction *compile_task_job_scheduler_source(SZrState *state, const char *source, const char *name) {
    SZrString *sourceName;

    if (state == ZR_NULL || source == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)name, strlen(name));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

/* 查找方法的 ABI 字段及回调，用于源语言和原生调用的交叉检查。 */
/**
 * ## Summary
 * 给descriptor形状和直接ABI测试按精确名称借出methods项，保留真实注册表元数据和callback身份，不构造替代接口。
 * ## Parameters
 * @param[in] type 当前用例已取得的借用descriptor；本函数只读取其表，可为空。
 * @param[in] methodName 查找的NUL结尾精确名称，调用期间借用；可为空。
 * ## Returns
 * @return 首个匹配的原表地址，未找到或空输入返回NULL；调用者检查后立即使用，不释放、修改或将其当作VM对象根。
 * ## Constraints and lifecycle
 * 容器或名称NULL返回NULL；调用者须提供有效methods/methodCount表，本函数不验证指针与计数一致性。返回值借用原表，当前builtins为静态表；查找不验证完整ABI，也不创建GC根。
 * ## References
 * tests/task/test_task_job_scheduler.c:113；tests/task/test_task_job_scheduler.c:121；tests/task/test_task_job_scheduler.c:122；tests/task/test_task_job_scheduler.c:123；zr_vm_library/src/zr_vm_library/task/task_runtime.c:1367；zr_vm_library/src/zr_vm_library/task/task_runtime.c:1369
 */
static const ZrLibMethodDescriptor *find_method_descriptor(const ZrLibTypeDescriptor *type,
                                                            const char *methodName) {
    TZrSize index;

    if (type == ZR_NULL || methodName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < type->methodCount; index++) {
        if (type->methods[index].name != ZR_NULL && strcmp(type->methods[index].name, methodName) == 0) {
            return &type->methods[index];
        }
    }

    return ZR_NULL;
}

/* 查找模块函数的 contractRole、返回类型和原生回调。 */
/**
 * ## Summary
 * 给descriptor形状和直接ABI测试按精确名称借出functions项，保留真实注册表元数据和callback身份，不构造替代接口。
 * ## Parameters
 * @param[in] module 当前用例已取得的借用descriptor；本函数只读取其表，可为空。
 * @param[in] functionName 查找的NUL结尾精确名称，调用期间借用；可为空。
 * ## Returns
 * @return 首个匹配的原表地址，未找到或空输入返回NULL；调用者检查后立即使用，不释放、修改或将其当作VM对象根。
 * ## Constraints and lifecycle
 * 容器或名称NULL返回NULL；调用者须提供有效functions/functionCount表，本函数不验证指针与计数一致性。返回值借用原表，当前builtins为静态表；查找不验证完整ABI，也不创建GC根。
 * ## References
 * tests/task/test_task_job_scheduler.c:144；tests/task/test_task_job_scheduler.c:152；tests/task/test_task_job_scheduler.c:153；tests/task/test_task_job_scheduler.c:154；zr_vm_library/src/zr_vm_library/task/task_runtime.c:1367；zr_vm_library/src/zr_vm_library/task/task_runtime.c:1369
 */
static const ZrLibFunctionDescriptor *find_function_descriptor(const ZrLibModuleDescriptor *module,
                                                                const char *functionName) {
    TZrSize index;

    if (module == ZR_NULL || functionName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < module->functionCount; index++) {
        if (module->functions[index].name != ZR_NULL && strcmp(module->functions[index].name, functionName) == 0) {
            return &module->functions[index];
        }
    }

    return ZR_NULL;
}

/* 用 Task.result 原生回调消费完成结果，验证 yieldNow/delay 返回 ABI。 */
/**
 * ## Summary
 * 把yieldNow/delay返回的Task作为真实self交给Task.result callback，用NULL值类型观察void完成ABI，不经过源码调用适配器。
 * ## Parameters
 * @param[in] state Task所属当前VM，调用期间借用。
 * @param[in] resultMethod 从Task类型借出的result descriptor，必须带有效callback。
 * @param[in] taskValue 先前回调写成的Task对象值，立即同步借用为self，不由此helper取得根。
 * ## Returns
 * @return 无；Unity断言检查回调true和NULL标签；失败按Unity机制中断本用例。此读取路径有目标槽BUG，断言源码不代表运行通过。
 * ## Constraints and lifecycle
 * BUG: 局部result未初始化即交给Task.result；正常COMPLETED包含typed-null字段，仍走Value_Copy，覆盖前读取目标ownership元数据，可能误释资源或崩溃。返回后的bool/type断言无法保护回调内读取；需在回调前初始化槽，本轮保留代码。
 * ## References
 * tests/task/test_task_job_scheduler.c:183；tests/task/test_task_job_scheduler.c:191；tests/task/test_task_job_scheduler.c:192；zr_vm_library/src/zr_vm_library/task/task_runtime.c:795；zr_vm_library/src/zr_vm_library/task/task_runtime.c:710；zr_vm_library/src/zr_vm_library/task/task_runtime.c:713；zr_vm_library/src/zr_vm_library/task/task_runtime.c:1162；zr_vm_library/src/zr_vm_library/task/task_runtime.c:835；zr_vm_library/src/zr_vm_library/task/task_runtime.c:836；zr_vm_library/src/zr_vm_library/task/task_runtime.c:837；zr_vm_library/src/zr_vm_library/task/task_runtime.c:216；zr_vm_library/src/zr_vm_library/task/task_runtime.c:217；zr_vm_core/include/zr_vm_core/value.h:151；zr_vm_core/include/zr_vm_core/value.h:161；zr_vm_core/include/zr_vm_core/value.h:229
 */
static void assert_task_completes_with_void(SZrState *state,
                                            const ZrLibMethodDescriptor *resultMethod,
                                            SZrTypeValue *taskValue) {
    ZrLibCallContext context = {0};
    /** 结果槽由Task.result以Copy规则覆盖，需要进入回调之前满足值所有权初始化约束。
     * BUG: 局部result未初始化即交给Task.result；正常COMPLETED包含typed-null字段，仍走Value_Copy，覆盖前读取目标ownership元数据，可能误释资源或崩溃。返回后的bool/type断言无法保护回调内读取；需在回调前初始化槽，本轮保留代码。
     */
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(resultMethod);
    TEST_ASSERT_NOT_NULL(resultMethod->callback);
    TEST_ASSERT_NOT_NULL(taskValue);
    context.state = state;
    context.selfValue = taskValue;
    TEST_ASSERT_TRUE(resultMethod->callback(&context, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, result.type);
}

/* zr.task 描述符对外发布 Job、Scheduler、yieldNow、delay 的 canonical 形状。 */
/**
 * ## Summary
 * 验证zr.task 3.0.0中Job构造、Scheduler.schedule和yieldNow/delay的部分canonical metadata，让编译器可识别protocol与contractRole。
 * ## Returns
 * @return 无；Unity收集该用例的断言结果，正常路径销毁独立VM；断言失败中断时不承诺继续执行尾部Destroy。
 * ## Constraints and lifecycle
 * 只读取已注册静态descriptor，不执行Job、不物化currentScheduler、不检查完整参数/方法表或字段发布；参数0直接索引以当前有效静态表为前提。各借用地址只在当前测试使用，最终Destroy释放VM。
 * ## References
 * tests/task/test_task_job_scheduler.c:216；tests/task/test_task_job_scheduler.c:223；tests/task/test_task_job_scheduler.c:229；tests/task/test_task_job_scheduler.c:236；tests/task/test_task_job_scheduler.c:241；tests/task/test_task_job_scheduler.c:244；tests/task/test_task_job_scheduler.c:255；tests/task/test_task_job_scheduler.c:258；tests/task/test_task_job_scheduler.c:260；tests/task/test_task_job_scheduler.c:262
 */
static void test_zr_task_descriptor_publishes_job_scheduler_contract(void) {
    SZrState *state = create_task_job_scheduler_test_state();
    const ZrLibModuleDescriptor *taskModule;
    const ZrLibTypeDescriptor *jobType;
    const ZrLibTypeDescriptor *schedulerType;
    const ZrLibMethodDescriptor *schedule;
    const ZrLibFunctionDescriptor *yieldNow;
    const ZrLibFunctionDescriptor *delay;

    TEST_ASSERT_NOT_NULL(state);
    taskModule = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.task");
    TEST_ASSERT_NOT_NULL(taskModule);
    TEST_ASSERT_EQUAL_STRING("3.0.0", taskModule->moduleVersion);

    /** 用protocol/constructor role和泛型返回shape连接Job元数据与编译器一次消费识别。
     * 只断言所列字段，不构造Job或证明callable执行。
     */
    jobType = find_type_descriptor(taskModule, "Job");
    TEST_ASSERT_NOT_NULL(jobType);
    TEST_ASSERT_EQUAL_UINT64(1U, jobType->genericParameterCount);
    TEST_ASSERT_TRUE((jobType->protocolMask & ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_JOB)) != 0U);
    TEST_ASSERT_EQUAL_UINT64(1U, jobType->metaMethodCount);
    TEST_ASSERT_EQUAL_INT(ZR_META_CONSTRUCTOR, jobType->metaMethods[0].metaType);
    TEST_ASSERT_EQUAL_UINT(ZR_MEMBER_CONTRACT_ROLE_TASK_JOB_CONSTRUCT, jobType->metaMethods[0].contractRole);
    TEST_ASSERT_EQUAL_STRING("zr.task.Job<T>", jobType->metaMethods[0].returnTypeName);
    TEST_ASSERT_NOT_NULL(jobType->metaMethods[0].callback);

    /** 验证Scheduler capability及schedule对Job<T>到Task<T>的表契约。
     * 参数0和generic形状是当前静态表局部观察，不授予完整ABI验证。
     */
    schedulerType = find_type_descriptor(taskModule, "Scheduler");
    TEST_ASSERT_NOT_NULL(schedulerType);
    TEST_ASSERT_TRUE((schedulerType->protocolMask & ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_SCHEDULER)) != 0U);
    schedule = find_method_descriptor(schedulerType, "schedule");
    TEST_ASSERT_NOT_NULL(schedule);
    TEST_ASSERT_EQUAL_UINT(ZR_MEMBER_CONTRACT_ROLE_TASK_SCHEDULER_SCHEDULE, schedule->contractRole);
    TEST_ASSERT_EQUAL_STRING("zr.task.Task<T>", schedule->returnTypeName);
    TEST_ASSERT_EQUAL_UINT64(1U, schedule->genericParameterCount);
    TEST_ASSERT_EQUAL_STRING("zr.task.Job<T>", schedule->parameters[0].typeName);

    /** 用yieldNow/delay的role、Task<void>与Duration文字shape连接编译工具和直接callback入口。
     * metadata中的Duration名称不等于runtime真实墙钟；下一用例直接传整数0。
     */
    yieldNow = find_function_descriptor(taskModule, "yieldNow");
    delay = find_function_descriptor(taskModule, "delay");
    TEST_ASSERT_NOT_NULL(yieldNow);
    TEST_ASSERT_NOT_NULL(delay);
    TEST_ASSERT_EQUAL_UINT(ZR_MEMBER_CONTRACT_ROLE_TASK_YIELD_NOW, yieldNow->contractRole);
    TEST_ASSERT_EQUAL_UINT(ZR_MEMBER_CONTRACT_ROLE_TASK_DELAY, delay->contractRole);
    TEST_ASSERT_EQUAL_STRING("zr.task.Task<void>", yieldNow->returnTypeName);
    TEST_ASSERT_EQUAL_STRING("zr.task.Task<void>", delay->returnTypeName);
    TEST_ASSERT_EQUAL_UINT64(1U, delay->parameterCount);
    TEST_ASSERT_EQUAL_STRING("Duration", delay->parameters[0].typeName);
    TEST_ASSERT_NOT_NULL(yieldNow->callback);
    TEST_ASSERT_NOT_NULL(delay->callback);

    ZrTests_State_Destroy(state);
}

/* 直接调用模块回调后经 Task.result 取 void，覆盖原生边界。 */
/**
 * ## Summary
 * 直接调用yieldNow和delay(0)描述符，然后通过helper取Task<void>结果，交叉观察模块返回ABI与Task.result self调用。
 * ## Returns
 * @return 无；Unity收集该用例的断言结果，正常路径销毁独立VM；断言失败中断时不承诺继续执行尾部Destroy。
 * ## Constraints and lifecycle
 * yieldNow使用一个cooperative轮次，delay输入整数0；不是Duration转换/墙钟/OS worker验证。Task对象输出槽由SetObject初始化，但assert helper的完成值目标槽未初始化，存在已证BUG；两者不可混淆。
 * ## References
 * tests/task/test_task_job_scheduler.c:304；tests/task/test_task_job_scheduler.c:305；tests/task/test_task_job_scheduler.c:310；tests/task/test_task_job_scheduler.c:314；tests/task/test_task_job_scheduler.c:315；tests/task/test_task_job_scheduler.c:317；zr_vm_library/src/zr_vm_library/task/task_runtime.c:795；zr_vm_library/src/zr_vm_library/task/task_runtime.c:710；zr_vm_library/src/zr_vm_library/task/task_runtime.c:713；zr_vm_library/src/zr_vm_library/task/task_runtime.c:1162；zr_vm_library/src/zr_vm_library/task/task_runtime.c:835；zr_vm_library/src/zr_vm_library/task/task_runtime.c:836；zr_vm_library/src/zr_vm_library/task/task_runtime.c:837；zr_vm_library/src/zr_vm_library/task/task_runtime.c:216；zr_vm_library/src/zr_vm_library/task/task_runtime.c:217；zr_vm_core/include/zr_vm_core/value.h:151；zr_vm_core/include/zr_vm_core/value.h:161；zr_vm_core/include/zr_vm_core/value.h:229
 */
static void test_yield_now_and_delay_complete_through_task_result_abi(void) {
    SZrState *state = create_task_job_scheduler_test_state();
    const ZrLibModuleDescriptor *taskModule;
    const ZrLibTypeDescriptor *taskType;
    const ZrLibMethodDescriptor *resultMethod;
    const ZrLibFunctionDescriptor *yieldNow;
    const ZrLibFunctionDescriptor *delay;
    ZrLibCallContext yieldContext = {0};
    ZrLibCallContext delayContext = {0};
    SZrTypeValue delayArgument;
    SZrTypeValue yieldTask;
    SZrTypeValue delayTask;

    TEST_ASSERT_NOT_NULL(state);
    taskModule = ZrLibrary_NativeRegistry_FindModule(state->global, "zr.task");
    TEST_ASSERT_NOT_NULL(taskModule);
    taskType = find_type_descriptor(taskModule, "Task");
    TEST_ASSERT_NOT_NULL(taskType);
    resultMethod = find_method_descriptor(taskType, "result");
    yieldNow = find_function_descriptor(taskModule, "yieldNow");
    delay = find_function_descriptor(taskModule, "delay");
    TEST_ASSERT_NOT_NULL(yieldNow);
    TEST_ASSERT_NOT_NULL(delay);

    /** 只设置state后直接调用yieldNow，再通过helper使用返回Task作self。
     * 没有源码参数适配或provider hook；helper目标槽BUG会在正常完成时触发未定义读取。
     */
    yieldContext.state = state;
    TEST_ASSERT_TRUE(yieldNow->callback(&yieldContext, &yieldTask));
    assert_task_completes_with_void(state, resultMethod, &yieldTask);

    /** 为delay提供一个整数0实参，再用同一完成值helper观察Task<void>。
     * context及参数值在同步调用内借用，先SetInt初始化输入；仍受helper完成值目标槽BUG影响，不是输入/Task输出未初始化。
     */
    ZrLib_Value_SetInt(state, &delayArgument, 0);
    delayContext.state = state;
    delayContext.argumentValues = &delayArgument;
    delayContext.argumentCount = 1U;
    TEST_ASSERT_TRUE(delay->callback(&delayContext, &delayTask));
    assert_task_completes_with_void(state, resultMethod, &delayTask);

    ZrTests_State_Destroy(state);
}

/* Job 被 schedule 消费一次；同一绑定再调用须在编译阶段拒绝。 */
/**
 * ## Summary
 * 把同一cold Job依次提交两次，意图在第二次源调用被拒绝时保护一次性消费契约。
 * ## Returns
 * @return 无；Unity收集该用例的断言结果，正常路径销毁独立VM；断言失败中断时不承诺继续执行尾部Destroy。
 * ## Constraints and lifecycle
 * 只断言Compile返回NULL，没有运行源代码。TODO: 核对消费诊断的类别/位置并先确认单次调用能编译，避免导入、语法或其他失败让此负测假阳性；NULL不能证明特定拒绝原因。
 * ## References
 * tests/task/test_task_job_scheduler.c:352；tests/task/test_task_job_scheduler.c:353；tests/task/test_task_job_scheduler.c:354；tests/task/test_task_job_scheduler.c:359；tests/task/test_task_job_scheduler.c:364
 */
static void test_scheduler_consumes_job_at_source_call_boundary(void) {
    /**
     * ## Summary
     * 为一次性Job负测提供唯一Job和两次schedule调用的完整源，返回值未执行。
     * ## Constraints and lifecycle
     * 静态字符串被C编译保留原样；NULL编译结果必须结合真实诊断解释，不能把语法文本当已执行函数。
     * ## References
     * tests/task/test_task_job_scheduler.c:352；tests/task/test_task_job_scheduler.c:353；tests/task/test_task_job_scheduler.c:354
     */
    static const char *source =
            "var task = import(\"zr.task\");\n"
    /**
     * ## Summary
     * 为Job<int>提供无参数纯callable，设计返回整数7以连接构造、schedule和Task.result契约；它属于紧随源字符串，不是C回调。
     * ## Returns
     * @return 按源设计返回整数7；实际执行由Job/Scheduler支持负责，C编译不证明ZR运行。
     * ## Constraints and lifecycle
     * 无捕获和额外资源；负测源不执行该fn，正常源只有返回值断言，不能由纯结果推断次数。注释位于字符串外，不改变解析输入。
     * ## References
     * tests/task/test_task_job_scheduler.c:352；tests/task/test_task_job_scheduler.c:359；zr_vm_library/src/zr_vm_library/task/task_runtime.c:1148；zr_vm_library/src/zr_vm_library/task/task_runtime.c:1152
     */
            "var job = init task.Job<int>(fn() => { return 7; });\n"
            "var first = task.currentScheduler.schedule<int>(job);\n"
            "var second = task.currentScheduler.schedule<int>(job);\n"
            "return 0;\n";
    SZrState *state = create_task_job_scheduler_test_state();

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NULL(compile_task_job_scheduler_source(
            state,
            source,
            "task_job_scheduler_single_consume.zr"));

    ZrTests_State_Destroy(state);
}

/* Task 是 must-use 结果，直接丢弃 yieldNow 返回值应拒绝。 */
/**
 * ## Summary
 * 以独立表达式丢弃yieldNow返回Task，意图约束Task结果必须被等待、返回或保存。
 * ## Returns
 * @return 无；Unity收集该用例的断言结果，正常路径销毁独立VM；断言失败中断时不承诺继续执行尾部Destroy。
 * ## Constraints and lifecycle
 * 只断言Compile返回NULL，不执行yieldNow。TODO: 读取must-use诊断及具体位置，区分导入/其他编译失败；它不是成功运行或Task资源清理测试。
 * ## References
 * tests/task/test_task_job_scheduler.c:389；tests/task/test_task_job_scheduler.c:394；tests/task/test_task_job_scheduler.c:396；zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement.c:2957；zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement.c:2958；zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement.c:2959
 */
static void test_discarded_task_expression_is_rejected(void) {
    /**
     * ## Summary
     * 提供导入后直接丢弃yieldNow Task的完整负测文本，触发目标是Task的must-use规则。
     * ## Constraints and lifecycle
     * 无注入函数，只有模块调用表达式；本模板不证明yieldNow调度或正常完成。
     * ## References
     * tests/task/test_task_job_scheduler.c:388；tests/task/test_task_job_scheduler.c:389
     */
    static const char *source =
            "var task = import(\"zr.task\");\n"
            "task.yieldNow();\n"
            "return 0;\n";
    SZrState *state = create_task_job_scheduler_test_state();

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NULL(compile_task_job_scheduler_source(state, source, "task_job_scheduler_must_use.zr"));

    ZrTests_State_Destroy(state);
}

/* 源语言端到端经过 Job 构造、当前 Scheduler 调度和 Task.result。 */
/**
 * ## Summary
 * 串起源导入、cold Job构造、currentScheduler.schedule和completion.result，观察真实执行返回整数7。
 * ## Returns
 * @return 无；Unity收集该用例的断言结果，正常路径销毁独立VM；断言失败中断时不承诺继续执行尾部Destroy。
 * ## Constraints and lifecycle
 * 正常源路径依赖当前注册表/公共编译和整数执行支持，当前断言仅返回7。TODO: 若要证明名字中的callable_once，需可观察的调用计数或副作用；重复纯函数仍可返回7，当前断言不证明次数。
 * ## References
 * tests/task/test_task_job_scheduler.c:431；tests/task/test_task_job_scheduler.c:432；tests/task/test_task_job_scheduler.c:433；tests/task/test_task_job_scheduler.c:443；tests/task/test_task_job_scheduler.c:445；tests/task/test_task_job_scheduler.c:446；tests/task/test_task_job_scheduler.c:448；tests/harness/runtime_support.c:429；tests/harness/runtime_support.c:447；tests/harness/runtime_support.c:466
 */
static void test_job_constructor_and_current_scheduler_schedule_complete_callable_once(void) {
    /**
     * ## Summary
     * 提供Job到Scheduler到Task.result的正常源链，让返回7成为可观察的端到端结果。
     * ## Constraints and lifecycle
     * 静态借用至编译返回；并无调用次数计数，fn字符串字节本轮保持原样。
     * ## References
     * tests/task/test_task_job_scheduler.c:431；tests/task/test_task_job_scheduler.c:432；tests/task/test_task_job_scheduler.c:433；tests/task/test_task_job_scheduler.c:434
     */
    static const char *source =
            "var task = import(\"zr.task\");\n"
    /**
     * ## Summary
     * 为Job<int>提供无参数纯callable，设计返回整数7以连接构造、schedule和Task.result契约；它属于紧随源字符串，不是C回调。
     * ## Returns
     * @return 按源设计返回整数7；实际执行由Job/Scheduler支持负责，C编译不证明ZR运行。
     * ## Constraints and lifecycle
     * 无捕获和额外资源；负测源不执行该fn，正常源只有返回值断言，不能由纯结果推断次数。注释位于字符串外，不改变解析输入。
     * ## References
     * tests/task/test_task_job_scheduler.c:431；tests/task/test_task_job_scheduler.c:443；zr_vm_library/src/zr_vm_library/task/task_runtime.c:1148；zr_vm_library/src/zr_vm_library/task/task_runtime.c:1152
     */
            "var job = init task.Job<int>(fn() => { return 7; });\n"
            "var completion = task.currentScheduler.schedule<int>(job);\n"
            "var value = completion.result();\n"
            "return value;\n";
    SZrState *state = create_task_job_scheduler_test_state();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    /** 在编译成功后执行完整源并比较返回7，以验证跨构造/调度/result链的可观察结果。
     * TODO: 没有记录callable调用次数，重复返回常量7不能由此观察区分；行为修复或新增测试留后续。
     */
    function = compile_task_job_scheduler_source(state, source, "task_job_scheduler_round_trip.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);

    ZrTests_State_Destroy(state);
}

/**
 * ## Summary
 * 把五个queue reuse回归和其宿主回调编入同一TU，共用此文件的真实fixture、compiler与descriptor查找支持。
 * ## Constraints and lifecycle
 * include不是另一测试进程；片段有独立59项台账。后续main顺序登记五个父用例及五个片段用例，静态saved根依赖串行执行。
 * ## References
 * tests/task/test_task_job_scheduler.c:459；tests/task/test_task_job_scheduler.c:478；tests/task/test_task_job_scheduler.c:482；tests/CMakeLists.txt:4753
 */
#include "task_scheduler_queue_reuse_cases.inc"

/**
 * ## Summary
 * 把五个父文件契约用例与五个queue片段回归串成单一Unity程序，将Unity汇总状态交给CTest调用者。
 * ## Returns
 * @return UNITY_END的整程序汇总退出码，非零表示用例失败；注册与C编译本身都不代表用例已运行。
 * ## Constraints and lifecycle
 * 十个RUN_TEST按当前顺序同步执行；CTest名task_job_scheduler只选择整程序，120秒是整程序timeout。状态/根清理由各用例承担，不由main自动接管。
 * ## References
 * tests/task/test_task_job_scheduler.c:472；tests/task/test_task_job_scheduler.c:473；tests/task/test_task_job_scheduler.c:482；tests/task/test_task_job_scheduler.c:486；tests/cmake/task-scheduler-queue-tests.cmake:23
 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zr_task_descriptor_publishes_job_scheduler_contract);
    RUN_TEST(test_job_constructor_and_current_scheduler_schedule_complete_callable_once);
    RUN_TEST(test_yield_now_and_delay_complete_through_task_result_abi);
    RUN_TEST(test_scheduler_consumes_job_at_source_call_boundary);
    RUN_TEST(test_discarded_task_expression_is_rejected);
    RUN_TEST(test_scheduler_reuse_preserves_completed_job_result);
    RUN_TEST(test_scheduler_keeps_reentrant_enqueue_until_outer_job_returns);
    RUN_TEST(test_scheduler_reuses_queue_for_cooperative_turns);
    RUN_TEST(test_scheduler_checked_fields_reject_writes_without_replaying);
    RUN_TEST(test_scheduler_queue_oom_releases_pin_and_can_retry);
    /** 将当前十个程序内用例的Unity汇总退出码传播到CTest。
     * 失败断言可中断当前用例；main不恢复VM根和注册表，不把进入下一个用例等同于前一资源清理完成。
     */
    return UNITY_END();
}
