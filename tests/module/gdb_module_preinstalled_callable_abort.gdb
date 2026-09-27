# 手工调试模块预安装闭包触发 SIGABRT 的堆栈；由调用者提供待测可执行文件。
set pagination off
set confirm off
set print pretty on
handle SIGPIPE nostop noprint pass
catch signal SIGABRT
run
bt
bt full
frame 1
info locals
quit
