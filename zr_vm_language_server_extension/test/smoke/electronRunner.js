const { runSmokeSuite } = require('./smokeSuite.js');

/** VS Code Electron 测试宿主的导出入口；把启动脚本传入的服务端模式和焦点交给共用桌面套件。
 * @note run-electron-smoke.js 应显式传 native；未传值时默认执行完整桌面场景。
 */
async function run() {
    await runSmokeSuite({
        expectedMode: process.env.ZR_TEST_SERVER_MODE || 'native',
        focus: process.env.ZR_TEST_SMOKE_FOCUS || 'all',
    });
}

module.exports = {
    run,
};
