# SIGABRT 后读取预安装闭包的四个捕获槽；frame 7 依赖当前调试构建的调用栈。
# TODO: 当前无自动化入口固定 frame 7，运行前需用 bt 核对栈帧和捕获槽数量。
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
