set -e
proj=/tmp/zrvm-compile-proj
rm -rf "$proj"
mkdir -p "$proj/src" "$proj/bin"
cat > "$proj/case.zrp" <<'EOF'
{
  "name": "case",
  "source": "src",
  "binary": "bin",
  "entry": "main"
}
EOF
cp /tmp/zrvm-cases4/release_both.zr "$proj/src/main.zr"
cd /mnt/e/Git/zr_vm
./build/codex-wsl-gcc-release-ci-make/bin/zr_vm_cli --compile "$proj/case.zrp" --intermediate
printf '\n==== zri ====\n'
sed -n '1,240p' "$proj/bin/main.zri"