const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const { createArtifactLayout } = require('./artifact-layout');

// npm 的 build:native 和 vscode:prepublish 进入此脚本；布局必须与后续 sync:native 一致。
const layout = createArtifactLayout({
    repositoryRoot: path.resolve(__dirname, '..', '..'),
    extensionRoot: path.resolve(__dirname, '..'),
});
const repositoryRoot = layout.repositoryRoot;
const extensionRoot = layout.extensionRoot;
// ZR_NATIVE_BUILD_DIR 由调用方指定；后续清理路径要求它是可丢弃的专用构建目录。
const buildDir = layout.native.buildDir;
const buildConfig = layout.nativeBuildConfig;
// 默认构建两个打包入口；覆盖目标列表时，校验仍要求两个入口均存在并可启动。
const targets = (process.env.ZR_NATIVE_BUILD_TARGET || 'zr_vm_language_server_stdio,zr_vm_cli_executable')
    .split(',')
    .map((value) => value.trim())
    .filter((value) => value.length > 0);
const jobs = process.env.ZR_BUILD_JOBS || '8';
// 自定义请求的 smoke 必须能从仓内固定工程读到模块信息。
const projectModulesFixture = path.join(
    repositoryRoot,
    'tests',
    'fixtures',
    'projects',
    'import_basic',
    'import_basic.zrp',
);

/** 非开发者终端下查找 MSVC 环境入口，先尊重显式路径，再询问 VS 安装索引。 */
function findVsDevCmd() {
    const candidates = [];
    const programFilesX86 = process.env['ProgramFiles(x86)'] || 'C:\\Program Files (x86)';
    const vswhere = path.join(programFilesX86, 'Microsoft Visual Studio', 'Installer', 'vswhere.exe');

    if (process.env.VSDEVCMD_PATH) {
        candidates.push(process.env.VSDEVCMD_PATH);
    }

    candidates.push(
        'D:\\Tools\\VS\\Common7\\Tools\\VsDevCmd.bat',
        'C:\\Program Files\\Microsoft Visual Studio\\2026\\Community\\Common7\\Tools\\VsDevCmd.bat',
        'C:\\Program Files\\Microsoft Visual Studio\\2026\\Professional\\Common7\\Tools\\VsDevCmd.bat',
        'C:\\Program Files\\Microsoft Visual Studio\\2026\\Enterprise\\Common7\\Tools\\VsDevCmd.bat',
        'C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\Tools\\VsDevCmd.bat',
        'C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\Common7\\Tools\\VsDevCmd.bat',
        'C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\Common7\\Tools\\VsDevCmd.bat',
    );

    for (const candidate of candidates) {
        if (candidate && fs.existsSync(candidate)) {
            return path.resolve(candidate);
        }
    }

    if (fs.existsSync(vswhere)) {
        const result = spawnSync(vswhere, [
            '-latest',
            '-products',
            '*',
            '-requires',
            'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
            '-find',
            'Common7\\Tools\\VsDevCmd.bat',
        ], {
            cwd: repositoryRoot,
            encoding: 'utf8',
        });
        if (result.status === 0) {
            const resolved = String(result.stdout || '')
                .split(/\r?\n/)
                .map((line) => line.trim())
                .find((line) => line.length > 0 && fs.existsSync(line));
            if (resolved) {
                return resolved;
            }
        }
    }

    return null;
}

/** 把 VsDevCmd 导出的环境复制给 CMake 子进程，避免依赖调用者预先打开开发者终端。 */
function importVsDevCmdEnvironment() {
    const vsDevCmdPath = findVsDevCmd();
    const arch = process.env.ZR_VS_ARCH || 'x64';
    const hostArch = process.env.ZR_VS_HOST_ARCH || arch;

    if (!vsDevCmdPath) {
        console.error('Could not locate VsDevCmd.bat. Set VSDEVCMD_PATH or launch from a Visual Studio developer shell.');
        process.exit(1);
    }

    const result = spawnSync('cmd.exe', [
        '/d',
        '/c',
        `call "${vsDevCmdPath}" -no_logo -arch=${arch} -host_arch=${hostArch} >nul && set`,
    ], {
        cwd: repositoryRoot,
        encoding: 'utf8',
        windowsVerbatimArguments: true,
    });

    if (result.status !== 0) {
        process.stderr.write(result.stderr || '');
        console.error(`Failed to import Visual Studio environment from ${vsDevCmdPath}`);
        process.exit(result.status ?? 1);
    }

    const importedEnv = { ...process.env };
    for (const line of String(result.stdout || '').split(/\r?\n/)) {
        const separator = line.indexOf('=');
        if (separator <= 0) {
            continue;
        }

        importedEnv[line.slice(0, separator)] = line.slice(separator + 1);
    }

    return importedEnv;
}

