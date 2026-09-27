# 手工用于 resource_shared_weak 单测：从嵌套 finally 的 Shared 返回用例开始，
# 记录强引用 retain/release，抵达 Weak 返回用例时停止；窗口也包含夹在其间的 pending-return 用例。
# 调用时先以 gdb --args 加载 zr_vm_resource_shared_weak_test；计数是操作前的 control 状态。
set pagination off
set confirm off
break test_pending_shared_return_through_nested_finally
run
break ZrCore_OwnershipShared_RetainStrong
commands
silent
printf "RETAIN %p strong=%u\n", control, control->strongRefCount
bt 6
continue
end
break ZrCore_OwnershipShared_ReleaseStrong
commands
silent
printf "RELEASE %p strong=%u\n", control, control->strongRefCount
bt 6
continue
end
break test_pending_weak_return_through_nested_finally
commands
silent
quit
end
continue
