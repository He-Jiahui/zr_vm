/* Execute the complete actual inline page script, with browser boundary stubs.
 * No private viewer functions are exposed or duplicated. Input/output are JSON. */
"use strict";
const fs = require("node:fs");
const vm = require("node:vm");
const { TextDecoder } = require("node:util");
const input = JSON.parse(fs.readFileSync(0, "utf8"));
const elements = new Map();
for (const match of input.html.matchAll(/\bid="([^"]+)"/g)) {
  elements.set(match[1], {
    innerHTML: "", textContent: "", hidden: false, listeners: {},
    addEventListener(name, callback) { this.listeners[name] = callback; },
  });
}
const scripts = [...input.html.matchAll(/<script\b([^>]*)>([\s\S]*?)<\/script>/gi)];
const embedded = scripts.find(match => /\bid="zr-bench-embed"/.test(match[1]));
if (embedded) elements.get("zr-bench-embed").textContent = embedded[2];
const encodings = [];
class FileReader {
  readAsText(file, encoding) {
    encodings.push(encoding);
    this.result = file.text;
    this.onload();
  }
}
const context = vm.createContext({
  document: { getElementById: id => elements.get(id) || null },
  FileReader, TextDecoder, Uint8Array,
  // Browser atob returns one character per byte, not decoded Unicode.
  atob: text => Buffer.from(text, "base64").toString("latin1"),
});
for (const script of scripts) {
  if (/\btype="text\/plain"/.test(script[1])) continue;
  vm.runInContext(script[2], context, { timeout: 2000, filename: "actual-viewer-inline.js" });
}
function snapshot() {
  return Object.fromEntries([...elements].map(([id, el]) => [id, {
    html: el.innerHTML, text: el.textContent, hidden: el.hidden,
  }]));
}
const states = [snapshot()];
for (const text of input.files || []) {
  const callback = elements.get("file-input").listeners.change;
  if (typeof callback !== "function") throw new Error("actual file change listener missing");
  callback({ target: { files: [{ text }] } });
  states.push(snapshot());
}
process.stdout.write(JSON.stringify({ states, encodings }));

// @unit tests/benchmarks/benchmark_compare_viewer_harness.js::module::stdio_protocol [block]
//
// ## Summary
// 作为独立 CommonJS 测试进程，从标准输入读取一个 JSON 请求，在所有场景完成后向标准输出写一个 JSON 结果。
//
// ## Parameters
// - 输入对象：html 为受信页面字符串，files 若提供为要依次触发的文本序列；由 Python page 构造。
//
// ## Returns
// 没有模块导出 API 或函数返回值；正常完成输出含 states 和 encodings 的 JSON 文本，不附加换行。
// 启动/读取/JSON解析/VM或回调/序列化异常均未在此兜底，Node按异常退出，Python check=True传播失败。
//
// ## Constraints and lifecycle
// readFileSync(0) 同步等到 stdin EOF；没有内部读取超时、schema认证或输入/输出大小预算。
// 只借用标准流，不创建文件夹具或关闭调用方传入的标准句柄；内存对象属于本进程并随进程结束释放。
// 页面脚本及宿主替身共享一次请求的内存状态，不能据 vm 上下文把任意不可信页面视为安全可执行输入。
// 初始脚本的2秒限制不覆盖前置I/O或后续宿主回调；Python外层10秒只约束该子进程等待/通信。
//
// ## References
// - tests/benchmarks/benchmark_compare_viewer_harness.js:3
// - tests/benchmarks/benchmark_compare_viewer_harness.js:4
// - tests/benchmarks/benchmark_compare_viewer_harness.js:7
// - tests/benchmarks/benchmark_compare_viewer_harness.js:48
// - tests/benchmarks/test_benchmark_compare_viewer.py:57
// - tests/benchmarks/test_benchmark_compare_viewer.py:58
// - tests/benchmarks/test_benchmark_compare_viewer.py:61
// - tests/benchmarks/test_benchmark_compare_viewer.py:63