// Windows 缺少开发者环境时先导入；已有环境或其他平台沿用调用者环境。
const env = process.platform === 'win32' && !process.env.VSCMD_VER
    ? importVsDevCmdEnvironment()
    : process.env;
const nativeToolchainEnv = sanitizeNativeToolchainEnvironment(env);

// 构建命令在模块加载时运行；测试只能以脚本进程调用，不应 require 此文件。
try {
    buildAndVerifyNativeAssets(nativeToolchainEnv);
} catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    process.exit(1);
}

/** 从构建子进程 PATH 排除扩展本地 npm .bin，避免工具链命令落到依赖包装器。 */
function sanitizeNativeToolchainEnvironment(sourceEnv) {
    const sanitizedEnv = { ...sourceEnv };
    const pathKey = Object.keys(sanitizedEnv).find((key) => key.toLowerCase() === 'path') || 'PATH';
    const pathValue = sanitizedEnv[pathKey];
    const extensionNodeBin = path.resolve(extensionRoot, 'node_modules', '.bin');

    if (typeof pathValue !== 'string' || pathValue.length === 0) {
        return sanitizedEnv;
    }

    sanitizedEnv[pathKey] = pathValue
        .split(path.delimiter)
        .filter((entry) => !pathsEqual(entry, extensionNodeBin))
        .join(path.delimiter);
    return sanitizedEnv;
}

/** PATH 过滤在 Windows 上按大小写无关路径比较，其他平台保留区分。 */
function pathsEqual(left, right) {
    if (!left || !right) {
        return false;
    }

    const leftResolved = path.resolve(left);
    const rightResolved = path.resolve(right);
    return process.platform === 'win32'
        ? leftResolved.toLowerCase() === rightResolved.toLowerCase()
        : leftResolved === rightResolved;
}

/** 只在现有缓存可复用时沿用构建目录，否则以 Ninja 重新配置原生目标。
 * @note buildDir 被视为可丢弃的专用目录，生成器不符时会递归清除。
 */
function ensureBuildDirectory(env) {
    const cachePath = path.join(buildDir, 'CMakeCache.txt');
    if (fs.existsSync(cachePath)) {
        const cacheText = fs.readFileSync(cachePath, 'utf8');
        // BUG: 仅凭生成器复用缓存；先以 Ninja Debug 配置、后将
        // ZR_NATIVE_BUILD_CONFIG 改为 Release 时不会重配 CMAKE_BUILD_TYPE，
        // 后续校验可接受仍为 Debug 的二进制产物。
        if (cacheText.includes('CMAKE_GENERATOR:INTERNAL=Ninja')) {
            return;
        }

        // BUG: ZR_NATIVE_BUILD_DIR 可指向仓库根等任意目录；若该处有非 Ninja 缓存，
        // 此处将递归删除整个目录，而没有校验其是否为专用构建根。
        fs.rmSync(buildDir, { recursive: true, force: true });
    }

    fs.mkdirSync(buildDir, { recursive: true });
    const configureArgs = ['-S', repositoryRoot, '-B', buildDir];

    configureArgs.push('-G', 'Ninja', `-DCMAKE_BUILD_TYPE=${buildConfig}`);

    configureArgs.push(
        '-DBUILD_TESTS=OFF',
        '-DBUILD_WASM=OFF',
        '-DBUILD_LANGUAGE_SERVER_EXTENSION=OFF',
    );

    const configureResult = spawnSync('cmake', configureArgs, {
        cwd: repositoryRoot,
        stdio: 'inherit',
        env,
    });
    if (configureResult.status !== 0) {
        process.exit(configureResult.status ?? 1);
    }
}

