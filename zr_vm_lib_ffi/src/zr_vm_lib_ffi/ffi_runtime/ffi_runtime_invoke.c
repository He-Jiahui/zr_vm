#include "ffi_runtime_internal.h"

#if ZR_VM_HAS_LIBFFI
/** @brief 暂存原生调用错误，供统一清理完成后再转为 VM 异常。
 * @note 清理前直接抛异常可能绕过参数缓冲区、GC pin 与回调状态的归还。
 */
static void zr_ffi_record_pending_error(
        ZrFfiErrorCode *outCode,
        char *message,
        TZrSize messageSize,
        ZrFfiErrorCode code,
        const char *format,
        ...) {
    va_list arguments;

    if (outCode == ZR_NULL || message == ZR_NULL || messageSize == 0 ||
        format == ZR_NULL) {
        return;
    }

    *outCode = code;
    va_start(arguments, format);
    vsnprintf(message, messageSize, format, arguments);
    va_end(arguments);
}
/** @brief 按已声明的整数或布尔返回类型解释 canonical return_code 策略。
 * @note 该策略把非零视为原生失败；浮点、聚合及 void 不参与此判定。
 */
static TZrBool zr_ffi_return_code_is_failure(
        const ZrFfiTypeLayout *type,
        const void *storage,
        TZrInt64 *outCode) {
    TZrInt64 code = 0;

    if (type == ZR_NULL || storage == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (type->kind) {
        case ZR_FFI_TYPE_BOOL:
        case ZR_FFI_TYPE_U8:
            code = (TZrInt64)*(const TZrUInt8 *)storage;
            break;
        case ZR_FFI_TYPE_I8:
            code = (TZrInt64)*(const TZrInt8 *)storage;
            break;
        case ZR_FFI_TYPE_U16:
            code = (TZrInt64)*(const TZrUInt16 *)storage;
            break;
        case ZR_FFI_TYPE_I16:
            code = (TZrInt64)*(const TZrInt16 *)storage;
            break;
        case ZR_FFI_TYPE_U32:
            code = (TZrInt64)*(const TZrUInt32 *)storage;
            break;
        case ZR_FFI_TYPE_I32:
            code = (TZrInt64)*(const TZrInt32 *)storage;
            break;
        case ZR_FFI_TYPE_U64:
            code = (TZrInt64)*(const TZrUInt64 *)storage;
            break;
        case ZR_FFI_TYPE_I64:
            code = *(const TZrInt64 *)storage;
            break;
        case ZR_FFI_TYPE_ENUM:
            return zr_ffi_return_code_is_failure(
                    type->as.enumType.underlying, storage, outCode);
        default:
            return ZR_FALSE;
    }
    if (outCode != ZR_NULL) {
        *outCode = code;
    }
    return (TZrBool)(code != 0);
}

#endif

/** @brief SymbolHandle.call 与直接元调用共用的 libffi 调用边界。
 * @note 参数类型与返回布局来自已解析签名；writebackContext 只在直接元调用中提供，
 *       用于 ref/out 参数的属性引用或调用参数写回。
 */
TZrBool zr_ffi_symbol_invoke_array(SZrState *state,
                                          SZrObject *selfObject,
                                          ZrFfiSymbolData *symbolData,
                                          SZrObject *argumentsArray,
                                          ZrLibCallContext *writebackContext,
                                          SZrTypeValue *result) {
    const SZrTypeValue *ownerValue;
    SZrObject *ownerObject;
    ZrFfiLibraryData *libraryData;
    TZrSize argumentCount;
    TZrSize index;
    char errorBuffer[ZR_FFI_ERROR_BUFFER_LENGTH] = {0};
#if ZR_VM_HAS_LIBFFI
    ZrFfiMarshalledValue *marshalledValues = ZR_NULL;
    void **ffiArguments = ZR_NULL;
    SZrTypeValue **callbackArguments = ZR_NULL;
    SZrTypeValue *loadedReferenceValues = ZR_NULL;
    TZrBool *managedReferenceArguments = ZR_NULL;
    SZrGcNativeCallPin selfPin = {0};
    SZrGcNativeCallPin ownerPin = {0};
    SZrGcNativeCallPin *argumentPins = ZR_NULL;
    SZrGcNativeCallPin *referencePins = ZR_NULL;
    unsigned char *returnStorage = ZR_NULL;
    TZrBool callSucceeded = ZR_FALSE;
    ZrFfiErrorCode pendingErrorCode = ZR_FFI_ERROR_NONE;
    char pendingErrorMessage[ZR_FFI_ERROR_BUFFER_LENGTH] = {0};
    int capturedErrno = 0;
#if defined(ZR_PLATFORM_WIN)
    DWORD capturedLastError = ERROR_SUCCESS;
#endif
#endif
    if (selfObject == ZR_NULL || symbolData == ZR_NULL) {
        zr_ffi_raise_error(state, ZR_FFI_ERROR_MARSHAL, "symbol invoke requires a valid SymbolHandle");
        return ZR_FALSE;
    }
    if (symbolData->base.kind != ZR_FFI_HANDLE_SYMBOL) {
        zr_ffi_raise_error(state, ZR_FFI_ERROR_MARSHAL, "symbol handle has unexpected internal kind");
        return ZR_FALSE;
    }
    if (symbolData->closed) {
        zr_ffi_raise_error(state, ZR_FFI_ERROR_NATIVE_CALL, "symbol handle is closed");
        return ZR_FALSE;
    }
    if (argumentsArray == ZR_NULL) {
        zr_ffi_raise_error(state, ZR_FFI_ERROR_MARSHAL, "symbol invoke requires an arguments array");
        return ZR_FALSE;
    }
    ownerValue = zr_ffi_find_field_raw(state, selfObject, ZR_FFI_HIDDEN_OWNER_FIELD);
    if (ownerValue == ZR_NULL || ownerValue->type != ZR_VALUE_TYPE_OBJECT || ownerValue->value.object == ZR_NULL) {
        zr_ffi_raise_error(state, ZR_FFI_ERROR_NATIVE_CALL, "symbol handle has no owning LibraryHandle");
        return ZR_FALSE;
    }
    ownerObject = ZR_CAST_OBJECT(state, ownerValue->value.object);
    libraryData = (ZrFfiLibraryData *) zr_ffi_get_handle_data(state, ownerObject);
    if (libraryData == ZR_NULL || libraryData->base.kind != ZR_FFI_HANDLE_LIBRARY || libraryData->closeRequested ||
        libraryData->libraryHandle == ZR_NULL) {
        zr_ffi_raise_error(state, ZR_FFI_ERROR_LOAD, "owning library handle is closed");
        return ZR_FALSE;
    }
    argumentCount = zr_ffi_array_length(state, argumentsArray);
    if (symbolData->signature == ZR_NULL || argumentCount != symbolData->signature->parameterCount) {
        zr_ffi_raise_error(
                state, ZR_FFI_ERROR_MARSHAL, "symbol '%s' expected %llu arguments but got %llu",
                symbolData->symbolName != ZR_NULL ? symbolData->symbolName : "<symbol>",
                (unsigned long long) (symbolData->signature != ZR_NULL ? symbolData->signature->parameterCount : 0),
                (unsigned long long) argumentCount);
        return ZR_FALSE;
    }
#if !ZR_VM_HAS_LIBFFI
    zr_ffi_raise_error(state, ZR_FFI_ERROR_ABI_MISMATCH, "this build does not include libffi");
    return ZR_FALSE;
#else
    if (argumentCount > 0) {
        marshalledValues = (ZrFfiMarshalledValue *) calloc(argumentCount, sizeof(ZrFfiMarshalledValue));
        ffiArguments = (void **) calloc(argumentCount, sizeof(void *));
        callbackArguments = (SZrTypeValue **) calloc(argumentCount, sizeof(SZrTypeValue *));
        loadedReferenceValues = (SZrTypeValue *)calloc(
                argumentCount, sizeof(SZrTypeValue));
        managedReferenceArguments = (TZrBool *)calloc(
                argumentCount, sizeof(TZrBool));
        argumentPins = (SZrGcNativeCallPin *) calloc(argumentCount, sizeof(SZrGcNativeCallPin));
        referencePins = (SZrGcNativeCallPin *)calloc(
                argumentCount, sizeof(SZrGcNativeCallPin));
    }
    if ((argumentCount > 0 &&
          (marshalledValues == ZR_NULL || ffiArguments == ZR_NULL ||
           callbackArguments == ZR_NULL || loadedReferenceValues == ZR_NULL ||
           managedReferenceArguments == ZR_NULL || argumentPins == ZR_NULL ||
           referencePins == ZR_NULL))) {
        zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_MARSHAL,
                    "out of memory while preparing ffi arguments");
        goto cleanup;
    }
    /* 原生函数可同步回调 VM 并触发 GC；在 ffi_call 全程固定符号、库所有者、
     * 参数对象和 ref/out 属性引用，避免原生地址及 VM 值失效。 */
    if (!ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(selfObject), &selfPin) ||
        !ZrCore_Gc_NativeCallPinValue(state, ownerValue, &ownerPin)) {
        zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_NATIVE_CALL,
                    "failed to pin symbol handle for ffi call");
        goto cleanup;
    }
    for (index = 0; index < argumentCount; index++) {
        const SZrTypeValue *argumentValue = zr_ffi_array_get(state, argumentsArray, index);
        const SZrTypeValue *effectiveArgumentValue = argumentValue;
        ZrFfiTypeLayout *parameterType = symbolData->signature->parameters[index].type;
        ZrFfiMarshalledValue *marshalledValue = &marshalledValues[index];
        /* 只有直接元调用提供 writebackContext，才把可写属性引用降为
         * 原生 ref/out 存储；inout 在调用前读取，out 在调用后写回。 */
        if (parameterType->kind == ZR_FFI_TYPE_POINTER &&
            parameterType->as.pointer.direction != ZR_FFI_DIRECTION_IN &&
            writebackContext != ZR_NULL &&
            ZrCore_PropertyReference_IsValid(
                    state, (SZrTypeValue *)argumentValue)) {
            managedReferenceArguments[index] = ZR_TRUE;
            ZrCore_Value_ResetAsNull(&loadedReferenceValues[index]);
            if (parameterType->as.pointer.direction == ZR_FFI_DIRECTION_INOUT &&
                !ZrCore_PropertyReference_Load(
                        state,
                        (SZrTypeValue *)argumentValue,
                        &loadedReferenceValues[index])) {
                zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_MARSHAL,
                    "failed to load ref argument %llu for symbol '%s'",
                        (unsigned long long)(index + 1),
                        symbolData->symbolName != ZR_NULL
                                ? symbolData->symbolName
                                : "<symbol>");
                goto cleanup;
            }
            effectiveArgumentValue = &loadedReferenceValues[index];
            if (!ZrCore_Gc_NativeCallPinValue(
                        state, argumentValue, &referencePins[index])) {
                zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_NATIVE_CALL,
                    "failed to pin managed ref/out argument %llu for ffi call",
                        (unsigned long long)(index + 1));
                goto cleanup;
            }
        }
        if (!ZrCore_Gc_NativeCallPinValue(
                    state, effectiveArgumentValue, &argumentPins[index])) {
            zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_NATIVE_CALL,
                    "failed to pin argument %llu for ffi call",
                               (unsigned long long)(index + 1));
            goto cleanup;
        }
        /* function 形参传 libffi closure 代码地址；canonical 哈希使
         * CallbackHandle 与原生函数指针契约在调用前完成一致性检查。 */
        if (parameterType->kind == ZR_FFI_TYPE_FUNCTION) {
            SZrObject *callbackObject = ZR_NULL;
            ZrFfiCallbackData *callbackData = ZR_NULL;
            if (!zr_ffi_value_is_object(argumentValue, &callbackObject)) {
                zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_MARSHAL,
                    "argument %llu must be a CallbackHandle",
                                   (unsigned long long) (index + 1));
                goto cleanup;
            }
            callbackData = (ZrFfiCallbackData *) zr_ffi_get_handle_data(state, callbackObject);
            if (callbackData == ZR_NULL || callbackData->base.kind != ZR_FFI_HANDLE_CALLBACK || callbackData->closed) {
                zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_MARSHAL,
                    "argument %llu is not an open CallbackHandle",
                                   (unsigned long long) (index + 1));
                goto cleanup;
            }
            if (parameterType->canonicalSignatureHash != 0u &&
                (callbackData->signature == ZR_NULL ||
                callbackData->signature->canonicalSignatureHash !=
                        parameterType->canonicalSignatureHash)) {
                zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_ABI_MISMATCH,
                    "argument %llu callback signature does not match canonical native contract",
                        (unsigned long long)(index + 1));
                goto cleanup;
            }
            marshalledValue->callbackData = callbackData;
            marshalledValue->callbackCodePointer = callbackData->codePointer;
            marshalledValue->argumentPointer =
                    &marshalledValue->callbackCodePointer;
            ffiArguments[index] = marshalledValue->argumentPointer;
            callbackArguments[index] = (SZrTypeValue *) argumentValue;
            continue;
        }
        if (parameterType->kind == ZR_FFI_TYPE_POINTER &&
            parameterType->as.pointer.direction != ZR_FFI_DIRECTION_IN &&
            writebackContext != ZR_NULL) {
            ZrFfiTypeLayout *pointeeType = parameterType->as.pointer.pointee;

            marshalledValue->ownedPointeeAllocation = calloc(
                    1, zr_ffi_non_void_call_storage_size(pointeeType));
            marshalledValue->ownedAllocation = calloc(1, sizeof(void *));
            if (marshalledValue->ownedPointeeAllocation == ZR_NULL ||
                marshalledValue->ownedAllocation == ZR_NULL) {
                zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_MARSHAL,
                    "out of memory while marshalling ref/out argument");
                goto cleanup;
            }
            if (parameterType->as.pointer.direction == ZR_FFI_DIRECTION_INOUT &&
                !zr_ffi_build_scalar_argument(
                        state,
                        effectiveArgumentValue,
                        pointeeType,
                        marshalledValue->ownedPointeeAllocation,
                        errorBuffer,
                        sizeof(errorBuffer))) {
                zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_MARSHAL,
                    "ref argument %llu for symbol '%s' failed to marshal: %s",
                        (unsigned long long)(index + 1),
                        symbolData->symbolName != ZR_NULL
                                ? symbolData->symbolName
                                : "<symbol>",
                        errorBuffer);
                goto cleanup;
            }
            *(void **)marshalledValue->ownedAllocation =
                    marshalledValue->ownedPointeeAllocation;
            marshalledValue->argumentPointer =
                    marshalledValue->ownedAllocation;
            ffiArguments[index] = marshalledValue->argumentPointer;
            continue;
        }
        marshalledValue->ownedAllocation = calloc(1, zr_ffi_non_void_call_storage_size(parameterType));
        if (marshalledValue->ownedAllocation == ZR_NULL) {
            zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_MARSHAL,
                    "out of memory while marshalling argument");
            goto cleanup;
        }
        if (!zr_ffi_build_scalar_argument(state, effectiveArgumentValue, parameterType,
                                          marshalledValue->ownedAllocation, errorBuffer, sizeof(errorBuffer))) {
            zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_MARSHAL,
                    "argument %llu for symbol '%s' failed to marshal: %s", (unsigned long long) (index + 1),
                               symbolData->symbolName != ZR_NULL ? symbolData->symbolName : "<symbol>", errorBuffer);
            goto cleanup;
        }
        marshalledValue->argumentPointer = marshalledValue->ownedAllocation;
        ffiArguments[index] = marshalledValue->argumentPointer;
    }
    returnStorage = (unsigned char *) calloc(1, zr_ffi_non_void_call_storage_size(symbolData->signature->returnType));
    if (returnStorage == ZR_NULL) {
        zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_MARSHAL,
                    "out of memory while preparing ffi return storage");
        goto cleanup;
    }
    /* 回调只在本次原生调用的生命周期内激活；保存旧状态是为了同一
     * CallbackHandle 被嵌套调用时，外层策略在内层返回后继续生效。 */
    for (index = 0u; index < argumentCount; index++) {
        ZrFfiMarshalledValue *marshalledValue = &marshalledValues[index];
        ZrFfiCallbackData *callbackData = marshalledValue->callbackData;

        if (callbackData == ZR_NULL) {
            continue;
        }
        marshalledValue->savedInvocationActive = callbackData->invocationActive;
        marshalledValue->savedLifetime = callbackData->activeLifetime;
        marshalledValue->savedThreadPolicy = callbackData->activeThreadPolicy;
        marshalledValue->savedExceptionPolicy = callbackData->activeExceptionPolicy;
        marshalledValue->savedCallbackExceptionObserved =
                callbackData->callbackExceptionObserved;
        marshalledValue->savedLastError = callbackData->lastError;
        memcpy(
                marshalledValue->savedLastErrorMessage,
                callbackData->lastErrorMessage,
                sizeof(marshalledValue->savedLastErrorMessage));
        callbackData->lastError = ZR_FFI_ERROR_NONE;
        callbackData->lastErrorMessage[0] = '\0';
        callbackData->activeLifetime = symbolData->signature->callbackLifetime;
        callbackData->activeThreadPolicy =
                symbolData->signature->callbackThreadPolicy;
        callbackData->activeExceptionPolicy =
                symbolData->signature->callbackExceptionPolicy;
        callbackData->callbackExceptionObserved = ZR_FALSE;
        callbackData->invocationActive = ZR_TRUE;
        marshalledValue->callbackActivationInstalled = ZR_TRUE;
    }
    if (symbolData->signature->errorPolicy == ZR_FFI_CONTRACT_ERROR_ERRNO) {
        errno = 0;
    }
