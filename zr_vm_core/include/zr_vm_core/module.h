//
// Created by HeJiahui on 2025/8/6.
//

#ifndef ZR_VM_CORE_MODULE_H
#define ZR_VM_CORE_MODULE_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"

struct SZrState;
struct SZrGlobalState;

struct ZR_STRUCT_ALIGN SZrObjectModule {
    SZrObject super; // 继承自 SZrObject，pub 变量存储在 super.nodeMap

    // 模块信息
    SZrString *moduleName; // 模块名
    TUInt64 pathHash; // xxhash 生成的路径哈希
    SZrString *fullPath; // 完整路径

    // 导出内容
    // pub 变量存储在 super.nodeMap 中
    // pro 变量存储在单独的 proNodeMap 中（包含所有 pub）
    SZrHashSet proNodeMap; // protected 变量（包含所有 pub）
};

typedef struct SZrObjectModule SZrObjectModule;

// 创建模块对象
ZR_CORE_API struct SZrObjectModule *ZrModuleCreate(struct SZrState *state);

// 设置模块信息
ZR_CORE_API void ZrModuleSetInfo(struct SZrState *state, struct SZrObjectModule *module, SZrString *moduleName,
                                 TUInt64 pathHash, SZrString *fullPath);

// 添加 pub 导出（同时添加到 pub 和 pro）
ZR_CORE_API void ZrModuleAddPubExport(struct SZrState *state, struct SZrObjectModule *module, SZrString *name,
                                      const SZrTypeValue *value);

// 添加 pro 导出（仅添加到 pro）
ZR_CORE_API void ZrModuleAddProExport(struct SZrState *state, struct SZrObjectModule *module, SZrString *name,
                                      const SZrTypeValue *value);

// 获取 pub 导出（跨模块访问）
ZR_CORE_API const SZrTypeValue *ZrModuleGetPubExport(struct SZrState *state, struct SZrObjectModule *module,
                                                     SZrString *name);

// 获取 pro 导出（同模块库访问）
ZR_CORE_API const SZrTypeValue *ZrModuleGetProExport(struct SZrState *state, struct SZrObjectModule *module,
                                                     SZrString *name);

// 计算路径哈希（使用 xxhash）
ZR_CORE_API TUInt64 ZrModuleCalculatePathHash(struct SZrState *state, struct SZrString *fullPath);

// 模块缓存操作
ZR_CORE_API struct SZrObjectModule *ZrModuleGetFromCache(struct SZrState *state, struct SZrString *path);
ZR_CORE_API void ZrModuleAddToCache(struct SZrState *state, struct SZrString *path, struct SZrObjectModule *module);

// 从源文件创建模块（旧接口，保持向后兼容）
ZR_CORE_API SZrObject *ZrModuleCreateFromSource(struct SZrState *state, SZrIoSource *source);

// zr.import native 函数
ZR_CORE_API TInt64 ZrImportNativeFunction(struct SZrState *state);

// 创建并注册 prototype 的 native 函数
// 参数: (module, typeName, prototypeType, accessModifier, inherits..., members...)
// 返回: prototype 对象
ZR_CORE_API TInt64 ZrCreatePrototypeNativeFunction(struct SZrState *state);

#endif // ZR_VM_CORE_MODULE_H
