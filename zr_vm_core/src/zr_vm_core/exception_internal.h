#ifndef ZR_VM_CORE_EXCEPTION_INTERNAL_H
#define ZR_VM_CORE_EXCEPTION_INTERNAL_H

#include "zr_vm_core/exception.h"
#include <setjmp.h>

#if defined(__cplusplus) && !defined(ZR_EXCEPTION_WITH_LONG_JUMP)
/* C++构建抛context指针；本地状态在调用宏前已写入，析构重入不属C11 scopes保证。 */
#define ZR_EXCEPTION_NATIVE_THROW(state, context) throw(context)
/* catch-all截获本保护块的所有C++异常；外来异常没有写status时返回INVALID，而非FINE。 */
#define ZR_EXCEPTION_NATIVE_TRY(state, context, block) \
    try { block } catch (...) { \
        if ((context)->status == ZR_THREAD_STATUS_FINE) \
            (context)->status = ZR_THREAD_STATUS_INVALID; \
    }
#else
/* 跳回当前恢复点的setjmp；不能用于不同原生线程或已返回的保护块。 */
#define ZR_EXCEPTION_NATIVE_THROW(state, context) longjmp((context)->jumpBuffer, 1)
/* 只在setjmp首次返回零时运行回调；longjmp返回非零后继续撤销恢复点，不重跑block。 */
#define ZR_EXCEPTION_NATIVE_TRY(state, context, block) \
    if (setjmp((context)->jumpBuffer) == 0) { block }
#endif

struct SZrState;
/* 只接收由当前活动 TryRun 建立的首成员恢复点；先还原 AOT 根链再让 GC scopes 发布 mutator 状态。 */
void ZrCore_Exception_RestoreLocalTryRunScopes(struct SZrState *state);

#endif
