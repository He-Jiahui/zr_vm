import type * as vscode from 'vscode';

/** VS Code 交给内联适配器的 DAP 请求；arguments 在命令分派后才按具体协议解析。 */
export type DapRequest = vscode.DebugProtocolMessage & {
    seq: number;
    type: 'request';
    command: string;
    arguments?: Record<string, unknown>;
};

/** DAP 响应通过 request_seq 和 command 关联原请求，成功与失败共用此封套。 */
export type DapResponse = vscode.DebugProtocolMessage & {
    seq: number;
    type: 'response';
    request_seq: number;
    success: boolean;
    command: string;
    body?: unknown;
    message?: string;
};

/** 适配器主动发送的 DAP 事件，和 request/response 共用顺序序号。 */
export type DapEvent = vscode.DebugProtocolMessage & {
    seq: number;
    type: 'event';
    event: string;
    body?: unknown;
};

/** launch.json 的本地项目启动参数；provider 补齐路径，launcher 消费 CLI 相关字段。 */
export interface ZrLaunchRequestArguments {
    project: string;
    cwd?: string;
    executionMode?: 'interp' | 'binary';
    cliPath?: string;
    args?: string[];
    debugAddress?: string;
    stopOnEntry?: boolean;
    authToken?: string;
}

/** attach 仅连接现存端点，不拥有外部 CLI 进程。 */
export interface ZrAttachRequestArguments {
    endpoint: string;
    authToken?: string;
}

/** zrdbg/1 主动推送的运行时事件；adapter 将其映射为 DAP 事件。 */
export interface ZrDbgEventMessage {
    method: string;
    params?: Record<string, unknown>;
}

/** zrdbg/1 的 JSON RPC 响应；id 用于匹配在途请求，error 表示调用失败。 */
export interface ZrDbgResponseMessage {
    id: number;
    result?: Record<string, unknown>;
    error?: {
        code: number;
        message: string;
    };
}

/** 运行时断点解析结果；adapter 用 verified 和 line 更新 VS Code 的显示状态。 */
export interface ZrDbgBreakpoint {
    verified?: boolean;
    line?: number;
    functionName?: string;
    instructionIndex?: number;
}

/** 运行时变量记录；variablesReference 由 adapter 解释为可展开子对象句柄。 */
export interface ZrDbgVariable {
    name: string;
    type: string;
    value: string;
    variablesReference: number;
}

/** 接收者与普通变量共用值表示，stack frame 可额外标明调用接收者。 */
export interface ZrDbgReceiver extends ZrDbgVariable {}

/** zrdbg/1 栈帧到 DAP stackTrace 的边界类型，源码路径还需经项目清单映射。 */
export interface ZrDbgFrame {
    frameId: number;
    moduleName: string;
    functionName: string;
    sourceFile: string;
    line: number;
    instructionIndex: number;
    frameDepth?: number;
    callKind?: string;
    argumentCount?: number;
    returnSlot?: number;
    isExceptionFrame?: boolean;
    receiver?: ZrDbgReceiver;
    arguments?: ZrDbgVariable[];
}

/** 栈帧下的运行时作用域标识；adapter 把 scopeId 转为 DAP variablesReference。 */
export interface ZrDbgScope {
    scopeId: number;
    frameId: number;
    name: string;
}
