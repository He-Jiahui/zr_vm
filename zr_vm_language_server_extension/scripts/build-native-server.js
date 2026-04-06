const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');

const repositoryRoot = path.resolve(__dirname, '..', '..');
const buildDir = process.env.ZR_NATIVE_BUILD_DIR
    ? path.resolve(process.env.ZR_NATIVE_BUILD_DIR)
    : path.join(repositoryRoot, 'build', 'codex-lsp');
const buildConfig = process.env.ZR_NATIVE_BUILD_CONFIG || 'Debug';
const targets = (process.env.ZR_NATIVE_BUILD_TARGET || 'zr_vm_language_server_stdio,zr_vm_cli_executable')
    .split(',')
    .map((value) => value.trim())
    .filter((value) => value.length > 0);
const jobs = process.env.ZR_BUILD_JOBS || '8';
const helloWorldProject = path.join(repositoryRoot, 'tests', 'fixtures', 'projects', 'hello_world', 'hello_world.zrp');

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

const env = process.platform === 'win32' && !process.env.VSCMD_VER
    ? importVsDevCmdEnvironment()
    : process.env;

try {
    buildAndVerifyNativeAssets(env);
} catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    process.exit(1);
}

function ensureBuildDirectory(env) {
    const cachePath = path.join(buildDir, 'CMakeCache.txt');
    if (fs.existsSync(cachePath)) {
        const cacheText = fs.readFileSync(cachePath, 'utf8');
        if (cacheText.includes('CMAKE_GENERATOR:INTERNAL=Ninja')) {
            return;
        }

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

function buildAndVerifyNativeAssets(env) {
    let verification = runConfiguredBuildAndVerify(env);
    if (verification.ok) {
        return;
    }

    console.warn(`Native asset verification failed after incremental build.\n${verification.message}`);
    console.warn(`Rebuilding ${buildDir} from a clean tree.`);
    fs.rmSync(buildDir, { recursive: true, force: true });

    verification = runConfiguredBuildAndVerify(env);
    if (!verification.ok) {
        throw new Error(`Native asset verification failed after clean rebuild.\n${verification.message}`);
    }
}

function runConfiguredBuildAndVerify(env) {
    ensureBuildDirectory(env);
    runBuild(env);
    return verifyNativeAssets(env);
}

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

    return verifyCliSmoke(cliExecutable, env);
}

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

    if (!fs.existsSync(helloWorldProject)) {
        return {
            ok: false,
            message: `Missing hello_world project fixture at ${helloWorldProject}`,
        };
    }

    const runResult = spawnSync(
        cliExecutable,
        [
            helloWorldProject,
            '--debug',
            '--debug-address',
            '127.0.0.1:0',
            '--debug-print-endpoint',
        ],
        {
            cwd: path.dirname(helloWorldProject),
            env,
            encoding: 'utf8',
            timeout: 10000,
            windowsHide: true,
        },
    );
    const stdout = String(runResult.stdout || '');
    if (runResult.status !== 0 ||
        !stdout.includes('debug_endpoint=') ||
        !stdout.includes('hello world')) {
        return {
            ok: false,
            message: formatSpawnFailure('zr_vm_cli hello_world.zrp --debug --debug-print-endpoint', runResult),
        };
    }

    return { ok: true, message: '' };
}

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