// @unit tests/benchmarks/benchmark_compare_viewer_harness.js::module::element_stub_population [block]
//
// ## Summary
// 按页面文本中的 id 捕获构建显示字段和事件监听器的最小元素替身。
//
// ## Constraints and lifecycle
// id 正则仅匹配非空、双引号的字面拼写，区分大小写；它可能在非真实属性位置命中，不能代替 HTML 解析。
// 同一 key 再出现时覆盖整个旧记录；所有元素初始 hidden=false，不从 HTML hidden 属性恢复状态。
// innerHTML/textContent 只是普通属性，不解析 HTML 或执行事件，也不实现 DOM 类型强制转换；
// listeners 为当前进程记录，后续页面注册与文件场景共享这些对象。
//
// ## References
// - tests/benchmarks/benchmark_compare_viewer_harness.js:8
// - tests/benchmarks/benchmark_compare_viewer_harness.js:9
// - tests/benchmarks/benchmark_compare_viewer_harness.js:10
// - tests/benchmarks/benchmark_compare_viewer_harness.js:11
// - tests/benchmarks/benchmark_compare_viewer_harness.js:12
// - scripts/benchmark/benchmark_compare_viewer.html:128
// - scripts/benchmark/benchmark_compare_viewer.html:134
// - scripts/benchmark/benchmark_compare_viewer.html:136

// @unit tests/benchmarks/benchmark_compare_viewer_harness.js::element::addEventListener [function]
//
// ## Summary
// 在当前替身元素上记录页面注册的指定名称回调，供宿主随后选择 change 事件。
//
// ## Parameters
// @param[in] name 用作 listeners 属性键的事件名。
// @param[in] callback 待保存的回调值；此处不检查它是否可调用。
//
// ## Returns
// @return undefined；写入或覆盖 this.listeners[name]，不立即触发回调。
//
// ## Constraints and lifecycle
// 须以持有 listeners 的元素对象为 this 调用；脱离该接收者调用不受支持。
// 同名监听器覆盖，不实现浏览器的多监听器、capture/options或移除语义；保存回调不转移其资源所有权。
// 回调及闭包在当前请求的元素存续期间可达，最终文件协议才检查 change 是否为function。
//
// ## References
// - tests/benchmarks/benchmark_compare_viewer_harness.js:12
// - tests/benchmarks/benchmark_compare_viewer_harness.js:43
// - scripts/benchmark/benchmark_compare_viewer.html:465

// @unit tests/benchmarks/benchmark_compare_viewer_harness.js::scripts [state]
//
// ## Summary
// 从受信页面文本中截取 script 标签的属性与正文，供嵌入数据查找和初始执行共享。
//
// ## Constraints and lifecycle
// 使用带 gi 标志的正则匹配 script 标签，不是 HTML parser；属性只是原始捕获字符串。
// 不解析 src、不下载外部脚本，也不完整处理任意 HTML/脚本标记组合。数组和捕获字符串由当前进程持有；
// 后续只按既有模板约定筛选 text/plain，不能把该模型视为浏览器脚本加载语义。
//
// ## References
// - tests/benchmarks/benchmark_compare_viewer_harness.js:15
// - tests/benchmarks/benchmark_compare_viewer_harness.js:16
// - tests/benchmarks/benchmark_compare_viewer_harness.js:32
// - tests/benchmarks/benchmark_compare_viewer_harness.js:33
// - scripts/benchmark/benchmark_compare_viewer.html:128
// - scripts/benchmark/benchmark_compare_viewer.html:169

