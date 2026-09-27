#ifndef ZR_VM_CORE_CALL_BINDING_SITE_H
#define ZR_VM_CORE_CALL_BINDING_SITE_H

#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"

/* 链接时核对指令族、操作及缓存下标，防止持久化行被另一条指令消费。 */
TZrBool zr_call_binding_site_matches(const SZrFunction *function, TZrUInt32 cacheIndex);
/* owner 原型和导入模块共用的描述符身份校验，不按名称猜测 getter/setter 或虚槽。 */
TZrBool zr_call_binding_descriptor_matches(const SZrFunction *function,
        const SZrFunctionCallSiteCacheEntry *entry, const SZrMemberDescriptor *descriptor);

#endif
