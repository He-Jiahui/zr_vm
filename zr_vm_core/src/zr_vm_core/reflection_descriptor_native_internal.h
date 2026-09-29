/**
 * @file
 * @brief 声明反射 descriptor 的 native 查询与构造方法安装入口。
 */
#ifndef ZR_VM_REFLECTION_DESCRIPTOR_NATIVE_INTERNAL_H
#define ZR_VM_REFLECTION_DESCRIPTOR_NATIVE_INTERNAL_H

#include "zr_vm_core/reflection.h"

struct SZrObject;
struct SZrState;

/**
 * @brief 为 descriptor 挂载共同查询方法，并按类型类别决定是否暴露 createInstance。
 * @pre descriptor 在后续 closure 与字段分配期间须保持 GC 可达；category 应与 descriptor 身份一致。
 * @return 全部适用方法安装成功时返回 true；安装失败返回 false。
 * TODO: 逐项安装失败会留下已挂方法；reflection.c 的现有 descriptor 复用路径先绑定再安装，需核查失败后的可见性与重试约定。
 */
TZrBool ZrCore_Reflection_AttachDescriptorNativeMethodsInternal(
        struct SZrState *state,
        struct SZrObject *descriptor,
        EZrReflectionTypeCategory category);

#endif