#if defined(ZR_PLATFORM_WIN)
    if (symbolData->signature->errorPolicy ==
        ZR_FFI_CONTRACT_ERROR_LAST_ERROR) {
        SetLastError(ERROR_SUCCESS);
    }
#endif
    if (!zr_ffi_invoke_native_symbol(symbolData, returnStorage, ffiArguments, errorBuffer, sizeof(errorBuffer))) {
        zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_NATIVE_CALL,
                    "%s",
                           errorBuffer[0] != '\0' ? errorBuffer : "ffi native call failed");
        goto cleanup;
    }
    /* 原生错误通道必须紧随 ffi_call 捕获，后续 VM 封送与写回可能覆盖它们。 */
    capturedErrno = errno;
#if defined(ZR_PLATFORM_WIN)
    capturedLastError = GetLastError();
#endif
    if (symbolData->signature->errorPolicy ==
            ZR_FFI_CONTRACT_ERROR_RETURN_CODE) {
        TZrInt64 returnCode = 0;

        if (zr_ffi_return_code_is_failure(
                    symbolData->signature->returnType,
                    returnStorage,
                    &returnCode)) {
            zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_NATIVE_CALL,
                    "native symbol '%s' returned failure status %lld",
                    symbolData->symbolName != ZR_NULL
                            ? symbolData->symbolName
                            : "<symbol>",
                    (long long)returnCode);
            goto cleanup;
        }
    }
    if (symbolData->signature->errorPolicy == ZR_FFI_CONTRACT_ERROR_ERRNO &&
        capturedErrno != 0) {
        zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_NATIVE_CALL,
                    "native symbol '%s' reported errno %d",
                symbolData->symbolName != ZR_NULL
                        ? symbolData->symbolName
                        : "<symbol>",
                capturedErrno);
        goto cleanup;
    }
