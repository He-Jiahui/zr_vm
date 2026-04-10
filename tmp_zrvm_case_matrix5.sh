set -e
cd /mnt/e/Git/zr_vm
mkdir -p /tmp/zrvm-cases5
common_prefix='class Loop {
    pub @call(n: int, acc: int): int {
        if (n == 0) { return acc; }
        return this(n - 1, acc + 1);
    }
}
func direct(n: int, acc: int): int {
    if (n == 0) { return acc; }
    return direct(n - 1, acc + 1);
}
func guarded(flag: int): int {
    var marker = 0;
    try {
        try {
            if (flag != 0) { throw "boom"; }
            return 0;
        } finally {
            marker = marker + 7;
        }
    } catch (e) {
        return marker + 1;
    }
}
class Box {}
var ownerSeed = %unique new Box();
var owner = %shared(ownerSeed);
var weak = %weak(owner);
var alias = %upgrade(weak);
var loop = new Loop();
var directValue = direct(12, 0);
var metaValue = loop(10, 0);
var guardedValue = guarded(1);
var releasedOwner = %release(owner);
var releasedAlias = %release(alias);
'
printf "%s\nreturn directValue;\n" "$common_prefix" > /tmp/zrvm-cases5/return_direct.zr
printf "%s\nreturn metaValue;\n" "$common_prefix" > /tmp/zrvm-cases5/return_meta.zr
printf "%s\nreturn guardedValue;\n" "$common_prefix" > /tmp/zrvm-cases5/return_guarded.zr
printf "%s\nreturn directValue + 1;\n" "$common_prefix" > /tmp/zrvm-cases5/return_direct_plus1.zr
printf "%s\nreturn metaValue + 1;\n" "$common_prefix" > /tmp/zrvm-cases5/return_meta_plus1.zr
printf "%s\nreturn guardedValue + 1;\n" "$common_prefix" > /tmp/zrvm-cases5/return_guarded_plus1.zr
for f in /tmp/zrvm-cases5/*.zr; do
  echo "==== $(basename "$f")"
  ./build/codex-wsl-gcc-release-ci-make/bin/zr_vm_cli -e "$(cat "$f")" >/tmp/zrvm_case_stdout 2>/tmp/zrvm_case_stderr || true
  echo "[stdout]"
  cat /tmp/zrvm_case_stdout
  echo "[stderr]"
  cat /tmp/zrvm_case_stderr
  echo
done