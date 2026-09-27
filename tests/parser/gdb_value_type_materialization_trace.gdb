# 原意是在 value_type_runtime 的内联值与对象之间双向物化时比较字段来源和目标地址。
# BUG: execution_inline_frame.c:622/690 当前位于 prototype 记录辅助逻辑，非物化函数；descriptor 等采样变量在断点处不可用。
set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:622
commands
silent
if function && function->instructionsLength == 19
    printf "inline->object typeLayout=%u descriptor=%s fieldValue=%u/%p sourceAddress=%p\n", typeLayoutId, descriptor->name ? descriptor->name->stringDataExtend : 0, fieldValue.type, fieldValue.value.object, sourceAddress
end
continue
end
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:690
commands
silent
if function && function->instructionsLength == 19
    printf "object->inline typeLayout=%u descriptor=%s sourceField=%u/%p destAddress=%p\n", typeLayoutId, descriptor->name ? descriptor->name->stringDataExtend : 0, sourceFieldValue ? sourceFieldValue->type : 999, sourceFieldValue ? sourceFieldValue->value.object : 0, destinationAddress
end
continue
end
break __assert_fail
run
