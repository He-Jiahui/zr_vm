#ifndef ZR_VM_LIBRARY_AOT_TYPED_CALL_BINDING_H
#define ZR_VM_LIBRARY_AOT_TYPED_CALL_BINDING_H

#include "zr_vm_library/aot_runtime.h"

/* 生成调用使用当前指令索引复用 core 已链接的 call binding；失败诊断留在 state。 */
TZrBool aot_prepare_call_binding(SZrState *state, ZrAotGeneratedFrame *frame,
                                SZrTypeValue *callable);
/* META 绑定只在函数的指令映射存在时改写 callable；无绑定由调用方走动态 @call。 */
TZrBool aot_prepare_meta_binding(SZrState *state, ZrAotGeneratedFrame *frame,
        const SZrTypeValue *receiver, SZrTypeValue *callable, TZrBool *hasBinding);

#endif
