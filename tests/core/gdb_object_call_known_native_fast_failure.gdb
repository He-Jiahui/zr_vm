set pagination off
set confirm off
break UnityFail
commands
  silent
  printf "UnityFail hit\n"
  printf "gNativeCallCount=%u\n", gNativeCallCount
  printf "gObservedCorruption=%d\n", gObservedCorruption
  bt 10
  frame 2
  info locals
  frame 3
  info locals
  quit
end
run
