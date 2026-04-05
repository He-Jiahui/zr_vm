import * as path from 'node:path';
import { spawn, type ChildProcessByStdio } from 'node:child_process';
import type { Readable } from 'node:stream';
import type { ZrLaunchRequestArguments } from './types';

type OutputChannelName = 'stdout' | 'stderr';

export type ZrCliLaunchResult = {
    endpoint: string;
};

export class ZrCliLauncher {
    private child: ChildProcessByStdio<null, Readable, Readable> | undefined;
    private readonly exitListeners: Array<(code: number | null) => void> = [];
    private stdoutBuffer = '';
    private stderrBuffer = '';

    constructor(private readonly onOutput: (channel: OutputChannelName, text: string) => void) {}

    onExit(listener: (code: number | null) => void): void {
        this.exitListeners.push(listener);
    }

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

        return await new Promise<ZrCliLaunchResult>((resolve, reject) => {
            let settled = false;
            const timeoutHandle = setTimeout(() => {
                if (settled) {
                    return;
                }

                settled = true;
                reject(new Error(this.formatStartupFailure('Timed out waiting for debug_endpoint=')));
            }, 15000);
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
            const complete = () => {
                if (finished) {
                    return;
                }
                finished = true;
                clearTimeout(timeoutHandle);
                resolve();
            };

            child.once('exit', complete);
            child.kill();
            if (child.exitCode !== null) {
                complete();
            }
        });
    }

    private extractEndpoint(): string | undefined {
        const match = this.stdoutBuffer.match(/debug_endpoint=([^\r\n]+)/);
        return match ? match[1].trim() : undefined;
    }

    private formatStartupFailure(reason: string): string {
        return `${reason}\nstdout:\n${this.stdoutBuffer}\nstderr:\n${this.stderrBuffer}`;
    }
}
