#include "execution_context.h"

const TZrChar *ZrCore_Execution_BoundaryStatusName(EZrExecutionBoundaryStatus status) {
    switch (status) {
        case ZR_EXECUTION_BOUNDARY_OK: return "ok";
        case ZR_EXECUTION_BOUNDARY_INVALID_ARGUMENT: return "invalid-argument";
        case ZR_EXECUTION_BOUNDARY_INVALID_FRAME: return "invalid-frame";
        case ZR_EXECUTION_BOUNDARY_INVALID_FUNCTION: return "invalid-function";
        case ZR_EXECUTION_BOUNDARY_INVALID_PROGRAM_COUNTER: return "invalid-program-counter";
        case ZR_EXECUTION_BOUNDARY_INVALID_STACK: return "invalid-stack";
        case ZR_EXECUTION_BOUNDARY_TERMINATED: return "terminated";
        default: return "unknown";
    }
}
