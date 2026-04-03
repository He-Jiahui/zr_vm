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

#define ZR_NATIVE_MODULE_INFO_EXPORT_NAME "__zr_native_module_info"

struct ZR_STRUCT_ALIGN SZrObjectModule {
    SZrObject super; // 继承自 SZrObject，pub 变量存储在 super.nodeMap

    // 模块信息
    SZrString *moduleName; // 模块名
    TZrUInt64 pathHash; // xxhash 生成的路径哈希
    SZrString *fullPath; // 完整路径

    // 导出内容
    // pub 变量存储在 super.nodeMap 中
    // pro 变量存储在单独的 proNodeMap 中（包含所有 pub）
    SZrHashSet proNodeMap; // protected 变量（包含所有 pub）
};

typedef struct SZrObjectModule SZrObjectModule;

// 创建模块对象
ZR_CORE_API struct SZrObjectModule *ZrCore_Module_Create(struct SZrState *state);

// 设置模块信息
ZR_CORE_API void ZrCore_Module_SetInfo(struct SZrState *state, struct SZrObjectModule *module, SZrString *moduleName,
                                 TZrUInt64 pathHash, SZrString *fullPath);

// 添加 pub 导出（同时添加到 pub 和 pro）
ZR_CORE_API void ZrCore_Module_AddPubExport(struct SZrState *state, struct SZrObjectModule *module, SZrString *name,
                                      const SZrTypeValue *value);

// 添加 pro 导出（仅添加到 pro）
ZR_CORE_API void ZrCore_Module_AddProExport(struct SZrState *state, struct SZrObjectModule *module, SZrString *name,
                                      const SZrTypeValue *value);

// 获取 pub 导出（跨模块访问）
ZR_CORE_API const SZrTypeValue *ZrCore_Module_GetPubExport(struct SZrState *state, struct SZrObjectModule *module,
                                                     SZrString *name);

// 获取 pro 导出（同模块库访问）
ZR_CORE_API const SZrTypeValue *ZrCore_Module_GetProExport(struct SZrState *state, struct SZrObjectModule *module,
                                                     SZrString *name);

// 计算路径哈希（使用 xxhash）
ZR_CORE_API TZrUInt64 ZrCore_Module_CalculatePathHash(struct SZrState *state, struct SZrString *fullPath);

// 模块缓存操作
ZR_CORE_API struct SZrObjectModule *ZrCore_Module_GetFromCache(struct SZrState *state, struct SZrString *path);
ZR_CORE_API void ZrCore_Module_AddToCache(struct SZrState *state, struct SZrString *path, struct SZrObjectModule *module);

// 内部模块导入 helper
ZR_CORE_API struct SZrObjectModule *ZrCore_Module_ImportByPath(struct SZrState *state, struct SZrString *path);
ZR_CORE_API TZrInt64 ZrCore_Module_ImportNativeEntry(struct SZrState *state);

// 创建并注册 prototype 的 native 函数
// 参数: (module, typeName, prototypeType, accessModifier, inherits..., members...)
// 返回: prototype 对象
ZR_CORE_API TZrInt64 ZrCore_PrototypeNativeFunction_Create(struct SZrState *state);

// 从编译后的函数的prototypeData中解析prototype信息并创建prototype实例
// 将创建的prototype注册到模块的导出中
// 参数: state - VM状态, module - 目标模块, entryFunction - 编译后的入口函数(__entry)
// 返回: 成功创建的prototype数量
ZR_CORE_API TZrSize ZrCore_Module_CreatePrototypesFromData(struct SZrState *state, 
                                                      struct SZrObjectModule *module,
                                                      struct SZrFunction *entryFunction);

// 向后兼容：保留旧函数名，内部调用新函数
ZR_CORE_API TZrSize ZrCore_Module_CreatePrototypesFromConstants(struct SZrState *state, 
                                                          struct SZrObjectModule *module,
                                                          struct SZrFunction *entryFunction);

#endif // ZR_VM_CORE_MODULE_H
