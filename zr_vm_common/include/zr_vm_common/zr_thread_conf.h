//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_THREAD_CONF_H
#define ZR_THREAD_CONF_H

#define ZR_THREAD_STACK_SIZE_MIN 32
#define ZR_THREAD_STACK_SIZE_BASIC (ZR_THREAD_STACK_SIZE_MIN * 2)
#define ZR_THREAD_STACK_SIZE_EXTRA 5

#if !defined(ZR_THREAD_LOCK)
// default option
// user can define ZR_THREAD_LOCK and ZR_THREAD_UNLOCK to use their own lock
#define ZR_THREAD_LOCK(state) ((void)0)
#define ZR_THREAD_UNLOCK(state) ((void)0)
#endif

enum EZrThreadStatus {
    ZR_THREAD_STATUS_FINE = 0,
    ZR_THREAD_STATUS_YIELD = 1,
    ZR_THREAD_STATUS_RUNTIME_ERROR = 2,
    ZR_THREAD_STATUS_MEMORY_ERROR = 3,
    ZR_THREAD_STATUS_EXCEPTION_ERROR = 4,
};

typedef enum EZrThreadStatus EZrThreadStatus;
#endif //ZR_THREAD_CONF_H
