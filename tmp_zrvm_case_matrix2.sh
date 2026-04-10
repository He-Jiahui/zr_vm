set -e
cd /mnt/e/Git/zr_vm
mkdir -p /tmp/zrvm-cases2
cat > /tmp/zrvm-cases2/no_owner_while_lt.zr <<'EOF'
class Loop {
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
var loop = new Loop();
var directValue = direct(12, 0);
var metaValue = loop(10, 0);
var guardedValue = guarded(1);
var spin = 0;
while (spin < 1000) {
    spin = spin + 1;
}
return directValue + metaValue + guardedValue + spin / 1000;
EOF
cat > /tmp/zrvm-cases2/owner_while_le.zr <<'EOF'
class Loop {
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
var after = %upgrade(weak);
var spin = 0;
while (spin <= 999) {
    spin = spin + 1;
}
return directValue + metaValue + guardedValue + spin / 1000;
EOF
cat > /tmp/zrvm-cases2/owner_while_lt_var.zr <<'EOF'
class Loop {
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
var after = %upgrade(weak);
var spin = 0;
var limit = 1000;
while (spin < limit) {
    spin = spin + 1;
}
return directValue + metaValue + guardedValue + spin / 1000;
EOF
for f in /tmp/zrvm-cases2/*.zr; do
  echo "==== $(basename "$f")"
  ./build/codex-wsl-gcc-release-ci-make/bin/zr_vm_cli -e "$(cat "$f")" || true
  echo
done