#if defined(ZR_PLATFORM_WIN)
    if (symbolData->signature->errorPolicy ==
                ZR_FFI_CONTRACT_ERROR_LAST_ERROR &&
        capturedLastError != ERROR_SUCCESS) {
        zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_NATIVE_CALL,
                    "native symbol '%s' reported last-error %lu",
                symbolData->symbolName != ZR_NULL
                        ? symbolData->symbolName
                        : "<symbol>",
                (unsigned long)capturedLastError);
        goto cleanup;
    }
#endif
    /* TODO: ref/out 按参数顺序逐个写回；后一个转换或 Store 失败时，
     * 前面的 VM 属性可能已经改变。核对写回是否承诺原子性，并补充失败注入用例。 */
    for (index = 0; index < argumentCount; index++) {
        ZrFfiMarshalledValue *marshalledValue = &marshalledValues[index];
        ZrFfiTypeLayout *parameterType =
                symbolData->signature->parameters[index].type;

        if (marshalledValue->ownedPointeeAllocation != ZR_NULL) {
            SZrTypeValue writebackValue;

            ZrLib_Value_SetNull(&writebackValue);
            if (!zr_ffi_set_result_from_scalar(
                        state,
                        parameterType->as.pointer.pointee,
                        marshalledValue->ownedPointeeAllocation,
                        &writebackValue) ||
                !(managedReferenceArguments[index]
                          ? ZrCore_PropertyReference_Store(
                                    state,
                                    (SZrTypeValue *)zr_ffi_array_get(
                                            state, argumentsArray, index),
                                    &writebackValue)
                          : ZrLib_CallContext_WriteBackArgument(
                                    writebackContext, index, &writebackValue))) {
                zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_MARSHAL,
                    "failed to write back ref/out argument %llu for symbol '%s'",
                        (unsigned long long)(index + 1),
                        symbolData->symbolName != ZR_NULL
                                ? symbolData->symbolName
                                : "<symbol>");
                goto cleanup;
            }
        }
    }
    /* return_default 策略允许回调异常以零返回值交还原生侧；其他
     * 回调错误由外层调用在统一清理后向 VM 报告。 */
    for (index = 0; index < argumentCount; index++) {
        if (callbackArguments[index] != ZR_NULL) {
            SZrObject *callbackObject = ZR_CAST_OBJECT(state, callbackArguments[index]->value.object);
            ZrFfiCallbackData *callbackData =
                    (ZrFfiCallbackData *) zr_ffi_get_handle_data(state, callbackObject);
            if (callbackData != ZR_NULL && callbackData->lastError != ZR_FFI_ERROR_NONE) {
                if (callbackData->callbackExceptionObserved &&
                    callbackData->activeExceptionPolicy ==
                            ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_RETURN_DEFAULT) {
                    callbackData->lastError = ZR_FFI_ERROR_NONE;
                    callbackData->lastErrorMessage[0] = '\0';
                    continue;
                }
                zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    callbackData->lastError,
                    "%s",
                                   callbackData->lastErrorMessage[0] != '\0' ? callbackData->lastErrorMessage
                                                                             : "ffi callback failed");
                goto cleanup;
            }
        }
    }
    if (!zr_ffi_set_result_from_scalar(state, symbolData->signature->returnType, returnStorage, result)) {
        zr_ffi_record_pending_error(
                    &pendingErrorCode,
                    pendingErrorMessage,
                    sizeof(pendingErrorMessage),
                    ZR_FFI_ERROR_MARSHAL,
                    "failed to marshal return value from symbol '%s'",
                           symbolData->symbolName != ZR_NULL ? symbolData->symbolName : "<symbol>");
        goto cleanup;
    }
    callSucceeded = ZR_TRUE;
