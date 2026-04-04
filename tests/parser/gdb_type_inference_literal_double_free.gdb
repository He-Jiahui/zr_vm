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
