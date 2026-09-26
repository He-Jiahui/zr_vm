//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_THREAD_CONF_H
#define ZR_THREAD_CONF_H

/* VM 线程栈起始容量；增长逻辑按最小值和额外预留为调用帧留空间。 */
#define ZR_THREAD_STACK_SIZE_MIN 32
#define ZR_THREAD_STACK_SIZE_BASIC (ZR_THREAD_STACK_SIZE_MIN * 2)
#define ZR_THREAD_STACK_SIZE_EXTRA 5

#if !defined(ZR_THREAD_LOCK)
/* core/parser 在调用 native、I/O、调试钩子及栈操作等可重入边界调用这对钩子；
 * 默认是空操作，宿主若覆盖须成对定义并核对这些边界的锁顺序。 */
// default option
// user can define ZR_THREAD_LOCK and ZR_THREAD_UNLOCK to use their own lock
#define ZR_THREAD_LOCK(state) ((void) 0)
#define ZR_THREAD_UNLOCK(state) ((void) 0)
#endif

/** @brief VM 执行结果在正常、让出、错误与终止之间的跨模块状态编号。 */
enum EZrThreadStatus {
    ZR_THREAD_STATUS_INVALID = -1,
    ZR_THREAD_STATUS_FINE = 0,
    ZR_THREAD_STATUS_YIELD = 1,
    ZR_THREAD_STATUS_RUNTIME_ERROR = 2,
    ZR_THREAD_STATUS_MEMORY_ERROR = 3,
    ZR_THREAD_STATUS_EXCEPTION_ERROR = 4,
    ZR_THREAD_STATUS_EXECUTION_TERMINATED = 5,

    ZR_THREAD_STATUS_ENUM_MAX
};

typedef enum EZrThreadStatus EZrThreadStatus;
#endif // ZR_THREAD_CONF_H