// @unit tests/benchmarks/benchmark_compare_viewer_harness.js::scripts.find::<predicate> [callback]
//
// ## Summary
// 为 Array.find 识别第一个带约定嵌入数据 id 的 script 捕获。
//
// ## Parameters
// @param[in] match script正则的匹配数组，match[1] 是原始属性文本。
//
// ## Returns
// @return 属性文本中匹配约定双引号 id 拼写时为 true，否则为 false；find 只保留首个命中。
//
// ## Constraints and lifecycle
// 本谓词正则无 i 标志，按现有模板的大小写和双引号字面拼写工作，不做完整属性认证。
// 只借用匹配数组，不修改它；空结果由后续 embedded 条件处理，不创建浏览器元素。
//
// ## References
// - tests/benchmarks/benchmark_compare_viewer_harness.js:15
// - tests/benchmarks/benchmark_compare_viewer_harness.js:16
// - tests/benchmarks/benchmark_compare_viewer_harness.js:17
// - scripts/benchmark/benchmark_compare_viewer.html:128

// @unit tests/benchmarks/benchmark_compare_viewer_harness.js::module::embedded_script_text [block]
//
// ## Summary
// 把已找到的嵌入数据 script 正文放入同 id 替身的 textContent，让实际页面按原逻辑读取。
//
// ## Constraints and lifecycle
// 只在 embedded 存在时赋值；依赖前面的 id 扫描已建立 zr-bench-embed 元素。
// 没有另行认证元素存在，若两种正则的假设不成立，elements.get 的属性访问可抛异常并终止请求。
// 正文按捕获字符串原样保存；不在这里解码base64或解析JSON，后续由页面处理。
//
// ## References
// - tests/benchmarks/benchmark_compare_viewer_harness.js:16
// - tests/benchmarks/benchmark_compare_viewer_harness.js:17
// - tests/benchmarks/benchmark_compare_viewer_harness.js:9
// - scripts/benchmark/benchmark_compare_viewer.html:128
// - scripts/benchmark/benchmark_compare_viewer.html:207
// - scripts/benchmark/benchmark_compare_viewer.html:209

// @unit tests/benchmarks/benchmark_compare_viewer_harness.js::FileReader [class]
//
// ## Summary
// 提供页面文件输入路径所需的同步 FileReader 替身，实例只承载 result 和页面设置的 onload。
//
// ## Constraints and lifecycle
// 实例由实际页面 new 创建，借用到 context；不持有文件句柄、不读取磁盘、不模拟异步事件队列。
// 调用前由页面设置可调用 onload，readAsText 会以该实例为接收者立即调用它；没有onerror/abort等浏览器API。
// 只适用于此测试输入协议，不据此推断真实浏览器读取/编码/并发时序。
//
// ## References
// - tests/benchmarks/benchmark_compare_viewer_harness.js:19
// - tests/benchmarks/benchmark_compare_viewer_harness.js:20
// - tests/benchmarks/benchmark_compare_viewer_harness.js:23
// - tests/benchmarks/benchmark_compare_viewer_harness.js:28
// - scripts/benchmark/benchmark_compare_viewer.html:468
// - scripts/benchmark/benchmark_compare_viewer.html:469
// - scripts/benchmark/benchmark_compare_viewer.html:482

// @unit tests/benchmarks/benchmark_compare_viewer_harness.js::FileReader.readAsText [function]
//
// ## Summary
// 记录页面请求的编码参数，把传入伪文件文本交给 result，并同步进入页面已设置的 onload。
//
// ## Parameters
// @param[in] file 至少提供 text 属性的伪文件对象；其值直接赋给 result。
// @param[in] encoding 仅追加到 encodings 供断言，不据此解码 file.text。
//
// ## Returns
// @return 正常 onload 完成后为 undefined；属性读取或回调异常直接传播，不转成读取错误事件。
//
// ## Constraints and lifecycle
// 以 FileReader 实例为 this，且 this.onload 必须可调用；this.onload() 保留 reader 接收者。
// 先记录 encoding，再赋 result，最后调用回调；后续失败不回滚已经发生的写入。
// 不复制真实文件、不开句柄，也不提供异步完成或取消；onload 内部可借用 result 到其当前调用结束。
//
// ## References
// - tests/benchmarks/benchmark_compare_viewer_harness.js:20
// - tests/benchmarks/benchmark_compare_viewer_harness.js:21
// - tests/benchmarks/benchmark_compare_viewer_harness.js:22
// - tests/benchmarks/benchmark_compare_viewer_harness.js:23
// - scripts/benchmark/benchmark_compare_viewer.html:469
// - scripts/benchmark/benchmark_compare_viewer.html:471
// - scripts/benchmark/benchmark_compare_viewer.html:482

