//
// Created by HeJiahui on 2025/6/25.
//

#ifndef ZR_VM_CORE_CALLBACK_H
#define ZR_VM_CORE_CALLBACK_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/exception.h"


struct SZrState;
struct SZrGlobalState;


/**
 * @brief 生命周期回调的零/一/二参数适配器：DECLARE 定义类型和桥接入口，IMPLEMENT 生成桥接函数，CALL 经 TryRun 捕获异常状态。
 * @note 适配器只保存调用期间有效的栈参数；回调不得在返回后保留参数结构地址。直接调用回调字段不会自动进入 TryRun。
 */
/** @brief 声明无额外参数的回调及供 TryRun 使用的桥接入口。 */
#define ZR_CALLBACK_DECLARE_NO_PARAM(TYPE)                                                                             \
    typedef void (*TYPE)(struct SZrState * STATE);                                                                     \
    struct SZrCallbackImpl_##TYPE {                                                                                    \
        TYPE TYPE;                                                                                                     \
    };                                                                                                                 \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments);

/** @brief 在一个源文件生成无参数回调的桥接实现。 */
#define ZR_CALLBACK_IMPLEMENT_NO_PARAM(TYPE)                                                                           \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments) {                                             \
        struct SZrCallbackImpl_##TYPE *argumentStruct = (struct SZrCallbackImpl_##TYPE *) arguments;                   \
        argumentStruct->TYPE(state);                                                                                   \
    }

/** @brief 同步调用并将抛出的线程状态写入 OUT_STATUS。 */
#define ZR_CALLBACK_CALL_NO_PARAM(STATE, TYPE, FUNCTION, OUT_STATUS)                                                   \
    {                                                                                                                  \
        struct SZrCallbackImpl_##TYPE argumentStruct;                                                                  \
        argumentStruct.TYPE = FUNCTION;                                                                                \
        OUT_STATUS = ZrCore_Exception_TryRun(STATE, ZrCallbackImpl_##TYPE, &argumentStruct);                                 \
    }

/** @brief 声明单参数回调及其异常边界桥接入口。 */
#define ZR_CALLBACK_DECLARE_ONE_PARAM(TYPE, PARAM1_TYPE, PARAM1_NAME)                                                  \
    typedef void (*TYPE)(struct SZrState * STATE, PARAM1_TYPE PARAM1_NAME);                                            \
    struct SZrCallbackImpl_##TYPE {                                                                                    \
        TYPE TYPE;                                                                                                     \
        PARAM1_TYPE PARAM1_NAME;                                                                                       \
    };                                                                                                                 \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments);

/** @brief 在一个源文件生成单参数回调的桥接实现。 */
#define ZR_CALLBACK_IMPLEMENT_ONE_PARAM(TYPE, PARAM1_NAME)                                                             \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments) {                                             \
        struct SZrCallbackImpl_##TYPE *argumentStruct = (struct SZrCallbackImpl_##TYPE *) arguments;                   \
        argumentStruct->TYPE(state, argumentStruct->PARAM1_NAME);                                                      \
    }

/** @brief 同步执行单参数回调，调用者须检查 OUT_STATUS。 */
#define ZR_CALLBACK_CALL_ONE_PARAM(STATE, TYPE, FUNCTION, PARAM1_NAME, PARAM1_VALUE, OUT_STATUS)                       \
    {                                                                                                                  \
        struct SZrCallbackImpl_##TYPE argumentStruct;                                                                  \
        argumentStruct.TYPE = FUNCTION;                                                                                \
        argumentStruct.PARAM1_NAME = PARAM1_VALUE;                                                                     \
        OUT_STATUS = ZrCore_Exception_TryRun(STATE, ZrCallbackImpl_##TYPE, &argumentStruct);                                 \
    }

/** @brief 声明双参数回调及其异常边界桥接入口。 */
#define ZR_CALLBACK_DECLARE_TWO_PARAMS(TYPE, PARAM1_TYPE, PARAM1_NAME, PARAM2_TYPE, PARAM2_NAME)                       \
    typedef void (*TYPE)(struct SZrState * STATE, PARAM1_TYPE PARAM1_NAME, PARAM2_TYPE PARAM2_NAME);                   \
    struct SZrCallbackImpl_##TYPE {                                                                                    \
        TYPE TYPE;                                                                                                     \
        PARAM1_TYPE PARAM1_NAME;                                                                                       \
        PARAM2_TYPE PARAM2_NAME;                                                                                       \
    };                                                                                                                 \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments);

