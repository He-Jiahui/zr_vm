# 在 clang Debug 的 type_inference 单测中记录推断类型释放入口和嵌套元素数组状态，
# 用于调查历史重复释放；SIGABRT 停止时再由末尾 bt full 读取故障帧。
# 当前脚本固定构建路径并会采集所有 Free 调用，输出不等同于重复释放证据。
set pagination off
set print pretty on
set confirm off
set breakpoint pending on

file ./build/codex-wsl-clang-debug/bin/zr_vm_type_inference_test
set args --verbose

handle SIGABRT stop print nopass
break ZrParser_InferredType_Free
commands
    silent
    printf "\n[break] ZrParser_InferredType_Free state=%p type=%p\n", state, type
    if type != 0
        printf "  baseType=%d typeName=%p elemValid=%d elemHead=%p elemLen=%llu elemCap=%llu elemSize=%llu\n", \
            type->baseType, type->typeName, type->elementTypes.isValid, type->elementTypes.head, \
            (unsigned long long)type->elementTypes.length, (unsigned long long)type->elementTypes.capacity, \
            (unsigned long long)type->elementTypes.elementSize
    end
    continue
end

run
bt full
