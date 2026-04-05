#include "backend_aot_llvm_text_call_result.h"

void backend_aot_llvm_write_guarded_call_text(FILE *file,
                                              TZrUInt32 *tempCounter,
                                              const TZrChar *calleeName,
                                              const TZrChar *argsText,
                                              const TZrChar *successLabel,
                                              const TZrChar *failLabel) {
    TZrUInt32 tempValue;

    if (file == ZR_NULL || calleeName == ZR_NULL || argsText == ZR_NULL || successLabel == ZR_NULL ||
        failLabel == ZR_NULL) {
        return;
    }

    tempValue = backend_aot_llvm_next_temp(tempCounter);
    fprintf(file, "  %%t%u = call i1 @%s(%s)\n", (unsigned)tempValue, calleeName, argsText);
    fprintf(file,
            "  br i1 %%t%u, label %%%s, label %%%s\n",
            (unsigned)tempValue,
            successLabel,
            failLabel);
}

void backend_aot_llvm_write_nonzero_call_text(FILE *file,
                                              TZrUInt32 *tempCounter,
                                              const TZrChar *calleeName,
                                              const TZrChar *argsText,
                                              const TZrChar *successLabel,
                                              const TZrChar *failLabel) {
    TZrUInt32 resultTemp;
    TZrUInt32 okTemp;

    if (file == ZR_NULL || calleeName == ZR_NULL || argsText == ZR_NULL || successLabel == ZR_NULL ||
        failLabel == ZR_NULL) {
        return;
    }

    resultTemp = backend_aot_llvm_next_temp(tempCounter);
    okTemp = backend_aot_llvm_next_temp(tempCounter);
    fprintf(file, "  %%t%u = call i64 @%s(%s)\n", (unsigned)resultTemp, calleeName, argsText);
    fprintf(file,
            "  %%t%u = icmp ne i64 %%t%u, 0\n",
            (unsigned)okTemp,
            (unsigned)resultTemp);
    fprintf(file,
            "  br i1 %%t%u, label %%%s, label %%%s\n",
            (unsigned)okTemp,
            successLabel,
            failLabel);
}
