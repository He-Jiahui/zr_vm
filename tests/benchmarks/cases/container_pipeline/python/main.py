from pathlib import Path
import sys

COMMON_DIR = Path(__file__).resolve().parents[3] / "common" / "python"
if str(COMMON_DIR) not in sys.path:
    sys.path.insert(0, str(COMMON_DIR))

from benchmark_runner import run_main


if __name__ == "__main__":
    run_main("container_pipeline")
