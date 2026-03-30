const fs = require('node:fs');
const net = require('node:net');
const path = require('node:path');
const playwright = require('playwright');
const { runTests } = require('@vscode/test-web');

const EDGE_EXECUTABLE_CANDIDATES = [
    'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
    'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
];

patchChromiumLaunchForEdgeFallback();

async function main() {
    const extensionDevelopmentPath = path.resolve(__dirname, '..');
    const extensionTestsPath = path.resolve(__dirname, '..', 'test', 'smoke', 'browserRunner.js');
    const repoRoot = path.resolve(__dirname, '..', '..');
    const workspacePath = path.resolve(repoRoot, 'tests', 'fixtures', 'projects', 'import_basic');
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

function shouldRetryWithEdge(error) {
    const message = String(error && error.message ? error.message : error);
    return message.includes('Executable doesn\'t exist') ||
        message.includes('browserType.launch') ||
        message.includes('Please run the following command');
}

function hasSystemEdge() {
    return EDGE_EXECUTABLE_CANDIDATES.some((candidate) => fs.existsSync(candidate));
}

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

main().catch((error) => {
    console.error(error.stack || String(error));
    process.exit(1);
});