/** 先尝试增量构建与双入口 smoke，只有产物校验失败才清理并重建一次。 */
function buildAndVerifyNativeAssets(env) {
    let verification = runConfiguredBuildAndVerify(env);
    if (verification.ok) {
        return;
    }

    console.warn(`Native asset verification failed after incremental build.\n${verification.message}`);
    console.warn(`Rebuilding ${buildDir} from a clean tree.`);
    // BUG: 验证失败时也会直接递归删除 ZR_NATIVE_BUILD_DIR；若调用方将其设为
    // 仓库根等非专用目录，清理范围会超出原生构建产物。
    fs.rmSync(buildDir, { recursive: true, force: true });

    verification = runConfiguredBuildAndVerify(env);
    if (!verification.ok) {
        throw new Error(`Native asset verification failed after clean rebuild.\n${verification.message}`);
    }
}

/** 把配置、构建和运行校验作为一次可重试单元。 */
function runConfiguredBuildAndVerify(env) {
    ensureBuildDirectory(env);
    runBuild(env);
    return verifyNativeAssets(env);
}

/** 按选定目标与并行度调用 CMake；构建命令失败立即终止脚本。 */
function runBuild(env) {
    const args = ['--build', buildDir];
    if (process.platform === 'win32') {
        args.push('--config', buildConfig);
    }
    if (targets.length > 0) {
        args.push('--target', ...targets);
    }
    args.push('--parallel', jobs);

    const result = spawnSync('cmake', args, {
        cwd: repositoryRoot,
        stdio: 'inherit',
        env,
    });

    if (result.status !== 0) {
        process.exit(result.status ?? 1);
    }
}

/** 在同步到扩展之前确认 CLI 与 stdio 服务端都能从同一构建根启动。 */
function verifyNativeAssets(env) {
    const cliExecutable = path.join(buildDir, 'bin', process.platform === 'win32' ? 'zr_vm_cli.exe' : 'zr_vm_cli');
    const lspExecutable = path.join(
        buildDir,
        'bin',
        process.platform === 'win32' ? 'zr_vm_language_server_stdio.exe' : 'zr_vm_language_server_stdio',
    );

    if (!fs.existsSync(cliExecutable)) {
        return {
            ok: false,
            message: `Missing zr_vm_cli at ${cliExecutable}`,
        };
    }
    if (!fs.existsSync(lspExecutable)) {
        return {
            ok: false,
            message: `Missing zr_vm_language_server_stdio at ${lspExecutable}`,
        };
    }

    const cliVerification = verifyCliSmoke(cliExecutable, env);
    if (!cliVerification.ok) {
        return cliVerification;
    }

    return verifyLanguageServerSmoke(lspExecutable, env);
}

/** CLI 的最小启动探针检查帮助输出，优先暴露加载器或链接失败。 */
function verifyCliSmoke(cliExecutable, env) {
    const helpResult = spawnSync(cliExecutable, ['--help'], {
        cwd: repositoryRoot,
        env,
        encoding: 'utf8',
        timeout: 5000,
        windowsHide: true,
    });
    if (helpResult.status !== 0 || !String(helpResult.stdout || '').includes('Usage:')) {
        return {
            ok: false,
            message: formatSpawnFailure('zr_vm_cli --help', helpResult),
        };
    }

    return { ok: true, message: '' };
}

/** 将子进程退出状态及输出聚合为构建阶段可读的失败原因。 */
function formatSpawnFailure(label, result) {
    const stdout = String(result.stdout || '');
    const stderr = String(result.stderr || '');
    return [
        `${label} failed.`,
        `status=${result.status ?? 'null'}`,
        `signal=${result.signal ?? 'null'}`,
        `error=${result.error instanceof Error ? result.error.message : 'none'}`,
        `stdout:\n${stdout}`,
        `stderr:\n${stderr}`,
    ].join('\n');
}

