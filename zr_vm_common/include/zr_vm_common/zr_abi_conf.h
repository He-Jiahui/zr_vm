//
// 编译端、原生模块描述符和运行时加载器共用这些版本门槛。
//

#ifndef ZR_ABI_CONF_H
#define ZR_ABI_CONF_H

#include "zr_vm_common/zr_common_conf.h"

/** @brief 原生模块要求的最小宿主 ABI；加载器拒绝比当前运行时更新的要求。 */
#define ZR_VM_NATIVE_RUNTIME_ABI_VERSION 4U
/** @brief 原生插件描述符布局版本；注册入口要求精确匹配。 */
#define ZR_VM_NATIVE_PLUGIN_ABI_VERSION 6U
/** @brief 脚本可见的原生模块信息对象版本。 */
#define ZR_NATIVE_MODULE_INFO_VERSION 1U
/** @brief 预留的 CLI 清单格式版本。TODO: 当前只有常量审计脚本读取它，需核查是否接入清单读写链。 */
#define ZR_CLI_MANIFEST_VERSION 1U

#endif // ZR_ABI_CONF_H
