//
// Internal helpers shared by split module translation units.
//

#ifndef ZR_VM_CORE_MODULE_INTERNAL_H
#define ZR_VM_CORE_MODULE_INTERNAL_H

#include "zr_vm_core/module.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_common/zr_runtime_limits_conf.h"
#include "zr_vm_common/zr_string_conf.h"
#include "xxHash/xxhash.h"

/* 与编译产物成员访问标记同宽，供原型可见性恢复。 */
typedef TZrUInt8 EZrAccessModifier;

/* 构造跨阶段暂存状态；prototype 归 GC，members 借用编译 blob，inheritTypeNames 独立持有。 */
typedef struct {
    SZrObjectPrototype *prototype;
    SZrString *typeName;
    EZrObjectPrototypeType prototypeType;
    EZrAccessModifier accessModifier;
    TZrUInt64 protocolMask;
    TZrUInt32 modifierFlags;
    TZrUInt32 nextVirtualSlotIndex;
    TZrUInt32 nextPropertyIdentity;
    TZrUInt32 layoutByteSize;
    TZrUInt32 layoutByteAlign;
    TZrBool hasDecoratorMetadata;
    SZrTypeValue decoratorMetadataValue;
    SZrArray inheritTypeNames;
    const SZrCompiledMemberInfo *members;
    TZrUInt32 membersCount;
    TZrBool needsPostCreateSetup;
} SZrPrototypeCreationInfo;

/* 原型安装后填充接口槽，失败表示目标原型未建立完整分派表。 */
TZrBool zr_module_bind_interface_dispatch(SZrState *state, SZrObjectPrototype *receiver,
                                          SZrObjectPrototype *interfacePrototype);

/* 哈希表对模块名使用字符串类型值作键；只借用 GC 管理的字符串。 */
static inline void zr_module_init_string_key(SZrState *state, SZrTypeValue *key, SZrString *stringValue) {
    ZrCore_Value_InitAsRawObject(state, key, ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue));
    key->type = ZR_VALUE_TYPE_STRING;
}

/* 注册表值使用普通对象类型标签，供缓存读取端再次核验模块内部类型。 */
static inline void zr_module_init_object_value(SZrState *state, SZrTypeValue *value, SZrRawObject *objectValue) {
    ZrCore_Value_InitAsRawObject(state, value, objectValue);
    value->type = ZR_VALUE_TYPE_OBJECT;
}

/* 返回全局缓存对象的借用指针；缺少合法注册表时调用方应视作未命中。 */
static inline SZrObject *zr_module_get_loaded_modules_registry(SZrState *state) {
    SZrGlobalState *global;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    global = state->global;
    if (!ZrCore_Value_IsGarbageCollectable(&global->loadedModulesRegistry) ||
        global->loadedModulesRegistry.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
}

#endif // ZR_VM_CORE_MODULE_INTERNAL_H
