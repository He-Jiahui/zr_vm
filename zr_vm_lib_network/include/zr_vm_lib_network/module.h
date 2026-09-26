#ifndef ZR_VM_LIB_NETWORK_MODULE_H
#define ZR_VM_LIB_NETWORK_MODULE_H

#include "zr_vm_lib_network/conf.h"
#include "zr_vm_library/native_binding.h"

/** @brief 返回 zr.network 根模块的静态描述符，供原生插件入口和注册器使用。
 *  @return 借用静态存储；调用方不释放。
 *  @note 根模块通过链接暴露 tcp/udp，单独安装根描述符时须先使叶模块可解析。
 */
ZR_NETWORK_API const ZrLibModuleDescriptor *ZrVmLibNetwork_GetModuleDescriptor(void);
/** @brief 向全局状态按 TCP、UDP、根模块顺序注册网络提供者。
 *  @pre global 非空，且状态可附加原生注册器。
 *  @return 任一步失败返回 false；此前成功注册的叶模块不会回滚。
 */
ZR_NETWORK_API TZrBool ZrVmLibNetwork_Register(SZrGlobalState *global);

#endif
