#include "backend_aot_llvm_module_prelude.h"

#include <string.h>

static const TZrChar *backend_aot_llvm_runtime_contract_name(TZrUInt32 contractBit) {
    return backend_aot_exec_ir_runtime_contract_name(contractBit);
}

static TZrUInt32 backend_aot_llvm_runtime_contract_count(TZrUInt32 runtimeContracts) {
    return backend_aot_exec_ir_runtime_contract_count(runtimeContracts);
}

void backend_aot_write_llvm_contracts(FILE *file, TZrUInt32 runtimeContracts) {
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_REFLECTION_TYPEOF) {
        fprintf(file, "declare i1 @ZrCore_Reflection_TypeOfValue(ptr, ptr, ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_FUNCTION_PRECALL) {
        fprintf(file, "declare ptr @ZrCore_Function_PreCall(ptr, ptr, i64, ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_BORROW) {
        fprintf(file, "declare i1 @ZrCore_Ownership_BorrowValue(ptr, ptr, ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_LOAN) {
        fprintf(file, "declare i1 @ZrCore_Ownership_LoanValue(ptr, ptr, ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_SHARE) {
        fprintf(file, "declare i1 @ZrCore_Ownership_NativeShared(ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_WEAK) {
        fprintf(file, "declare i1 @ZrCore_Ownership_NativeWeak(ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_DETACH) {
        fprintf(file, "declare i1 @ZrCore_Ownership_DetachValue(ptr, ptr, ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_UPGRADE) {
        fprintf(file, "declare i1 @ZrCore_Ownership_UpgradeValue(ptr, ptr, ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE) {
        fprintf(file, "declare void @ZrCore_Ownership_ReleaseValue(ptr, ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_ITER_INIT) {
        fprintf(file, "declare i1 @ZrCore_Object_IterInit(ptr, ptr, ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_ITER_MOVE_NEXT) {
        fprintf(file, "declare i1 @ZrCore_Object_IterMoveNext(ptr, ptr, ptr)\n");
    }
}

void backend_aot_write_runtime_contract_array_llvm(FILE *file, TZrUInt32 runtimeContracts) {
    TZrUInt32 contractBit;

    fprintf(file, "; runtimeContracts:");
    for (contractBit = 1; contractBit <= ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE; contractBit <<= 1) {
        if ((runtimeContracts & contractBit) == 0) {
            continue;
        }
        fprintf(file, " %s", backend_aot_llvm_runtime_contract_name(contractBit));
    }
    fprintf(file, "\n");
}

void backend_aot_write_runtime_contract_globals_llvm(FILE *file, TZrUInt32 runtimeContracts) {
    TZrUInt32 contractBit;
    TZrUInt32 contractCount;
    TZrUInt32 contractIndex = 0;

    if (file == ZR_NULL) {
        return;
    }

    contractCount = backend_aot_llvm_runtime_contract_count(runtimeContracts);
    for (contractBit = 1; contractBit <= ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE; contractBit <<= 1) {
        const TZrChar *contractName;

        if ((runtimeContracts & contractBit) == 0) {
            continue;
        }

        contractName = backend_aot_llvm_runtime_contract_name(contractBit);
        fprintf(file,
                "@zr_aot_runtime_contract_%u = private unnamed_addr constant [%llu x i8] c\"%s\\00\"\n",
                (unsigned)contractIndex,
                (unsigned long long)(strlen(contractName) + 1),
                contractName);
        contractIndex++;
    }

    fprintf(file, "@zr_aot_runtime_contracts = private constant [%u x ptr] [", (unsigned)(contractCount + 1));
    contractIndex = 0;
    for (contractBit = 1; contractBit <= ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE; contractBit <<= 1) {
        if ((runtimeContracts & contractBit) == 0) {
            continue;
        }

        if (contractIndex > 0) {
            fprintf(file, ", ");
        }
        fprintf(file, "ptr @zr_aot_runtime_contract_%u", (unsigned)contractIndex);
        contractIndex++;
    }
    if (contractIndex > 0) {
        fprintf(file, ", ");
    }
    fprintf(file, "ptr null]\n");
}

void backend_aot_write_embedded_blob_llvm(FILE *file, const TZrByte *blob, TZrSize blobLength) {
    TZrSize index;

    if (file == ZR_NULL || blob == ZR_NULL || blobLength == 0) {
        return;
    }

    fprintf(file, "@zr_aot_embedded_module_blob = private constant [%llu x i8] [\n",
            (unsigned long long)blobLength);
    for (index = 0; index < blobLength; index++) {
        fprintf(file, "  i8 %u", (unsigned)blob[index]);
        if (index + 1 < blobLength) {
            fprintf(file, ",");
        }
        if ((index % 16) == 15 || index + 1 == blobLength) {
            fprintf(file, "\n");
        } else {
            fprintf(file, " ");
        }
    }
    fprintf(file, "]\n");
}

void backend_aot_write_llvm_runtime_helper_decls(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_BeginGeneratedFunction(ptr, i32, ptr)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_CopyConstant(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_CreateClosure(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_GetClosureValue(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_SetClosureValue(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_CopyStack(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_GetGlobal(ptr, ptr, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_CreateObject(ptr, ptr, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_CreateArray(ptr, ptr, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_TypeOf(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_ToObject(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_ToStruct(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_MetaGet(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_MetaSet(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_MetaGetCached(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_MetaSetCached(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_MetaGetStaticCached(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_MetaSetStaticCached(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_OwnUnique(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_OwnBorrow(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_OwnLoan(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_OwnShare(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_OwnWeak(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_OwnDetach(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_OwnUpgrade(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_OwnRelease(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_LogicalEqual(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_LogicalNotEqual(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_LogicalGreaterSigned(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_LogicalLessSigned(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_LogicalGreaterEqualSigned(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_LogicalLessEqualSigned(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_IsTruthy(ptr, ptr, i32, ptr)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_Add(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_Sub(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_Mul(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_AddInt(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_SubInt(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_BitwiseXor(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_MulSigned(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_Div(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_DivSigned(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_Mod(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_Neg(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_ToString(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_GetMember(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_SetMember(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_GetByIndex(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_SetByIndex(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_SuperArrayGetInt(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_SuperArraySetInt(ptr, ptr, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_IterInit(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_IterMoveNext(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_IterCurrent(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_PrepareDirectCall(ptr, ptr, i32, i32, i32, ptr)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_PrepareMetaCall(ptr, ptr, i32, i32, i32, ptr)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_PrepareStaticDirectCall(ptr, ptr, i32, i32, i32, i32, ptr)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric(ptr, ptr, ptr, i32, i32, i32, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_FinishDirectCall(ptr, ptr, ptr, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_Try(ptr, ptr, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_EndTry(ptr, ptr, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_Throw(ptr, ptr, i32, ptr)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_Catch(ptr, ptr, i32)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_EndFinally(ptr, ptr, i32, ptr)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_SetPendingReturn(ptr, ptr, i32, i32, ptr)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_SetPendingBreak(ptr, ptr, i32, ptr)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_SetPendingContinue(ptr, ptr, i32, ptr)\n");
    fprintf(file, "declare i1 @ZrLibrary_AotRuntime_ToInt(ptr, ptr, i32, i32)\n");
    fprintf(file, "declare i64 @ZrLibrary_AotRuntime_Return(ptr, ptr, i32, i1)\n");
    fprintf(file, "declare i64 @ZrLibrary_AotRuntime_FailGeneratedFunction(ptr, ptr)\n");
    fprintf(file, "declare i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr, i32, i32, i32)\n");
}

void backend_aot_llvm_write_module_prelude(FILE *file,
                                           const SZrAotExecIrModule *module,
                                           const TZrChar *moduleName,
                                           const TZrChar *inputHash,
                                           const SZrAotWriterOptions *options) {
    if (file == ZR_NULL || module == ZR_NULL || moduleName == ZR_NULL || inputHash == ZR_NULL) {
        return;
    }

    backend_aot_write_llvm_contracts(file, module->runtimeContracts);
    backend_aot_write_runtime_contract_array_llvm(file, module->runtimeContracts);
    fprintf(file, "\n");
    backend_aot_write_instruction_listing(file, "; ", module);
    fprintf(file, "@zr_aot_module_name = private unnamed_addr constant [%llu x i8] c\"%s\\00\"\n",
            (unsigned long long)(strlen(moduleName) + 1),
            moduleName);
    fprintf(file, "@zr_aot_input_hash = private unnamed_addr constant [%llu x i8] c\"%s\\00\"\n",
            (unsigned long long)(strlen(inputHash) + 1),
            inputHash);
    backend_aot_write_runtime_contract_globals_llvm(file, module->runtimeContracts);
    backend_aot_write_embedded_blob_llvm(file,
                                         options != ZR_NULL ? options->embeddedModuleBlob : ZR_NULL,
                                         options != ZR_NULL ? options->embeddedModuleBlobLength : 0);
    fprintf(file, "%%ZrAotGeneratedFrame = type { ptr, ptr, ptr, ptr, i32, i32, i32, i32, i32, i1 }\n");
    fprintf(file, "%%ZrAotGeneratedDirectCall = type { ptr, ptr, ptr, i32, i32, i32, i32, i32, i1, i1 }\n");
    fprintf(file, "%%ZrAotCompiledModule = type { i32, i32, ptr, i32, ptr, ptr, ptr, i64, ptr, i32, ptr }\n");
    fprintf(file, "\n");
}
