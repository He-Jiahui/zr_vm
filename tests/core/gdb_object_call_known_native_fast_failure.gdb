# 在已加载的已知原生函数快速路径 Unity 单测中只捕获断言失败现场；正常通过不会命中 UnityFail。
# 全局计数和污染标记属于该单测进程，不能用其他测试二进制替代。
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