// @unit tests/benchmarks/benchmark_compare_viewer_harness.js::context.document.getElementById [callback]
//
// ## Summary
// 让实际页面借用 id 对应的元素替身，缺失时返回 null。
//
// ## Parameters
// @param[in] id 用于 Map 查询的键；按当前页面传入字符串，不额外进行 DOM 字符串强制转换。
//
// ## Returns
// @return 已存的元素对象或 null；返回对象与后续事件和快照共享，不是副本。
//
// ## Constraints and lifecycle
// 返回借用在当前Node请求期间有效，可被页面修改显示属性或注册listeners；没有真实DOM树、节点创建或文档生命周期。
// 跨VM和宿主使用同一元素记录，不得把它解释为完整浏览器 document 接口。
//
// ## References
// - tests/benchmarks/benchmark_compare_viewer_harness.js:27
// - tests/benchmarks/benchmark_compare_viewer_harness.js:8
// - scripts/benchmark/benchmark_compare_viewer.html:171
// - scripts/benchmark/benchmark_compare_viewer.html:207

// @unit tests/benchmarks/benchmark_compare_viewer_harness.js::context.atob [callback]
//
// ## Summary
// 把页面嵌入的base64送入Node Buffer，并以 latin1 返回每字节一个字符的二进制字符串。
//
// ## Parameters
// @param[in] text 按现有注入器生成的base64文本使用；实际接受范围沿用 Buffer.from。
//
// ## Returns
// @return 新的 latin1 字符串，供页面再构造 Uint8Array 并以 TextDecoder 解码UTF-8；错误直接传播。
//
// ## Constraints and lifecycle
// 此处不把base64直接解码成Unicode，也不实现浏览器 atob 的完整输入校验。
// Buffer.from 的宽松接受规则不能当浏览器对无效base64的拒绝证据；临时Buffer与返回字符串由Node管理。
// 实际注入器生成合法ASCII base64，中文/非BMP是否还原由页面decoder及有限测试共同检查。
//
// ## References
// - tests/benchmarks/benchmark_compare_viewer_harness.js:29
// - tests/benchmarks/benchmark_compare_viewer_harness.js:30
// - scripts/benchmark/benchmark_compare_viewer.html:213
// - scripts/benchmark/benchmark_compare_viewer.html:214
// - scripts/benchmark/aggregate_benchmark_summary.py:400
// - scripts/benchmark/aggregate_benchmark_summary.py:401

// @unit tests/benchmarks/benchmark_compare_viewer_harness.js::module::initial_script_execution [block]
//
// ## Summary
// 按捕获顺序在同一vm上下文执行页面的初始脚本，保留页面自身IIFE与私有函数边界。
//
// ## Constraints and lifecycle
// 仅按原始属性中 type="text/plain" 的大小写敏感正则跳过数据脚本；不完整解释type/src或浏览器加载策略。
// 每次 runInContext 独立设置2000毫秒，并以 actual-viewer-inline.js 标记该正文的栈信息。
// 这是各次初始执行的限制，不是整进程总时限，不覆盖随后由宿主直接调用的change/onload回调。
// 异常未兜底，前面脚本造成的上下文变更不回滚；该vm仅用于受信测试页面。
//
// ## References
// - tests/benchmarks/benchmark_compare_viewer_harness.js:32
// - tests/benchmarks/benchmark_compare_viewer_harness.js:33
// - tests/benchmarks/benchmark_compare_viewer_harness.js:34
// - tests/benchmarks/benchmark_compare_viewer_harness.js:26
// - scripts/benchmark/benchmark_compare_viewer.html:128
// - scripts/benchmark/benchmark_compare_viewer.html:169
// - tests/benchmarks/test_benchmark_compare_viewer.py:61

