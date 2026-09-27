import * as path from 'node:path';
import { spawn, type ChildProcessByStdio } from 'node:child_process';
import type { Readable } from 'node:stream';
import type { ZrLaunchRequestArguments } from './types';

/** CLI 输出分别送到 DAP stdout/stderr 事件，供 VS Code 调试控制台展示。 */
type OutputChannelName = 'stdout' | 'stderr';

/** CLI 启动后打印的实际监听端点；DAP 适配器用它连接 zrdbg/1。 */
export type ZrCliLaunchResult = {
    endpoint: string;
};

/** 持有一次本地调试启动的子进程及诊断输出；与一个 DAP 会话同寿命。 */
export class ZrCliLauncher {
    private child: ChildProcessByStdio<null, Readable, Readable> | undefined;
    private readonly exitListeners: Array<(code: number | null) => void> = [];
    // TODO: 端点识别后仍持续累积完整 stdout/stderr；长时间运行且持续输出的项目会
    // 增长会话内存。需以持续输出的调试会话测量后改为有界诊断尾部缓冲。
    private stdoutBuffer = '';
    private stderrBuffer = '';

    /** adapter 注入输出转发器，子进程日志由同一 DAP 会话呈现。 */
    constructor(private readonly onOutput: (channel: OutputChannelName, text: string) => void) {}

    /** DAP 适配器订阅 CLI 退出，以发送 exited/terminated；监听应在 launch 前注册。 */
    onExit(listener: (code: number | null) => void): void {
        this.exitListeners.push(listener);
    }

    /** 以暂停入口模式启动 CLI，等候其发布端点后交给 DAP 适配器连接。
     * @throws CLI 路径缺失、启动失败、提前退出或 15 秒内未发布端点。
     */
    async launch(config: ZrLaunchRequestArguments): Promise<ZrCliLaunchResult> {
        const cliPath = config.cliPath?.trim();
        if (!cliPath) {
            throw new Error('Missing zr_vm_cli path.');
        }

        const projectPath = config.project;
        const workingDirectory = config.cwd && config.cwd.length > 0
            ? config.cwd
            : path.dirname(projectPath);
        const debugAddress = config.debugAddress && config.debugAddress.length > 0
            ? config.debugAddress
            : '127.0.0.1:0';
        const executionMode = config.executionMode ?? 'interp';
        // BUG: launch.json 的 args 若包含 CLI 的 `--` 程序参数分隔符，后续追加的
        // --debug 系列标志会被 CLI 当作程序参数；调试端点不启动，随后提前退出或超时。
        const args = [
            projectPath,
            '--execution-mode',
            executionMode,
            ...(Array.isArray(config.args) ? config.args : []),
            '--debug',
            '--debug-address',
            debugAddress,
            '--debug-wait',
            '--debug-print-endpoint',
        ];

        const child = spawn(cliPath, args, {
            cwd: workingDirectory,
            stdio: ['ignore', 'pipe', 'pipe'],
        });
        this.child = child;

        // 同一 stdout 流同时服务启动握手和 DAP 控制台；连接完成后仍向用户转发输出。
        child.stdout.setEncoding('utf8');
        child.stderr.setEncoding('utf8');
        child.stdout.on('data', (chunk: string) => {
            this.stdoutBuffer += chunk;
            this.onOutput('stdout', chunk);
        });
        child.stderr.on('data', (chunk: string) => {
            this.stderrBuffer += chunk;
            this.onOutput('stderr', chunk);
        });
        child.on('exit', (code) => {
            for (const listener of this.exitListeners) {
                listener(code);
            }
        });

        /** 子进程退出、spawn 错误、端点到达与超时都只结算一次启动结果。 */
        return await new Promise<ZrCliLaunchResult>((resolve, reject) => {
            let settled = false;
            // BUG: 端点超时只拒绝 DAP 启动，未停止仍由本对象持有的 CLI；
            // 若 CLI 随后进入 --debug-wait，它会在失败会话结束后继续等待连接。
            const timeoutHandle = setTimeout(() => {
                if (settled) {
                    return;
                }

                settled = true;
                reject(new Error(this.formatStartupFailure('Timed out waiting for debug_endpoint=')));
            }, 15000);
            /** 清理启动阶段定时器；不负责终止子进程或释放会话。 */
            const finish = (error?: Error, result?: ZrCliLaunchResult) => {
                if (settled) {
                    return;
                }
                settled = true;
                clearTimeout(timeoutHandle);
                if (error) {
                    reject(error);
                    return;
                }
                resolve(result!);
            };

            // 缓冲更新监听先注册，端点监听随后读取该次 data 的累积结果。
            child.stdout.on('data', () => {
                const endpoint = this.extractEndpoint();
                if (endpoint) {
                    finish(undefined, { endpoint });
                }
            });
            child.on('error', (error) => {
                finish(error);
            });
            child.on('exit', (code) => {
                if (this.extractEndpoint()) {
                    return;
                }

                finish(new Error(this.formatStartupFailure(`zr_vm_cli exited before endpoint was available (code=${code})`)));
            });
        });
    }

    /** DAP disconnect/terminate 时停止本会话启动的 CLI，并等待进程退出。 */
    async stop(): Promise<void> {
        if (!this.child || this.child.killed) {
            return;
        }

        await new Promise<void>((resolve) => {
            const child = this.child!;
            let finished = false;
            const timeoutHandle = setTimeout(() => {
                if (!finished) {
                    child.kill();
                }
            }, 1500);
            /** 进程退出后释放 stop 等待；重复 exit/超时回调只完成一次。 */
            const complete = () => {
                if (finished) {
                    return;
                }
                finished = true;
                clearTimeout(timeoutHandle);
                resolve();
            };

            // TODO: 若子进程此前被信号结束，exitCode 仍为 null 而 exit 已发出；
            // 需核查 signalCode 分支，避免 stop 永远等待不会再来的 exit 事件。
            child.once('exit', complete);
            child.kill();
            if (child.exitCode !== null) {
                complete();
            }
        });
    }

    /** 只从 CLI 启动协议的 stdout 标记读取端点，供 adapter 连接同一进程。 */
    private extractEndpoint(): string | undefined {
        // BUG: data 事件可在端点行结束前截断；若首块只有 debug_endpoint=127.0.0.1:12，
        // 正则立即接受端口 12，后续的 345 才到也无法更正已返回的连接目标。
        const match = this.stdoutBuffer.match(/debug_endpoint=([^\r\n]+)/);
        return match ? match[1].trim() : undefined;
    }

    /** 把启动前的两路输出附在错误后，帮助定位 CLI 参数或环境失败。 */
    private formatStartupFailure(reason: string): string {
        return `${reason}\nstdout:\n${this.stdoutBuffer}\nstderr:\n${this.stderrBuffer}`;
    }
}
