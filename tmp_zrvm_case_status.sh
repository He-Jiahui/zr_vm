cd /mnt/e/Git/zr_vm
for f in /tmp/zrvm-cases/case3.zr /tmp/zrvm-cases2/owner_while_le.zr; do
  echo "==== $f"
  ./build/codex-wsl-gcc-release-ci-make/bin/zr_vm_cli -e "$(cat "$f")"
  status=$?
  echo "[exit=$status]"
  echo
 done