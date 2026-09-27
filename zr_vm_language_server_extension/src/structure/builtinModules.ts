/** 静态库目录的展示分类，独立于语言服务器更细的声明种类。 */
export type BuiltinSymbolKind = 'constant' | 'function' | 'type';

/** 可见符号快照供内置库树生成叶节点，不承担类型检查或调用提示。 */
export interface BuiltinSymbolSnapshot {
    name: string;
    kind: BuiltinSymbolKind;
    detail?: string;
}

/** 汇总根模块的子模块链接；目前链接仅展示名称，不承载导航命令。 */
export interface BuiltinModuleLinkSnapshot {
    name: string;
    moduleName: string;
    detail?: string;
}

/** Web 和桌面共用的静态展示记录；模块是否真正可用仍由运行时决定。 */
export interface BuiltinModuleSnapshot {
    moduleName: string;
    detail?: string;
    modules?: BuiltinModuleLinkSnapshot[];
    symbols?: BuiltinSymbolSnapshot[];
}

// 目录只提供离线可见的摘要，不是运行时注册表或语言服务器的导出清单。
// TODO: 官方清单还含 zr.builtin、zr.container、zr.network.tcp 等模块；确认视图
// 是否应完整枚举可用模块，并建立与 native_binding_official_inventory.c 的校验。
const BUILTIN_MODULES: Record<string, BuiltinModuleSnapshot> = {
    'zr.system': {
        moduleName: 'zr.system',
        detail: 'System native module root that aggregates leaf submodules.',
        modules: [
            { name: 'console', moduleName: 'zr.system.console', detail: 'Console output helpers.' },
            { name: 'fs', moduleName: 'zr.system.fs', detail: 'Filesystem helpers.' },
            { name: 'env', moduleName: 'zr.system.env', detail: 'Environment helpers.' },
            { name: 'process', moduleName: 'zr.system.process', detail: 'Process helpers.' },
            { name: 'gc', moduleName: 'zr.system.gc', detail: 'Garbage-collection controls.' },
            { name: 'exception', moduleName: 'zr.system.exception', detail: 'Exception hierarchy and global hooks.' },
            { name: 'vm', moduleName: 'zr.system.vm', detail: 'VM inspection and module invocation helpers.' },
        ],
    },
    'zr.system.console': {
        moduleName: 'zr.system.console',
        symbols: [
            { name: 'print', kind: 'function' },
            { name: 'printLine', kind: 'function' },
            { name: 'printError', kind: 'function' },
            { name: 'printErrorLine', kind: 'function' },
            { name: 'read', kind: 'function' },
            { name: 'readLine', kind: 'function' },
        ],
    },
    'zr.system.fs': {
        moduleName: 'zr.system.fs',
        symbols: [
            { name: 'currentDirectory', kind: 'function' },
            { name: 'changeCurrentDirectory', kind: 'function' },
            { name: 'pathExists', kind: 'function' },
            { name: 'isFile', kind: 'function' },
            { name: 'isDirectory', kind: 'function' },
            { name: 'createDirectory', kind: 'function' },
            { name: 'createDirectories', kind: 'function' },
            { name: 'removePath', kind: 'function' },
            { name: 'readText', kind: 'function' },
            { name: 'writeText', kind: 'function' },
            { name: 'appendText', kind: 'function' },
            { name: 'getInfo', kind: 'function' },
            { name: 'SystemFileInfo', kind: 'type' },
            { name: 'FileSystemEntry', kind: 'type' },
            { name: 'File', kind: 'type' },
            { name: 'Folder', kind: 'type' },
            { name: 'IStreamReader', kind: 'type' },
            { name: 'IStreamWriter', kind: 'type' },
            { name: 'FileStream', kind: 'type' },
        ],
    },
    'zr.system.env': {
        moduleName: 'zr.system.env',
        symbols: [
            { name: 'getVariable', kind: 'function' },
        ],
    },
    'zr.system.process': {
        moduleName: 'zr.system.process',
        symbols: [
            { name: 'arguments', kind: 'constant' },
            { name: 'sleepMilliseconds', kind: 'function' },
            { name: 'exit', kind: 'function' },
        ],
    },
    'zr.system.gc': {
        moduleName: 'zr.system.gc',
        symbols: [
            { name: 'start', kind: 'function' },
            { name: 'stop', kind: 'function' },
            { name: 'step', kind: 'function' },
            { name: 'collect', kind: 'function' },
        ],
    },
    'zr.system.exception': {
        moduleName: 'zr.system.exception',
        symbols: [
            { name: 'registerUnhandledException', kind: 'function' },
            { name: 'Error', kind: 'type' },
            { name: 'StackFrame', kind: 'type' },
            { name: 'RuntimeError', kind: 'type' },
            { name: 'IOException', kind: 'type' },
            { name: 'TypeError', kind: 'type' },
            { name: 'MemoryError', kind: 'type' },
            { name: 'ExceptionError', kind: 'type' },
        ],
    },
    'zr.system.vm': {
        moduleName: 'zr.system.vm',
        symbols: [
            { name: 'loadedModules', kind: 'function' },
            { name: 'state', kind: 'function' },
            { name: 'callModuleExport', kind: 'function' },
            { name: 'SystemVmState', kind: 'type' },
            { name: 'SystemLoadedModuleInfo', kind: 'type' },
        ],
    },
    'zr.math': {
        moduleName: 'zr.math',
        detail: 'Built-in numeric algorithms, vector and matrix types, complex values, quaternions and tensors.',
        symbols: [
            { name: 'PI', kind: 'constant' },
            { name: 'TAU', kind: 'constant' },
            { name: 'E', kind: 'constant' },
            { name: 'EPSILON', kind: 'constant' },
            { name: 'INF', kind: 'constant' },
            { name: 'NAN', kind: 'constant' },
            { name: 'abs', kind: 'function' },
            { name: 'invokeCallback', kind: 'function' },
            { name: 'Vector2', kind: 'type' },
            { name: 'Vector3', kind: 'type' },
            { name: 'Vector4', kind: 'type' },
            { name: 'Quaternion', kind: 'type' },
            { name: 'Complex', kind: 'type' },
            { name: 'Matrix3x3', kind: 'type' },
            { name: 'Matrix4x4', kind: 'type' },
            { name: 'Tensor', kind: 'type' },
        ],
    },
    'zr.network': {
        moduleName: 'zr.network',
        detail: 'Network native module root that aggregates TCP and UDP leaf modules.',
        modules: [
            { name: 'tcp', moduleName: 'zr.network.tcp', detail: 'TCP client and server primitives.' },
            { name: 'udp', moduleName: 'zr.network.udp', detail: 'UDP datagram primitives.' },
        ],
    },
};

/** 供测试和目录消费者按名读取记录；未知模块预期返回 undefined。 */
export function getBuiltinModuleSnapshot(moduleName: string): BuiltinModuleSnapshot | undefined {
    // BUG: 普通对象继承原型键；传入 `toString` 返回函数，传入 `__proto__`
    // 返回对象原型，均不符合 BuiltinModuleSnapshot | undefined 契约。
    return BUILTIN_MODULES[moduleName];
}

/** 返回按模块名排序的目录快照，让树视图和测试获得稳定展示顺序。 */
export function listBuiltinModuleSnapshots(): BuiltinModuleSnapshot[] {
    return Object.values(BUILTIN_MODULES).sort((left, right) =>
        left.moduleName.localeCompare(right.moduleName),
    );
}
