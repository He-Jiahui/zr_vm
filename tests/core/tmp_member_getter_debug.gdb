# 手工跟踪成员属性 getter 单测及其运行时调用链；调用时预装入 execution_member_access_fast_paths 单测。
# TODO: 六次 finish 依赖当时栈深，需在当前二进制重核实 stableResult/resultAnchor 所属帧。
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
# TODO: GDB print 会调用 StackAnchorRestore；六次 finish 后须先确认 state/resultAnchor 仍在当前帧有效。
print ZrCore_Function_StackAnchorRestore(state, &resultAnchor)
continue
