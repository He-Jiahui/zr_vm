set -e
proj=/tmp/zrvm-case-project
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
cp /tmp/zrvm-cases/case4.zr "$proj/src/main.zr"
cd /mnt/e/Git/zr_vm
./build/codex-wsl-gcc-release-ci-make/bin/zr_vm_cli "$proj/case.zrp" || true
printf '\n==== zri ====\n'
sed -n '1,240p' "$proj/bin/main.zri"