#ifndef ZR_VM_CORE_HOTPATCH_H
#define ZR_VM_CORE_HOTPATCH_H

/* 兼容的 manifest 入口；发布、代际和受限 profile 接口由各自头文件暴露。
 * TODO: 核查外部 host 是否把本头视为完整热更新门面；当前总头 zr_vm_core.h
 * 单独包含各阶段接口，而本头仅转出 manifest 验证相关类型与接口。 */
#include "zr_vm_core/capability_manifest.h"

#endif
