#include "zr_vm_parser/exec_ir_host_aot_target.h"

#include <string.h>

/* RED 阶段保留真实可链接入口；完整 producer 等待实际 RED 验证与提交。 */
TZrBool ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(
        const SZrSemanticContext *context, TZrTypeId callableTypeId,
        const SZrExecIrLayout *returnLayout, SZrAotIrTargetContract *output,
        SZrAotIrDiagnostic *diagnostic) {
    (void)context;
    (void)callableTypeId;
    (void)returnLayout;
    (void)output;
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = ZR_AOT_IR_UNSUPPORTED;
    }
    return ZR_FALSE;
}
