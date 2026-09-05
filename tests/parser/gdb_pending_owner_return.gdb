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
