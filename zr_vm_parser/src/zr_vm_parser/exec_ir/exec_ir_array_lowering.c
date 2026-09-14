#include "zr_vm_parser/exec_ir_array_lowering.h"

TZrBool ZrParser_ExecIr_LowerArrayIndex(const SZrContiguousView *view,
                                        TZrInt64 index, TZrSize *offset,
                                        SZrViewDiagnostic *diagnostic) {
    return ZrCore_View_IndexOffset(view, index, offset, diagnostic);
}
