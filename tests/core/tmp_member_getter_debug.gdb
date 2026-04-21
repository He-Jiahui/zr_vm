break test_member_property_getter_native
run
finish
finish
finish
finish
finish
finish
bt
info locals
print stableResult.type
print stableResult.value.nativeObject.nativeInt64
print hasResultAnchor
print result
print ZrCore_Function_StackAnchorRestore(state, &resultAnchor)
continue
