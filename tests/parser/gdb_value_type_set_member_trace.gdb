# 原意是在短构造函数和长函数的成员赋值中观察接收者位置与赋入值。
# BUG: execution_inline_frame.c:1037 当前处理 union 字段元数据，非 set-member helper；下面的 function/memberName/assignedValue 等局部量不可用。
set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:1037
commands
silent
if function && (function->instructionsLength == 4 || function->instructionsLength == 19)
    printf "setMember fnInstr=%u receiver=%u source=%u member=%s fieldValueSlot=%u/%p assigned=%u/%p receiverAddr=%p\n", function->instructionsLength, receiverSlot, sourceSlot, memberName ? memberName->stringDataExtend : 0, fieldLayout.isValueSlot, fieldLayout.typeLayoutId, assignedValue->type, assignedValue->value.object, receiverPlace.address
end
continue
end
break __assert_fail
run
