# 引入产品模块共用的目标、依赖和安装协议；顶层先配置common，后加入Testing目录。
# 仅提供配置helper，不在这里初始化VM或开启Test phase。
include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)

# 为本文件后续helper建立一致的变体基名zr_vm_lib_testing。
# 与descriptor业务身份zr.testing不同；static/shared目标由开关选择。
set(zr_curr_module_name "zr_vm_lib_testing")

# 按当前BUILD_STATIC_LIB/BUILD_SHARED_LIB创建变体；ON把common源码并入各变体。
# 模块src递归glob有CONFIGURE_DEPENDS；common需先发布源清单。共享定义SHARED，静态定义STATIC，插件入口由此分流。
zr_declare_module(${zr_curr_module_name} ON)

# 把core同类变体接入Testing，供断言VM帧/异常和task运行时头部契约使用。
# 依赖头和link在本模块目标上为PRIVATE；公共头消费者仍需自己的依赖包含关系。
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
# 接入native registry、context与值/原型适配，供注册和断言bridge消费。
# 只配置目标依赖；不会执行Register，PRIVATE头关系不等于安装依赖接口。
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")
# 接入descriptor使用的parser属性角色定义，维持native表与builtin schema一致。
# parser-only角色测试不直接消费Testing descriptor；真实一致性测试另链接Testing。
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_parser")

# 安装公开include目录，供嵌入宿主调用getter/Register/诊断API。
# 不安装src/runtime私有头；复制头文件本身不封装或验证全部依赖头关系。
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

# 安装已启用的模块变体，并由共用helper传播模块名PUBLIC定义。
# TODO:仓内尚无该模块名宏消费，外部包用途沿CommonMacros核查；双变体Windows同名lib冲突是已记录的共用helper BUG。
zr_install_module(${zr_curr_module_name})
