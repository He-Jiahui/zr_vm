#!/usr/bin/env bash
# Sweep numeric_loops workload by bench_config scale: compare ZR interp vs aot_c run time.
# Work grows ~ (24*scale)*(3000*scale) inner iterations (see main.zr).
#
# Usage:
#   bash scripts/benchmark/time_numeric_loops_scale_sweep.sh [build_root] [repo_root]
# Env:
#   SCALE_LIST="4 16 32 64"   # 64 is very heavy (long runs); default below is 4 16 32
#
# Notes:
# - Each scale uses a fresh copy of tests/benchmarks/cases/numeric_loops/zr with rewritten bench_config.zr.
# - Checksum line will not match registry values at non-default scales; this script is timing-only.
set -euo pipefail

BUILD_ROOT="${1:-/mnt/e/Git/zr_vm/build/benchmark-gcc-release}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# BUG: 默认 REPO_ROOT 仅上溯到 zr_vm_aot；未传第二参数时 SRC_CASE 指向不存在的 zr_vm_aot/tests/benchmarks，脚本在基准准备阶段退出。
REPO_ROOT="${2:-$(cd "${SCRIPT_DIR}/../.." && pwd)}"
CLI="${BUILD_ROOT}/bin/zr_vm_cli"
SRC_CASE="${REPO_ROOT}/tests/benchmarks/cases/numeric_loops/zr"
SCALE_LIST="${SCALE_LIST:-4 16 32}"

if [[ ! -x "$CLI" ]]; then
    echo "missing CLI: $CLI"
    exit 1
fi
if [[ ! -f "${SRC_CASE}/benchmark_numeric_loops.zrp" ]]; then
    echo "missing fixture: ${SRC_CASE}/benchmark_numeric_loops.zrp"
    exit 1
fi

# Optional warmup runs (same argv) before timing to reduce cold-cache / first-dlopen noise.
WARMUP_RUNS="${WARMUP_RUNS:-1}"

# Run CLI from project directory (relative .zrp). Absolute paths to /mnt/e/ projects can fail on WSL without cwd.
mean_seconds_n() {
    # 同一份已编译临时项目承受预热和正式采样，使比例主要反映执行模式差异。
    local n=$1
    work_dir=$2
    shift 2
    local sum=0 i
    for i in $(seq 1 "$WARMUP_RUNS"); do
        (cd "$work_dir" && "$@" >/dev/null 2>&1) || true
    done
    for i in $(seq 1 "$n"); do
        local start_ns end_ns
        start_ns=$(date +%s%N)
        (cd "$work_dir" && "$@" >/dev/null 2>&1)
        end_ns=$(date +%s%N)
        sum=$((sum + end_ns - start_ns))
    done
    awk -v s="$sum" -v n="$n" 'BEGIN { printf "%.3f", s / n / 1e9 }'
}

echo "CLI=$CLI"
echo "SRC_CASE=$SRC_CASE"
echo "SCALE_LIST=$SCALE_LIST"
echo "WARMUP_RUNS=$WARMUP_RUNS (set 0 to disable)"
echo ""
printf "%-8s %12s %12s %10s %14s\n" "scale" "interp_mean_s" "aot_c_mean_s" "aot/interp" "aot_interp_s"
echo "--------------------------------------------------------------------------------"

for SCALE in $SCALE_LIST; do
    # BUG: 编译或正式采样命令失败时 set -e 会在循环尾的 rm 前退出，留下该轮 WORK 临时项目；需核对统一退出清理入口。
    WORK=$(mktemp -d /tmp/zr_numeric_scale_XXXXXX)
    cp -a "${SRC_CASE}/." "$WORK/"
    cat >"${WORK}/src/bench_config.zr" <<EOF
pub scale(): int {
    return ${SCALE};
}
EOF
    ZRP_NAME="benchmark_numeric_loops.zrp"

    (cd "$WORK" && "$CLI" --compile "$ZRP_NAME" --emit-aot-c >/dev/null 2>&1)

    INTERP=$(mean_seconds_n 3 "$WORK" "$CLI" "$ZRP_NAME")
    AOT=$(mean_seconds_n 3 "$WORK" "$CLI" "$ZRP_NAME" --execution-mode aot_c --require-aot-path)
    RATIO=$(awk -v a="$AOT" -v b="$INTERP" 'BEGIN { if (b > 0) printf "%.3f", a / b; else print "inf" }')
    DIFF=$(awk -v a="$AOT" -v b="$INTERP" 'BEGIN { printf "%.3f", a - b }')

    printf "%-8s %12s %12s %10s %14s\n" "$SCALE" "$INTERP" "$AOT" "$RATIO" "$DIFF"
    rm -rf "$WORK"
done

echo ""
echo "Interpretation (rule of thumb):"
echo "  If ratio aot/interp trends toward 1 as scale grows, fixed overhead dominates at small scale."
echo "  If ratio stays flat while both times grow, per-iteration cost differs (not just one-time overhead)."
