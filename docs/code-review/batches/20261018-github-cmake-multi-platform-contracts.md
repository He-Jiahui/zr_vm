# CMake 多平台与 sanitizer 工作流合同审查

本批逐项审查 .github/workflows/cmake-multi-platform.yml 的9个配置与有意义语句块，补6段中文文档；5个旧台账键复核、4个新键。没有函数定义，不能按函数或运行用例计数。定义、消费入口、失败传播、工具与产物边界记录在[九列表](../coverage/root_build_ci.tsv)。

文档说明矩阵选择、输出目录如何供后续configure/build/CTest消费、TypeScript依赖前置条件，以及独立sanitizer构建与测试入口之间的协作。三个旧注释改为当前矩阵事实：Ubuntu GCC/Clang Release与Windows MSVC Debug；未据没有Windows Release条目推断历史构建阻塞仍存在。各命令结果须由对应runner实际记录确定。

仅替换39至41行三个完整注释行，并在EOF追加配置文档；原93个非注释行、原物理行号、EOL和UTF8字节列保持。注释长度变化使后续绝对字节偏移变化，未声称所有原始偏移不变。未执行GitHub远端CI、三平台构建或TSAN，本批没有这些运行成功结论。

配对运行结束后，其他会话更新了docs/wiki/03-modules/compile.md（9727B→10656B）；该文件不在工作流20项定义契约域内，本批不暂存它。配对报告明确披露此后续漂移，不声称当前130项输入仍全等或当前Wiki校验通过；两阶段执行当时的有限快照与原始结果仍保留。

在实际checkout使用已核物理CPython3.14.4，按工作流现有入口依次执行三份unittest模块及 scripts/validate_wiki.py --root E:/Git/zr_vm，注释前后各运行一次。实际12方法均为9通过、1失败、2错误，unittest和校验器均退出1；校验器均报告118个Markdown、115个manifest页面、726个本地链接及81项错误。两ERROR由工作树缺失zensical.toml触发；FAIL在test_validate_wiki.py:61比较errors()时被81项诊断阻断，未到固定页面数断言。未将这些结果称为PASS或本批新BUG。

两阶段130个仓库输入除批准的两份YAML注释外逐文件保持，Wiki成员集、Pygments339文件、7项dist-info、20项工具与启动输入保持。原始stdout/stderr保留；仅对unittest的Ran行数值耗时作明确归一后比较，其余日志、12方法ID与结果逐项比较；此证据不等于完整宿主闭包或lexer安装注册成功。

证据：

- github-workflows-current-contract-review-r1.json及独立静态复核：20配置/块全量审查，13中文文档，无新增BUG/TODO。
- github-workflows-source-comment-application-r1.json：精确两源变换与逆向证明。
- github-workflows-contracts-native-before-r1-results.json及after-r1对应结果、真实终态。
- github-workflows-contracts-native-final-acceptance-r1.json，SHA256 4233fb4b488ebcce4211e02d0f292358e28f6862d4c8c268e3c0849aa743d1ee：有限注释配对接受，现有测试失败保留。

不新增重复实现的测试，不修改公开API或程序行为。check-batch和暂存空白检查单独要求成功；全仓inventory基线仍陈旧，前后相同退出1不等于全仓通过。产物保留于E:/cargo-targets/zr_vm，阶段临时目录端点均空，未创建pycache；生命周期OPEN，手动释放0B。
