const fs = require('node:fs');
const net = require('node:net');
const path = require('node:path');
// TODO: playwright 目前仅由 @vscode/test-web 间接依赖提供；核查严格或嵌套安装布局下
// 此处能否解析到与 test-web 相同的实例，否则入口会失败或 launch 回退无法生效。
const playwright = require('playwright');
const { runTests } = require('@vscode/test-web');

// Playwright 自带 Chromium 缺失时，允许 Windows 桌面安装的 Edge 承接 Web smoke。
// 此清单只覆盖常见系统安装位置；自定义浏览器位置需要另行确认。
const EDGE_EXECUTABLE_CANDIDATES = [
    'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
    'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
];

// 当前 npm 安装中 test-web 与本脚本共享 Playwright 模块；须先安装 launch 回退，再调用 runTests。
patchChromiumLaunchForEdgeFallback();

/** package.json 的 Web e2e 命令构建并同步 WASM 后，从 fixture 启动浏览器扩展测试。
 * @note 默认用 import_basic；ZR_TEST_WORKSPACE_PATH 必须是现存目录。
 */
async function main() {
    const extensionDevelopmentPath = path.resolve(__dirname, '..');
    const extensionTestsPath = path.resolve(__dirname, '..', 'test', 'smoke', 'browserRunner.js');
    const repoRoot = path.resolve(__dirname, '..', '..');
    const workspacePath = path.resolve(process.env.ZR_TEST_WORKSPACE_PATH ||
        path.join(repoRoot, 'tests', 'fixtures', 'projects', 'import_basic'));
    if (!fs.statSync(workspacePath).isDirectory()) {
        throw new Error(`Smoke workspace is not a directory: ${workspacePath}`);
    }
    const port = await findAvailablePort();

    await runTests({
        browserType: 'chromium',
        extensionDevelopmentPath,
        extensionTestsPath,
        folderPath: workspacePath,
        headless: true,
        quality: 'stable',
        testRunnerDataDir: path.join(extensionDevelopmentPath, '.vscode-test-web'),
        verbose: true,
        port,
    });
}

/** 仅在 Chromium 启动失败且系统 Edge 可用时重试同一组 test-web 启动参数。
 * 在共享实例的安装布局中只覆盖 Playwright chromium.launch；不影响其他浏览器类型。
 */
function patchChromiumLaunchForEdgeFallback() {
    const originalLaunch = playwright.chromium.launch.bind(playwright.chromium);

    playwright.chromium.launch = async (options = {}) => {
        try {
            return await originalLaunch(options);
        } catch (error) {
            if (!shouldRetryWithEdge(error) || !hasSystemEdge()) {
                throw error;
            }

            const retryOptions = {
                ...options,
                channel: 'msedge',
            };
            return originalLaunch(retryOptions);
        }
    };
}

/** 从 Playwright 错误文本判断 Chromium 缺失或启动失败是否适合尝试系统 Edge。
 * TODO: browserType.launch 匹配范围很宽，可能包括非浏览器缺失错误；应核对
 * test-web/Playwright 当前错误分类，避免二次失败掩盖原始原因。
 */
function shouldRetryWithEdge(error) {
    const message = String(error && error.message ? error.message : error);
    return message.includes('Executable doesn\'t exist') ||
        message.includes('browserType.launch') ||
        message.includes('Please run the following command');
}

/** 只探测标准 Windows 安装路径；安装于其他目录时保留 Chromium 原始错误。 */
function hasSystemEdge() {
    return EDGE_EXECUTABLE_CANDIDATES.some((candidate) => fs.existsSync(candidate));
}

/** 为 test-web 的本地 HTTP 服务选一个当前空闲的 loopback 端口。
 * TODO: 探测后立即关闭监听，runTests 稍后才绑定；而探测使用 127.0.0.1，
 * test-web 默认监听 localhost。核查端口抢占与 IPv4/IPv6 解析差异时的恢复路径。
 */
function findAvailablePort() {
    return new Promise((resolve, reject) => {
        const server = net.createServer();
        server.unref();
        server.on('error', reject);
        server.listen(0, '127.0.0.1', () => {
            const address = server.address();
            if (!address || typeof address === 'string') {
                server.close(() => reject(new Error('Unable to resolve an ephemeral port for web smoke.')));
                return;
            }

            const { port } = address;
            server.close((closeError) => {
                if (closeError) {
                    reject(closeError);
                    return;
                }
                resolve(port);
            });
        });
    });
}

// 浏览器下载、服务启动或 smoke 断言失败都需要向 npm 返回非零退出状态。
main().catch((error) => {
    console.error(error.stack || String(error));
    process.exit(1);
});