/** @brief 在一个源文件生成双参数回调的桥接实现。 */
#define ZR_CALLBACK_IMPLEMENT_TWO_PARAMS(TYPE, PARAM1_NAME, PARAM2_NAME)                                               \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments) {                                             \
        struct SZrCallbackImpl_##TYPE *argumentStruct = (struct SZrCallbackImpl_##TYPE *) arguments;                   \
        argumentStruct->TYPE(state, argumentStruct->PARAM1_NAME, argumentStruct->PARAM2_NAME);                         \
    }

/** @brief 同步执行双参数回调，调用者须检查 OUT_STATUS。 */
#define ZR_CALLBACK_CALL_TWO_PARAMS(STATE, TYPE, FUNCTION, PARAM1_NAME, PARAM1_VALUE, PARAM2_NAME, PARAM2_VALUE,       \
                                    OUT_STATUS)                                                                        \
    {                                                                                                                  \
        struct SZrCallbackImpl_##TYPE argumentStruct;                                                                  \
        argumentStruct.TYPE = FUNCTION;                                                                                \
        argumentStruct.PARAM1_NAME = PARAM1_VALUE;                                                                     \
        argumentStruct.PARAM2_NAME = PARAM2_VALUE;                                                                     \
        OUT_STATUS = ZrCore_Exception_TryRun(STATE, ZrCallbackImpl_##TYPE, &argumentStruct);                                 \
    }

/* N 参数适配器的内部展开元件；S 用于前置参数，E 用于最后一个参数。 */
#define ZR_CALLBACK_DECLARE_N_TYPE_CREATOR_S(T, N) T N,
#define ZR_CALLBACK_DECLARE_N_TYPE_CREATOR_E(T, N) T N
#define ZR_CALLBACK_DECLARE_N_STRUCT_CREATOR_SE(T, N) T N;
#define ZR_CALLBACK_IMPLEMENT_N_IMPL_CREATOR_S(T, N) argumentStruct->N,
#define ZR_CALLBACK_IMPLEMENT_N_IMPL_CREATOR_E(T, N) argumentStruct->N
#define ZR_CALLBACK_CALL_N_IMPL_CREATOR_SE(T, N) argumentStruct.N = N;


/**
 * @brief 以参数提供宏声明可变参数个数的回调与状态返回入口；IMPLEMENT_N_PARAMS 在一个源文件生成实现。
 * @note 提供宏的前置参数使用 S，最后一个参数必须使用 E；生成的 Invoke 只在同步调用期间借用参数。
 * e.g.
 * #define TEST_PROVIDER(S,E)\
 * S(TZrInt32, param1)\
 * S(TZrInt32, param2)\
 * E(TZrInt32, param3)
 * you must make the last parameter be wrapped with macro `E`
 * other parameters must be wrapped with macro `S`
 * @param TYPE Callback function name, e.g. FOnTest
 * @param PARAM_PROVIDER a macro with 2 parameters, the first one is a macro to create forward parameters, the second
 * one is a macro to create the last parameter, the macro contains S or E wraps the parameter type and name.
 */
#define ZR_CALLBACK_DECLARE_N_PARAMS(TYPE, PARAM_PROVIDER)                                                             \
    typedef void (*TYPE)(struct SZrState * STATE,                                                                      \
                         PARAM_PROVIDER(ZR_CALLBACK_DECLARE_N_TYPE_CREATOR_S, ZR_CALLBACK_DECLARE_N_TYPE_CREATOR_E));  \
    struct SZrCallbackImpl_##TYPE {                                                                                    \
        TYPE TYPE;                                                                                                     \
        PARAM_PROVIDER(ZR_CALLBACK_DECLARE_N_STRUCT_CREATOR_SE, ZR_CALLBACK_DECLARE_N_STRUCT_CREATOR_SE)               \
    };                                                                                                                 \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments);                                              \
    EZrThreadStatus ZrCallback_Invoke_##TYPE(                                                                          \
            struct SZrState *state, TYPE function,                                                                     \
            PARAM_PROVIDER(ZR_CALLBACK_DECLARE_N_TYPE_CREATOR_S, ZR_CALLBACK_DECLARE_N_TYPE_CREATOR_E));

