//
// Created by HeJiahui on 2025/6/25.
//

#ifndef ZR_VM_CORE_CALLBACK_H
#define ZR_VM_CORE_CALLBACK_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/exception.h"


struct SZrState;
struct SZrGlobalState;

#define ZR_CALLBACK_DECLARE_NO_PARAM(TYPE) \
    typedef void (*TYPE)(struct SZrState *STATE);\
    struct SZrCallbackImpl_##TYPE{TYPE TYPE;};\
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments);

#define ZR_CALLBACK_IMPLEMENT_NO_PARAM(TYPE) \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments) {\
        struct SZrCallbackImpl_##TYPE *argumentStruct = (struct SZrCallbackImpl_##TYPE *)arguments;\
        argumentStruct->TYPE(state);\
    }

#define ZR_CALLBACK_CALL_NO_PARAM(STATE, TYPE, FUNCTION, OUT_STATUS) \
    {\
        struct SZrCallbackImpl_##TYPE argumentStruct; argumentStruct.TYPE = FUNCTION;\
        OUT_STATUS = ZrExceptionTryRun(STATE, ZrCallbackImpl_##TYPE, &argumentStruct);\
    }

#define ZR_CALLBACK_DECLARE_ONE_PARAM(TYPE, PARAM1_TYPE, PARAM1_NAME) \
    typedef void (*TYPE)(struct SZrState *STATE, PARAM1_TYPE PARAM1_NAME);\
    struct SZrCallbackImpl_##TYPE{TYPE TYPE; PARAM1_TYPE PARAM1_NAME;};\
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments);

#define ZR_CALLBACK_IMPLEMENT_ONE_PARAM(TYPE, PARAM1_NAME) \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments) {\
        struct SZrCallbackImpl_##TYPE *argumentStruct = (struct SZrCallbackImpl_##TYPE *)arguments;\
        argumentStruct->TYPE(state, argumentStruct->PARAM1_NAME);\
    }

#define ZR_CALLBACK_CALL_ONE_PARAM(STATE, TYPE, FUNCTION, PARAM1_NAME, PARAM1_VALUE, OUT_STATUS) \
    {\
        struct SZrCallbackImpl_##TYPE argumentStruct; argumentStruct.TYPE = FUNCTION;\
        argumentStruct.PARAM1_NAME = PARAM1_VALUE;\
        OUT_STATUS = ZrExceptionTryRun(STATE, ZrCallbackImpl_##TYPE, &argumentStruct);\
    }

#define ZR_CALLBACK_DECLARE_TWO_PARAMS(TYPE, PARAM1_TYPE, PARAM1_NAME, PARAM2_TYPE, PARAM2_NAME) \
    typedef void (*TYPE)(struct SZrState *STATE, PARAM1_TYPE PARAM1_NAME, PARAM2_TYPE PARAM2_NAME);\
    struct SZrCallbackImpl_##TYPE{TYPE TYPE; PARAM1_TYPE PARAM1_NAME; PARAM2_TYPE PARAM2_NAME;};\
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments);

#define ZR_CALLBACK_IMPLEMENT_TWO_PARAMS(TYPE, PARAM1_NAME, PARAM2_NAME) \
    void ZrCallbackImpl_##TYPE(struct SZrState *state, TZrPtr arguments) {\
        struct SZrCallbackImpl_##TYPE *argumentStruct = (struct SZrCallbackImpl_##TYPE *)arguments;\
        argumentStruct->TYPE(state, argumentStruct->PARAM1_NAME, argumentStruct->PARAM2_NAME);\
    }

#define ZR_CALLBACK_CALL_TWO_PARAMS(STATE, TYPE, FUNCTION, PARAM1_NAME, PARAM1_VALUE, PARAM2_NAME, PARAM2_VALUE, OUT_STATUS) \
    {\
        struct SZrCallbackImpl_##TYPE argumentStruct; argumentStruct.TYPE = FUNCTION;\
        argumentStruct.PARAM1_NAME = PARAM1_VALUE;\
        argumentStruct.PARAM2_NAME = PARAM2_VALUE;\
        OUT_STATUS = ZrExceptionTryRun(STATE, ZrCallbackImpl_##TYPE, &argumentStruct);\
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
#endif //ZR_VM_CORE_CALLBACK_H
