# 扩展贡献清单测试审查

本批覆盖 extensionContributions.test.js 的10个审查单元：1个具名函数、2个 Node 测试回调、4个 some 谓词，以及3个有意义的断言块。保留5个既有物理键，补齐5个新键；已补3份中文 JSDoc，其余7项在九列表中逐项记录无需补注释的判断。没有新增 BUG 或 TODO。

readPackageJson 按文件位置同步重读 package.json，每次返回新的解析结果；两次读取之间没有原子快照，读取、解析和形状访问异常向当前测试传播。局部 JSON 由 JavaScript 管理，不向调用者交付文件句柄，不写夹具或启动扩展。

导入清理场景只检查激活字符串、命令展示字段和菜单 when 字面量的存在性，允许额外项及重复项；它不求值菜单条件，也不证明实际命令注册、编辑效果或界面可见性。信任场景只约束 supported=false 与三个路径配置的 restricted=true，不证明 VS Code 执行这些限制。首个异常中断当前回调，不能把静态断言数当实际运行次数。

源码只追加批准的三份完整私有 JSDoc。原2175字节、42行CRLF程序前缀及原行列坐标精确保留；删除3924字节后缀可恢复原始哈希。公开接口、签名与程序行为没有修改。

使用实体 Node v22.22.3，执行现有命令 node --test test/extensionContributions.test.js。注释前后分别实际运行2项测试，均2PASS、0FAIL、0跳过，stderr为空；两份原始stdout仅三处耗时数值不同，移除这些确切耗时行后完全相同。未运行整个 npm 通配测试、桌面/Web smoke 或 VS Code 信任执行场景。

证据：
- 静态逐单元提案：extension-contributions-current-contract-review-r1.json，SHA256 a9ae880e53b7bff79c6756b86ff348cd100a99b55d62978f5c19ef05d09f81be。
- 独立合同复核：extension-contributions-current-contract-independent-review-r1.json，SHA256 1b5929762bd9cb8b6803bf3bd21d0aae8853ff0c41225fdf7bd7a347107e14c2。
- 源码应用：extension-contributions-tests-source-comment-application-r1.json，SHA256 e6e4344a94fb5549008f8212c845626b327e84eb7713d2708265e1aee58c5220。
- 真实前后配对：extension-contributions-tests-native-final-acceptance-r1.json，SHA256 207a0eb031a0c75808a3d6f113efc31bd739b4d30e6e58b6a1587124f2bd2fb0；66baff/b466bd 外层和两个被等待的 Node 子进程均实际退出0。

报告、原始日志及失败诊断保留在 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/。本批没有新增测试、构建树或源码副本；产物生命周期仍 OPEN，手动释放0B。混合台账仅替换本文件5条旧记录、追加5条新记录；inventory 仅投影本文件的哈希、状态和说明三列。
