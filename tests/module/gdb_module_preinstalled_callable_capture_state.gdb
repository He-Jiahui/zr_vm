set pagination off
set confirm off
set print pretty on
handle SIGPIPE nostop noprint pass
catch signal SIGABRT
run
frame 7
print memberNativeName
print opA->type
print/x opA->value.object
print ((SZrRawObject*)opA->value.object)->type
print ((SZrObject*)opA->value.object)->internalType
call ZrCore_Debug_PrintObject(state, (SZrObject*)opA->value.object, stderr)
print ((SZrClosure*)currentCallableObject)->closureValueCount
print ((SZrClosure*)currentCallableObject)->closureValuesExtend[0]
print ((SZrClosure*)currentCallableObject)->closureValuesExtend[1]
print ((SZrClosure*)currentCallableObject)->closureValuesExtend[2]
print ((SZrClosure*)currentCallableObject)->closureValuesExtend[3]
print *ZrCore_ClosureValue_GetValue(((SZrClosure*)currentCallableObject)->closureValuesExtend[0])
print *ZrCore_ClosureValue_GetValue(((SZrClosure*)currentCallableObject)->closureValuesExtend[1])
print *ZrCore_ClosureValue_GetValue(((SZrClosure*)currentCallableObject)->closureValuesExtend[2])
print *ZrCore_ClosureValue_GetValue(((SZrClosure*)currentCallableObject)->closureValuesExtend[3])
quit
