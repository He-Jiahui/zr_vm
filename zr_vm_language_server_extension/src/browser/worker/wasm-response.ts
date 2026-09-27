import { ErrorCodes, ResponseError } from 'vscode-languageserver/browser';
import type { WasmResponse } from './wasm-bridge';

/**
 * 把 WASM JSON 封装转换成 worker 的 LSP 成功值或 ResponseError。
 * fallback 只用于合法 success 的 null 值，不能吞掉后端错误或畸形封装。
 */
export function responseData<T>(response: WasmResponse<T>, fallback: T): T {
    if (typeof response !== 'object' || response === null || typeof response.success !== 'boolean') {
        throw new ResponseError(ErrorCodes.InternalError, 'Malformed WASM response envelope.');
    }
    if (response.success === false) {
        const code = typeof response.code === 'number' && Number.isInteger(response.code)
            ? response.code : ErrorCodes.InternalError;
        const message = typeof response.error === 'string' && response.error.length > 0
            ? response.error : 'Malformed WASM error response.';
        throw new ResponseError(code, message, response.data);
    }
    if (!Object.prototype.hasOwnProperty.call(response, 'data') || response.data === undefined ||
        'error' in response || 'code' in response) {
        throw new ResponseError(ErrorCodes.InternalError, 'Malformed WASM success response.');
    }
    // TODO: 泛型断言不校验 data 的业务结构；需逐个核对 WASM export 的成功载荷及消费点。
    return response.data ?? fallback;
}
