//
// zr.system.env callbacks.
//

#include "zr_vm_lib_system/env.h"

#include "zr_vm_core/string.h"

#include <stdlib.h>

TZrBool ZrSystem_Env_GetVariable(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrString *nameString = ZR_NULL;
    const TZrChar *value;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    /* 注册描述符的 1..1 元数先由 dispatcher 检查；ReadString 在正常入口检查类型并对非字符串 noreturn。 */
    if (!ZrLib_CallContext_ReadString(context, 0, &nameString) || nameString == ZR_NULL) {
        return ZR_FALSE;
    }

    /* getenv 读取调用时的宿主环境；借用指针只在本次同步复制期间有效。本 callback 不锁定环境，调用方须遵循目标 CRT 对并发环境修改的同步约定。 */
    value = getenv(ZrCore_String_GetNativeString(nameString));
    if (value == ZR_NULL) {
        ZrLib_Value_SetNull(result);
    } else {
        /* BUG: 短串驻留表扩容 OOM 可使 SetString 留下派发预置的 null，而 callback 仍报成功，环境值被误报为缺失。 */
        /* TODO: value 在 SetString 的字符串分配期间仍被借用。RawObject_New 的 GcMalloc 首次分配失败时，
         * 全局已初始化且非即时 GC 可触发完整回收；重写函数槽时可能首次读取 ZR_VM_TRACE_GC_REWRITE_SLOTS，
         * 再次调用 getenv（gc_cycle.c:15-24,262-267,1710-1718）。StringTable_Init 预热的是另一开关
         * ZR_VM_TRACE_CORE_BOOTSTRAP（string.c:683,927-935），不能证明 GC 开关已缓存。核受支持 CRT 后续
         * getenv 对旧指针的有效期，并用受控 GC 验证该复制窗口；目前不据此标记行为缺陷。 */
        ZrLib_Value_SetString(context->state, result, value);
    }
    return ZR_TRUE;
}
