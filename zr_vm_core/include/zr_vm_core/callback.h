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
 * # parameter callback macros:
 * Use ZR_CALLBACK_DECLARE_(NO/ONE/TWO)_PARAM(S) with
 * ZR_CALLBACK_IMPLEMENT_(NO/ONE/TWO)_PARAM(S) to declare and implement a callback function
 * you can use it like this: ZR_CALLBACK_DECLARE_(NO/ONE/TWO)_PARAM(S) to invoke a inline safe callback function
 *
 * # more parameters macros with a parameter provider
 * For more parameters or to use flexible parameters, you can use ZR_CALLBACK_DECLARE_N_PARAMS with a parameter
 * provider and ZR_CALLBACK_IMPLEMENT_N_PARAMS to implement it in source file
 * for example:
 *
 */
#define ZR_CALLBACK_DECLARE_NO_PARAM(TYPE)                                                                             \
    typedef void (*TYPE)(struct SZrState * STATE);                                                                     \
    struct SZrCallbackImpl_##TYPE {                                                                                    \
        TYPE TYPE;                                                                                                     \
    };                                                                                                                 \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments);

#define ZR_CALLBACK_IMPLEMENT_NO_PARAM(TYPE)                                                                           \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments) {                                             \
        struct SZrCallbackImpl_##TYPE *argumentStruct = (struct SZrCallbackImpl_##TYPE *) arguments;                   \
        argumentStruct->TYPE(state);                                                                                   \
    }

#define ZR_CALLBACK_CALL_NO_PARAM(STATE, TYPE, FUNCTION, OUT_STATUS)                                                   \
    {                                                                                                                  \
        struct SZrCallbackImpl_##TYPE argumentStruct;                                                                  \
        argumentStruct.TYPE = FUNCTION;                                                                                \
        OUT_STATUS = ZrExceptionTryRun(STATE, ZrCallbackImpl_##TYPE, &argumentStruct);                                 \
    }

#define ZR_CALLBACK_DECLARE_ONE_PARAM(TYPE, PARAM1_TYPE, PARAM1_NAME)                                                  \
    typedef void (*TYPE)(struct SZrState * STATE, PARAM1_TYPE PARAM1_NAME);                                            \
    struct SZrCallbackImpl_##TYPE {                                                                                    \
        TYPE TYPE;                                                                                                     \
        PARAM1_TYPE PARAM1_NAME;                                                                                       \
    };                                                                                                                 \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments);

#define ZR_CALLBACK_IMPLEMENT_ONE_PARAM(TYPE, PARAM1_NAME)                                                             \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments) {                                             \
        struct SZrCallbackImpl_##TYPE *argumentStruct = (struct SZrCallbackImpl_##TYPE *) arguments;                   \
        argumentStruct->TYPE(state, argumentStruct->PARAM1_NAME);                                                      \
    }

#define ZR_CALLBACK_CALL_ONE_PARAM(STATE, TYPE, FUNCTION, PARAM1_NAME, PARAM1_VALUE, OUT_STATUS)                       \
    {                                                                                                                  \
        struct SZrCallbackImpl_##TYPE argumentStruct;                                                                  \
        argumentStruct.TYPE = FUNCTION;                                                                                \
        argumentStruct.PARAM1_NAME = PARAM1_VALUE;                                                                     \
        OUT_STATUS = ZrExceptionTryRun(STATE, ZrCallbackImpl_##TYPE, &argumentStruct);                                 \
    }

#define ZR_CALLBACK_DECLARE_TWO_PARAMS(TYPE, PARAM1_TYPE, PARAM1_NAME, PARAM2_TYPE, PARAM2_NAME)                       \
    typedef void (*TYPE)(struct SZrState * STATE, PARAM1_TYPE PARAM1_NAME, PARAM2_TYPE PARAM2_NAME);                   \
    struct SZrCallbackImpl_##TYPE {                                                                                    \
        TYPE TYPE;                                                                                                     \
        PARAM1_TYPE PARAM1_NAME;                                                                                       \
        PARAM2_TYPE PARAM2_NAME;                                                                                       \
    };                                                                                                                 \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments);

#define ZR_CALLBACK_IMPLEMENT_TWO_PARAMS(TYPE, PARAM1_NAME, PARAM2_NAME)                                               \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments) {                                             \
        struct SZrCallbackImpl_##TYPE *argumentStruct = (struct SZrCallbackImpl_##TYPE *) arguments;                   \
        argumentStruct->TYPE(state, argumentStruct->PARAM1_NAME, argumentStruct->PARAM2_NAME);                         \
    }

#define ZR_CALLBACK_CALL_TWO_PARAMS(STATE, TYPE, FUNCTION, PARAM1_NAME, PARAM1_VALUE, PARAM2_NAME, PARAM2_VALUE,       \
                                    OUT_STATUS)                                                                        \
    {                                                                                                                  \
        struct SZrCallbackImpl_##TYPE argumentStruct;                                                                  \
        argumentStruct.TYPE = FUNCTION;                                                                                \
        argumentStruct.PARAM1_NAME = PARAM1_VALUE;                                                                     \
        argumentStruct.PARAM2_NAME = PARAM2_VALUE;                                                                     \
        OUT_STATUS = ZrExceptionTryRun(STATE, ZrCallbackImpl_##TYPE, &argumentStruct);                                 \
    }

#define ZR_CALLBACK_DECLARE_N_TYPE_CREATOR_S(T, N) T N,
#define ZR_CALLBACK_DECLARE_N_TYPE_CREATOR_E(T, N) T N
#define ZR_CALLBACK_DECLARE_N_STRUCT_CREATOR_SE(T, N) T N;
#define ZR_CALLBACK_IMPLEMENT_N_IMPL_CREATOR_S(T, N) argumentStruct->N,
#define ZR_CALLBACK_IMPLEMENT_N_IMPL_CREATOR_E(T, N) argumentStruct->N
#define ZR_CALLBACK_CALL_N_IMPL_CREATOR_SE(T, N) argumentStruct.N = N;


/**
 * e.g.
 * #define TEST_PROVIDER(S,E)\
 * S(TInt32, param1)\
 * S(TInt32, param2)\
 * E(TInt32, param3)
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
        return ZrExceptionTryRun(state, ZrCallbackImpl_##TYPE, &argumentStruct);                                       \
    }


// typedef FZrTryFunction FZrAfterStateInitialized;
ZR_CALLBACK_DECLARE_NO_PARAM(FZrAfterStateInitialized)

ZR_CALLBACK_DECLARE_NO_PARAM(FZrBeforeStateReleased)

ZR_CALLBACK_DECLARE_ONE_PARAM(FZrAfterThreadInitialized, struct SZrState *, threadState)

ZR_CALLBACK_DECLARE_ONE_PARAM(FZrBeforeThreadReleased, struct SZrState *, threadState)


struct ZR_STRUCT_ALIGN SZrCallbackGlobal {
    FZrAfterStateInitialized afterStateInitialized;
    FZrBeforeStateReleased beforeStateReleased;
    FZrAfterThreadInitialized afterThreadInitialized;
    FZrBeforeThreadReleased beforeThreadReleased;
};

typedef struct SZrCallbackGlobal SZrCallbackGlobal;
#endif // ZR_VM_CORE_CALLBACK_H
