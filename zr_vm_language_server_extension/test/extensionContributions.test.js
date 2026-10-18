const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

// 从待发布的扩展清单读取贡献项，验证用户可见入口与命令实现约定的 ID 保持一致。
function readPackageJson() {
    return JSON.parse(fs.readFileSync(path.resolve(__dirname, '..', 'package.json'), 'utf8'));
}

// 防止桌面或 Web 虽已实现处理器，却因清单遗漏激活事件、命令或 Zr 编辑器菜单而无法触达。
// 命令执行和编辑结果由两端的 smoke 测试验证；此处专门约束静态贡献契约。
test('import cleanup commands are exposed through activation, commands, and editor context menu', () => {
    const manifest = readPackageJson();
    const commands = manifest.contributes?.commands ?? [];
    const editorContext = manifest.contributes?.menus?.['editor/context'] ?? [];

    assert(manifest.activationEvents.includes('onCommand:zr.organizeImports'));
    assert(manifest.activationEvents.includes('onCommand:zr.removeUnusedImports'));
    assert(commands.some((entry) =>
        entry.command === 'zr.organizeImports' &&
        entry.title === 'Organize Imports' &&
        entry.category === 'Zr'));
    assert(commands.some((entry) =>
        entry.command === 'zr.removeUnusedImports' &&
        entry.title === 'Remove Unused Imports' &&
        entry.category === 'Zr'));
    assert(editorContext.some((entry) =>
        entry.command === 'zr.organizeImports' &&
        entry.when === 'editorLangId == zr'));
    assert(editorContext.some((entry) =>
        entry.command === 'zr.removeUnusedImports' &&
        entry.when === 'editorLangId == zr'));
});

test('native tools require workspace trust and executable settings stay restricted', () => {
    const manifest = readPackageJson();
    assert.equal(manifest.capabilities?.untrustedWorkspaces?.supported, false);
    for (const setting of ['zr.executablePath', 'zr.debug.cli.path', 'zr.languageServer.native.path']) {
        assert.equal(manifest.contributes.configuration.properties[setting].restricted, true, setting);
    }
});

/**
 * @function readPackageJson
 *
 * ## Summary
 * 为本文件的两个同步测试读取扩展目录当前的 package.json，使静态贡献检查使用磁盘清单，而不是 require 的 JSON 模块缓存。
 *
 * ## Returns
 * @returns {*} JSON.parse 的新结果。当前清单为对象；本函数不检查形状。读取或解析异常向测试回调传播，失败时不返回替代清单。
 *
 * ## Constraints and lifecycle
 * 按 __dirname 定位文件，不依赖启动目录。每次调用独立同步读取，不提供两次读取之间的原子快照，也不加载 VS Code 或执行命令。readFileSync 不把打开的文件句柄交给调用者；结果由 JavaScript 内存管理。
 *
 * ## References
 * - zr_vm_language_server_extension/test/extensionContributions.test.js:8
 * - zr_vm_language_server_extension/test/extensionContributions.test.js:14
 * - zr_vm_language_server_extension/test/extensionContributions.test.js:37
 * - zr_vm_language_server_extension/package.json:412
 */

/**
 * @unit extensionContributions::import_cleanup_manifest
 *
 * ## Summary
 * 防止两条导入清理命令遗漏清单激活字符串、展示字段或编辑器上下文菜单声明。桌面和 Web 处理器注册、真实命令编辑效果由各自入口和 smoke 场景承担，本测试只比较清单值。
 *
 * ## Returns
 * 同步完成时返回 undefined；断言失败或清单读取、形状访问异常会终止本次测试回调。后面的断言可能尚未到达，不能按源码断言数报告运行结果。
 *
 * ## Constraints and lifecycle
 * 对命令和菜单各做存在性检查，允许额外项和重复项；when 只按精确字符串比较，不求值或证明界面显示。仅命令及菜单缺省为 []，activationEvents 和条目形状没有同等容错。同步借用本次读取的局部清单，不创建夹具或持久引用。该回调无 failfast 配置，失败不等于其余注册测试均停止。
 *
 * ## References
 * - zr_vm_language_server_extension/test/extensionContributions.test.js:13
 * - zr_vm_language_server_extension/test/extensionContributions.test.js:18
 * - zr_vm_language_server_extension/test/extensionContributions.test.js:20
 * - zr_vm_language_server_extension/test/extensionContributions.test.js:28
 * - zr_vm_language_server_extension/src/organizeImports.ts:173
 * - zr_vm_language_server_extension/test/smoke/smokeSuite.js:528
 * - zr_vm_language_server_extension/test/smoke/browserRunner.js:552
 */

/**
 * @unit extensionContributions::workspace_trust_manifest
 *
 * ## Summary
 * 检查清单继续声明不支持未信任工作区，并将 zr.executablePath、zr.debug.cli.path、zr.languageServer.native.path 三项标为 restricted，防止发布元数据回退。
 *
 * ## Returns
 * 同步完成时返回 undefined；比较失败抛 AssertionError，缺失的配置对象或属性访问也可能抛普通异常。首个异常会中断本次回调，未到达的键不算已检查。
 *
 * ## Constraints and lifecycle
 * 本测试独立重读清单，不启动原生工具、不加载扩展，也不验证 VS Code 实际执行工作区信任或设置限制。它只约束上述声明和值，不证明其他设置或运行时安全行为。局部 JSON 值由 JavaScript 管理，没有显式资源或夹具清理阶段。
 *
 * ## References
 * - zr_vm_language_server_extension/test/extensionContributions.test.js:36
 * - zr_vm_language_server_extension/test/extensionContributions.test.js:38
 * - zr_vm_language_server_extension/test/extensionContributions.test.js:39
 * - zr_vm_language_server_extension/test/extensionContributions.test.js:40
 * - zr_vm_language_server_extension/package.json:351
 * - zr_vm_language_server_extension/package.json:382
 * - zr_vm_language_server_extension/package.json:388
 * - zr_vm_language_server_extension/package.json:435
 */