cleanup:
    /* 无论封送、原生调用还是写回在哪一步失败，先恢复嵌套回调状态，
     * 再释放已分配的缓冲区和 GC pin，最后才把暂存错误抛向 VM。 */
    if (marshalledValues != ZR_NULL) {
        for (index = argumentCount; index > 0u; index--) {
            ZrFfiMarshalledValue *marshalledValue =
                    &marshalledValues[index - 1u];
            ZrFfiCallbackData *callbackData = marshalledValue->callbackData;

            if (callbackData == ZR_NULL ||
                !marshalledValue->callbackActivationInstalled) {
                continue;
            }
            /* 初次激活的回调保留本次 call 生命周期标记，供晚到的
             * 原生调用在 trampoline 中识别为过期；嵌套激活则还原外层状态。 */
            if (!marshalledValue->savedInvocationActive) {
                callbackData->invocationActive = ZR_FALSE;
                continue;
            }
            callbackData->invocationActive =
                    marshalledValue->savedInvocationActive;
            callbackData->activeLifetime = marshalledValue->savedLifetime;
            callbackData->activeThreadPolicy =
                    marshalledValue->savedThreadPolicy;
            callbackData->activeExceptionPolicy =
                    marshalledValue->savedExceptionPolicy;
            callbackData->callbackExceptionObserved =
                    marshalledValue->savedCallbackExceptionObserved;
            callbackData->lastError = marshalledValue->savedLastError;
            memcpy(
                    callbackData->lastErrorMessage,
                    marshalledValue->savedLastErrorMessage,
                    sizeof(callbackData->lastErrorMessage));
        }
    }
    if (marshalledValues != ZR_NULL) {
        for (index = 0; index < argumentCount; index++) {
            free(marshalledValues[index].ownedPointeeAllocation);
            free(marshalledValues[index].ownedAllocation);
        }
    }
    if (argumentPins != ZR_NULL) {
        for (index = argumentCount; index > 0; index--) {
            ZrCore_Gc_NativeCallUnpin(state->global, &argumentPins[index - 1u]);
        }
    }
    if (referencePins != ZR_NULL) {
        for (index = argumentCount; index > 0; index--) {
            ZrCore_Gc_NativeCallUnpin(
                    state->global, &referencePins[index - 1u]);
        }
    }
    ZrCore_Gc_NativeCallUnpin(state->global, &ownerPin);
    ZrCore_Gc_NativeCallUnpin(state->global, &selfPin);
    free(marshalledValues);
    free(ffiArguments);
    free(callbackArguments);
    free(loadedReferenceValues);
    free(managedReferenceArguments);
    free(argumentPins);
    free(referencePins);
    free(returnStorage);
    if (pendingErrorCode != ZR_FFI_ERROR_NONE) {
        zr_ffi_raise_error(
                state,
                pendingErrorCode,
                "%s",
                pendingErrorMessage);
        return ZR_FALSE;
    }
    return callSucceeded;
#endif
}
