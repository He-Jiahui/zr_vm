#include "zr_vm_core/bridge.h"

#include "zr_vm_core/execution.h"

/* 生成的 AOT 代码通过 runtime 帧槽进入这里，借用同一套转换和所有权语义；
 * bridge 保留 callInfo 参数形状，不另建一条装箱规则。 */
TZrBool ZrCore_Bridge_BoxTyped(struct SZrState *state,
                               struct SZrCallInfo *callInfo,
                               struct SZrTypeValue *destination,
                               const struct SZrTypeValue *source,
                               const struct SZrTypeValue *typeNameValue) {
    return ZrCore_Execution_ToObject(state, callInfo, destination, source, typeNameValue);
}

/* 与 BoxTyped 对称，失败状态直接透传执行转换入口供 AOT runtime 处理。 */
TZrBool ZrCore_Bridge_UnboxTyped(struct SZrState *state,
                                 struct SZrCallInfo *callInfo,
                                 struct SZrTypeValue *destination,
                                 const struct SZrTypeValue *source,
                                 const struct SZrTypeValue *typeNameValue) {
    return ZrCore_Execution_ToStruct(state, callInfo, destination, source, typeNameValue);
}
