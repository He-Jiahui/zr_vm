set pagination off
set confirm off
set print pretty on
handle SIGPIPE nostop noprint pass
catch signal SIGABRT
run
frame 7
print ((SZrClosure*)currentCallableObject)->closureValueCount
print ((SZrClosure*)currentCallableObject)->closureValuesExtend[0]->value.valuePointer
print &((SZrClosure*)currentCallableObject)->closureValuesExtend[0]->link.closedValue
print *(((SZrClosure*)currentCallableObject)->closureValuesExtend[0]->value.valuePointer)
print ((SZrClosure*)currentCallableObject)->closureValuesExtend[1]->value.valuePointer
print &((SZrClosure*)currentCallableObject)->closureValuesExtend[1]->link.closedValue
print *(((SZrClosure*)currentCallableObject)->closureValuesExtend[1]->value.valuePointer)
print ((SZrClosure*)currentCallableObject)->closureValuesExtend[2]->value.valuePointer
print &((SZrClosure*)currentCallableObject)->closureValuesExtend[2]->link.closedValue
print *(((SZrClosure*)currentCallableObject)->closureValuesExtend[2]->value.valuePointer)
print ((SZrClosure*)currentCallableObject)->closureValuesExtend[3]->value.valuePointer
print &((SZrClosure*)currentCallableObject)->closureValuesExtend[3]->link.closedValue
print *(((SZrClosure*)currentCallableObject)->closureValuesExtend[3]->value.valuePointer)
quit