/** @brief 生成 N 参数回调的 TryRun 桥接实现；与同名 DECLARE 配对。 */
#define ZR_CALLBACK_IMPLEMENT_N_PARAMS(TYPE, PARAM_PROVIDER)                                                           \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments) {                                             \
        struct SZrCallbackImpl_##TYPE *argumentStruct = (struct SZrCallbackImpl_##TYPE *) arguments;                   \
        argumentStruct->TYPE(state, PARAM_PROVIDER(ZR_CALLBACK_IMPLEMENT_N_IMPL_CREATOR_S,                             \
                                                   ZR_CALLBACK_IMPLEMENT_N_IMPL_CREATOR_E));                           \
    }                                                                                                                  \
    EZrThreadStatus ZrCallback_Invoke_##TYPE(                                                                          \
            struct SZrState *state, TYPE function,                                                                     \
            PARAM_PROVIDER(ZR_CALLBACK_DECLARE_N_TYPE_CREATOR_S, ZR_CALLBACK_DECLARE_N_TYPE_CREATOR_E)) {              \
        struct SZrCallbackImpl_##TYPE argumentStruct;                                                                  \
        argumentStruct.TYPE = function;                                                                                \
        PARAM_PROVIDER(ZR_CALLBACK_CALL_N_IMPL_CREATOR_SE, ZR_CALLBACK_CALL_N_IMPL_CREATOR_SE)                         \
        return ZrCore_Exception_TryRun(state, ZrCallbackImpl_##TYPE, &argumentStruct);                                       \
    }


/** 主线程启动并完成全局状态初始化后经 TryRun 调用；失败状态由创建流程继续抛出。 */
ZR_CALLBACK_DECLARE_NO_PARAM(FZrAfterStateInitialized)

/** TODO: 已由 GlobalState_New 保存，但当前释放链未发现派发点；需核对预期销毁顺序。 */
ZR_CALLBACK_DECLARE_NO_PARAM(FZrBeforeStateReleased)

/** TODO: 已由 GlobalState_New 保存，但当前线程创建链未发现派发点；需核对是否为预留接口。 */
ZR_CALLBACK_DECLARE_ONE_PARAM(FZrAfterThreadInitialized, struct SZrState *, threadState)

/** GC 消费待释放线程队列时调用；回调不得跨越队列摘链后的异常边界。 */
ZR_CALLBACK_DECLARE_ONE_PARAM(FZrBeforeThreadReleased, struct SZrState *, threadState)


/** 全局生命周期回调表；由宿主注册并在 State/GC 路径读取，未安装的字段为零。 */
/** TODO: 尚未找到待释放队列的生产者；若启用此派发，需保证回调异常退出后线程对象仍可追踪。 */
struct ZR_STRUCT_ALIGN SZrCallbackGlobal {
    FZrAfterStateInitialized afterStateInitialized;
    FZrBeforeStateReleased beforeStateReleased;
    FZrAfterThreadInitialized afterThreadInitialized;
    FZrBeforeThreadReleased beforeThreadReleased;
};

typedef struct SZrCallbackGlobal SZrCallbackGlobal;
#endif // ZR_VM_CORE_CALLBACK_H