// @unit tests/benchmarks/benchmark_compare_viewer_harness.js::snapshot [function]
//
// ## Summary
// 读取当前所有元素的三个显示字段，生成可交给父Python进程的有限页面状态。
//
// ## Returns
// @return 新的外层对象及各id对应的新记录，字段为 html、text、hidden；不包含listeners或整个元素对象。
//
// ## Constraints and lifecycle
// 调用时读取共享Map的当前内容，每次快照都建立新容器，供初始和每次事件后留档。
// 字段值只作浅复制，不执行HTML解析、字符串强制转换或深克隆；若脚本赋入可变对象，则该值仍可能共享。
// 现有页面将这些字段用于显示字符串/布尔状态；最终JSON序列化不是任意对象图的认证。
//
// ## References
// - tests/benchmarks/benchmark_compare_viewer_harness.js:36
// - tests/benchmarks/benchmark_compare_viewer_harness.js:37
// - tests/benchmarks/benchmark_compare_viewer_harness.js:38
// - tests/benchmarks/benchmark_compare_viewer_harness.js:41
// - tests/benchmarks/benchmark_compare_viewer_harness.js:46
// - tests/benchmarks/benchmark_compare_viewer_harness.js:48

// @unit tests/benchmarks/benchmark_compare_viewer_harness.js::snapshot.map::<entry> [callback]
//
// ## Summary
// 将一个Map条目投影成 Object.fromEntries 所需的 id 与显示字段记录。
//
// ## Parameters
// - [id, el]：实际参数解构一个Map条目的键和元素对象；只读取el的三个显示属性。
//
// ## Returns
// @return 新的二元素数组 [id, record]，record 仅保存当前 html/text/hidden 值。
//
// ## Constraints and lifecycle
// 只创建投影容器，不修改Map或元素，不复制listeners；属性值仍是浅复制。
// 供当前snapshot的同步Array.map使用，不保留额外文件或进程资源，读取异常向调用方传播。
//
// ## References
// - tests/benchmarks/benchmark_compare_viewer_harness.js:37
// - tests/benchmarks/benchmark_compare_viewer_harness.js:38
// - tests/benchmarks/benchmark_compare_viewer_harness.js:39
// - tests/benchmarks/benchmark_compare_viewer_harness.js:48

// @unit tests/benchmarks/benchmark_compare_viewer_harness.js::module::file_input_sequence [block]
//
// ## Summary
// 在初始快照后，依次模拟文件选择的change事件，并在每次同步处理完成后追加快照。
//
// ## Constraints and lifecycle
// files 缺失或为假值时使用空序列；否则须可迭代，每项直接作为伪文件的text值。
// 依赖file-input元素存在，且change单槽为function；缺失元素可先抛属性错误，非function则显式抛错。
// 以普通函数调用callback，不额外模拟DOM this/currentTarget、事件排队或多监听器；实际页面会同步进入FileReader.onload。
// 回调异常不追加本次快照，也不回滚此前元素变更；外层Python10秒等待仍适用，初始vm的2秒限制不覆盖此调用。
//
// ## References
// - tests/benchmarks/benchmark_compare_viewer_harness.js:41
// - tests/benchmarks/benchmark_compare_viewer_harness.js:42
// - tests/benchmarks/benchmark_compare_viewer_harness.js:43
// - tests/benchmarks/benchmark_compare_viewer_harness.js:44
// - tests/benchmarks/benchmark_compare_viewer_harness.js:45
// - tests/benchmarks/benchmark_compare_viewer_harness.js:46
// - scripts/benchmark/benchmark_compare_viewer.html:465
// - scripts/benchmark/benchmark_compare_viewer.html:469
// - scripts/benchmark/benchmark_compare_viewer.html:482
// - tests/benchmarks/test_benchmark_compare_viewer.py:61
