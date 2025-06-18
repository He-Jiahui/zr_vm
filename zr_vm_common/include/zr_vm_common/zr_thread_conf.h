//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_THREAD_CONF_H
#define ZR_THREAD_CONF_H

enum EZrThreadStatus {
    ZR_THREAD_STATUS_FINE = 0,
    ZR_THREAD_STATUS_YIELD = 1,
    ZR_THREAD_STATUS_RUNTIME_ERROR = 2,
    ZR_THREAD_STATUS_MEMORY_ERROR = 3,
    ZR_THREAD_STATUS_EXCEPTION_ERROR = 4,
};

typedef enum EZrThreadStatus EZrThreadStatus;
#endif //ZR_THREAD_CONF_H
