set pagination off
set breakpoint pending on
set debuginfod enabled off

file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args ./tests/fixtures/projects/classes_properties/classes_properties.zrp

break ZrFunctionPostCall
commands
silent
set $basev = (struct SZrTypeValue *)callInfo->functionBase.valuePointer
set $func = 0
set $fname = "<anonymous>"

if $basev->type == ZR_VALUE_TYPE_FUNCTION
    set $func = (struct SZrFunction *)$basev->value.object
end
if $basev->type == ZR_VALUE_TYPE_CLOSURE
    set $func = ((struct SZrClosure *)$basev->value.object)->function
end
if $func && $func->functionName
    set $fname = (char *)((struct SZrString *)$func->functionName)->stringDataExtend
end

if $func
    printf "POSTCALL %s returns=%ld", $fname, (long)resultCount
    if resultCount > 0
        set $ret = (struct SZrTypeValue *)(state->stackTop.valuePointer - resultCount)
        printf " type=%d", (int)$ret->type
        if $ret->type >= ZR_VALUE_TYPE_INT8 && $ret->type <= ZR_VALUE_TYPE_INT64
            printf " int=%lld", (long long)$ret->value.nativeObject.nativeInt64
        end
    end
    printf "\n"
end
continue
end

run
quit
