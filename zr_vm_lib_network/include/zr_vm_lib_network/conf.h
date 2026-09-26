#ifndef ZR_VM_LIB_NETWORK_CONF_H
#define ZR_VM_LIB_NETWORK_CONF_H

#include "zr_vm_common.h"

#define ZR_NETWORK_API ZR_API

/* 公开端点 host 与 CLI 调试端点展示共用的文本容量；调用者须预留终止符。 */
#define ZR_NETWORK_ENDPOINT_TEXT_CAPACITY 96U
/* 调试代理用于读取长度前缀网络帧的固定缓冲区容量。 */
#define ZR_NETWORK_FRAME_BUFFER_CAPACITY 8192U
/* API 的超时哨兵；调用端与底层等待函数均将其解释为无限等待。 */
#define ZR_NETWORK_WAIT_INFINITE ((TZrUInt32) 0xFFFFFFFFu)

#endif