/** 发送初始化和扩展专用请求，防止可启动但缺少关键协议能力的服务端被打包。 */
function verifyLanguageServerSmoke(lspExecutable, env) {
    const requests = [
        { jsonrpc: '2.0', id: 1, method: 'initialize', params: { capabilities: {} } },
        {
            jsonrpc: '2.0',
            id: 2,
            method: 'zr/projectModules',
            params: {
                uri: toFileUri(projectModulesFixture),
            },
        },
        {
            jsonrpc: '2.0',
            id: 3,
            method: 'zr/nativeDeclarationDocument',
            params: {
                uri: 'zr-decompiled:/zr.system.zr',
            },
        },
        { jsonrpc: '2.0', id: 4, method: 'shutdown', params: {} },
        { jsonrpc: '2.0', method: 'exit', params: {} },
    ];
    const input = Buffer.from(requests.map(serializeJsonRpcFrame).join(''), 'utf8');
    const result = spawnSync(lspExecutable, [], {
        cwd: repositoryRoot,
        env,
        input,
        encoding: 'utf8',
        timeout: 10000,
        windowsHide: true,
    });

    if (result.status !== 0) {
        return {
            ok: false,
            message: formatSpawnFailure('zr_vm_language_server_stdio custom request smoke', result),
        };
    }

    let responses;
    try {
        responses = parseJsonRpcFrames(String(result.stdout || ''));
    } catch (error) {
        return {
            ok: false,
            message: [
                'zr_vm_language_server_stdio custom request smoke produced invalid output.',
                `error=${error instanceof Error ? error.message : String(error)}`,
                `stdout:\n${String(result.stdout || '')}`,
                `stderr:\n${String(result.stderr || '')}`,
            ].join('\n'),
        };
    }

    const projectModulesResponse = responses.find((message) => message.id === 2);
    const nativeDeclarationResponse = responses.find((message) => message.id === 3);
    // BUG: 仅检查 method-not-found；进程以 0 退出但不发送 id=2/id=3 响应时，
    // 两个查找结果都是 undefined，仍返回 ok，缺失专用请求的服务端会通过发布校验。
    if (projectModulesResponse?.error?.code === -32601 || nativeDeclarationResponse?.error?.code === -32601) {
        return {
            ok: false,
            message: [
                'zr_vm_language_server_stdio is missing required custom requests.',
                `stdout:\n${String(result.stdout || '')}`,
                `stderr:\n${String(result.stderr || '')}`,
            ].join('\n'),
        };
    }

    return { ok: true, message: '' };
}

/** JSON-RPC 输入帧的 Content-Length 按 UTF-8 字节计数，与服务端 stdio 契约一致。 */
function serializeJsonRpcFrame(payload) {
    const text = JSON.stringify(payload);
    return `Content-Length: ${Buffer.byteLength(text, 'utf8')}\r\n\r\n${text}`;
}

/** 将 smoke 的 stdout 解析为响应序列，供专用请求校验查找 ID。 */
function parseJsonRpcFrames(text) {
    const responses = [];
    let offset = 0;

    while (offset < text.length) {
        const headerEnd = text.indexOf('\r\n\r\n', offset);
        if (headerEnd < 0) {
            break;
        }

        const header = text.slice(offset, headerEnd);
        const match = /Content-Length:\s*(\d+)/i.exec(header);
        if (!match) {
            throw new Error(`Missing Content-Length header near offset ${offset}`);
        }

        const bodyLength = Number.parseInt(match[1], 10);
        const bodyStart = headerEnd + 4;
        // BUG: Content-Length 是字节数，text.length/slice 按 UTF-16 码元计数；
        // 响应含非 ASCII 文本且后接其他帧时会错位，误判有效服务端输出为损坏。
        const bodyEnd = bodyStart + bodyLength;
        const body = text.slice(bodyStart, bodyEnd);

        responses.push(JSON.parse(body));
        offset = bodyEnd;
    }

    return responses;
}

/** 把固定工程路径交给 zr/projectModules 请求。
 * TODO: 当前只归一化分隔符，仓库路径含空格或 # 时未做 URI 转义；
 * 核查服务端 URI 解析后改用标准 URL 构造。
 */
function toFileUri(filePath) {
    const normalized = filePath.replace(/\\/g, '/');
    return normalized.startsWith('/')
        ? `file://${normalized}`
        : `file:///${normalized}`;
}
