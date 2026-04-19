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
