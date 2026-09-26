#ifndef ZR_VM_LIB_NETWORK_UDP_REGISTRY_H
#define ZR_VM_LIB_NETWORK_UDP_REGISTRY_H

#include "zr_vm_lib_network/conf.h"
#include "zr_vm_library/native_binding.h"

/**
 * @brief 取得供网络原生注册器挂载的 UDP 叶子模块描述符。
 * @return 静态只读描述符的借用指针，调用方不得释放。
 * @note 描述符中的回调和类型表由脚本导入 zr.network.udp 时使用。
 */
ZR_NETWORK_API const ZrLibModuleDescriptor *ZrNetwork_UdpRegistry_GetModule(void);

#endif
