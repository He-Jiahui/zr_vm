#ifndef ZR_VM_LIB_NETWORK_INTERNAL_H
#define ZR_VM_LIB_NETWORK_INTERNAL_H

#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_lib_network/network.h"

/** @brief VM 对象中原生句柄的判别值，决定 value 联合体中有效的成员。 */
typedef enum EZrNetworkVmHandleKind {
    ZR_NETWORK_VM_HANDLE_KIND_TCP_LISTENER = 1,
    ZR_NETWORK_VM_HANDLE_KIND_TCP_STREAM = 2,
    ZR_NETWORK_VM_HANDLE_KIND_UDP_SOCKET = 3
} EZrNetworkVmHandleKind;

/** @brief VM 对象字段保存此指针；kind 决定 value 的有效成员，成员拥有对应系统 socket。
 *  BUG: zr_network_alloc_handle 分配本结构后，成功路径只在 VM close 方法关闭 socket；
 *  当前没有释放本结构的终结路径，即使脚本显式 close 也会泄漏堆内存。 */
typedef struct ZrNetworkVmHandle {
    EZrNetworkVmHandleKind kind;
    union {
        SZrNetworkListener listener;
        SZrNetworkStream stream;
        SZrNetworkUdpSocket udpSocket;
    } value;
} ZrNetworkVmHandle;

/** @brief 获取当前原生方法的对象接收者；非对象接收者返回空指针。 */
SZrObject *zr_network_self_object(const ZrLibCallContext *context);
/** @brief 将网络层错误规范化为 VM 运行时异常，并返回 ZR_FALSE 便于调用方传播。 */
TZrBool zr_network_raise_runtime_error(SZrState *state, const TZrChar *message);
/** @brief 创建带模块类型的 VM 对象；类型暂不可用时回退为普通对象。 */
SZrObject *zr_network_new_typed_object(SZrState *state, const TZrChar *moduleName, const TZrChar *typeName);
/** @brief 把构造完成的对象设为原生函数结果。 */
TZrBool zr_network_finish_object(SZrState *state, SZrTypeValue *result, SZrObject *object);

/** @brief 分配并初始化带判别值的原生句柄，返回堆对象所有权。 */
ZrNetworkVmHandle *zr_network_alloc_handle(EZrNetworkVmHandleKind kind);
/** @brief 将句柄指针存入 VM 对象隐藏字段；当前返回值只表示入参有效。 */
TZrBool zr_network_store_handle(SZrState *state, SZrObject *object, ZrNetworkVmHandle *handle);
/** @brief 从 VM 对象取回指定 kind 的句柄，类型或字段不匹配时返回空指针。 */
ZrNetworkVmHandle *zr_network_get_handle(SZrState *state, SZrObject *object, EZrNetworkVmHandleKind expectedKind);

/** @brief 从脚本主机字符串与端口整数构造地址值，并校验长度与端口范围。 */
TZrBool zr_network_read_endpoint_args(const ZrLibCallContext *context,
                                      TZrSize hostIndex,
                                      TZrSize portIndex,
                                      SZrNetworkEndpoint *outEndpoint);
/** @brief 从脚本整数读取毫秒超时；无该实参时使用 defaultValue。 */
TZrBool zr_network_read_timeout_arg(const ZrLibCallContext *context,
                                    TZrSize index,
                                    TZrUInt32 defaultValue,
                                    TZrUInt32 *outTimeoutMs);
/** @brief 从脚本整数读取正的最大接收字节数。 */
TZrBool zr_network_read_byte_count_arg(const ZrLibCallContext *context,
                                       TZrSize index,
                                       TZrSize *outLength);

#endif
