import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path


def capture_log(log_path: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if log_path:
        shutil.copyfile(log_path, destination)
    else:
        with destination.open("wb") as out:
            out.write(sys.stdin.buffer.read())


def build_remote_command(board_dir: str, run_seconds: int, startup_script: str) -> str:
    inner = f"cd {shlex.quote(board_dir)} && timeout {int(run_seconds)} ./{startup_script}"
    return f"bash -l -c {shlex.quote(inner)}"


def capture_from_board(board: str,
                       board_dir: str,
                       run_seconds: int,
                       startup_script: str,
                       destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    remote_cmd = build_remote_command(board_dir, run_seconds, startup_script)
    ssh_cmd = ["ssh", board, remote_cmd]

    with destination.open("wb") as out:
        process = subprocess.Popen(ssh_cmd, stdout=out, stderr=subprocess.STDOUT)
        try:
            process.wait()
        except KeyboardInterrupt:
            process.terminate()
            process.wait()
            raise

        # `timeout` exits with 124 when the sample window ends normally.
        if process.returncode not in (0, 124):
            raise subprocess.CalledProcessError(process.returncode, ssh_cmd)


def ensure_perf_events_exist(log_path: Path) -> None:
    with log_path.open("r", encoding="utf-8", errors="ignore") as handle:
        for line in handle:
            if line.startswith("PERF_EVENT "):
                return
    raise RuntimeError(f"No PERF_EVENT lines captured in {log_path}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Capture PERF_EVENT log and run the analyzer.")
    parser.add_argument("--log-file", "-l", type=Path, help="Existing log file to ingest.")
    parser.add_argument("--board", default=os.environ.get("BOARD", "root@10.20.20.36"),
                        help="Board SSH target used for live collection.")
    parser.add_argument("--board-dir", default="/lib/modules/4.1.15-g3dc0a4b",
                        help="Board directory that contains preload_drivers.sh.")
    parser.add_argument("--startup-script", default="preload_drivers.sh",
                        help="Board-side startup script to run during collection.")
    parser.add_argument("--run-seconds", type=int, default=180,
                        help="Live collection duration in seconds when --log-file is not used.")
    parser.add_argument(
        "--output-dir",
        "-o",
        type=Path,
        default=Path("scripts/perf_runs") / datetime.utcnow().strftime("%Y%m%d%H%M%S"),
        help="Directory to place raw log and analysis outputs.",
    )
    parser.add_argument(
        "--skip-analyze",
        action="store_true",
        help="Only capture the log without invoking the analyzer.",
    )

    args = parser.parse_args()
    raw_dir = args.output_dir / "raw"
    raw_dir.mkdir(parents=True, exist_ok=True)
    log_dest = raw_dir / "board.log"
    if args.log_file:
        capture_log(args.log_file, log_dest)
        capture_duration_ms = None
    else:
        start = time.monotonic()
        capture_from_board(args.board,
                           args.board_dir,
                           args.run_seconds,
                           args.startup_script,
                           log_dest)
        capture_duration_ms = int((time.monotonic() - start) * 1000)

    ensure_perf_events_exist(log_dest)

    if args.skip_analyze:
        print(f"Logged PERF_EVENT to {log_dest}")
        return

    analyzer = Path(__file__).with_name("analyze_linkage_perf.py")
    analyze_cmd = [
        sys.executable,
        str(analyzer),
        "--log", str(log_dest),
        "--output-dir", str(args.output_dir),
    ]
    if capture_duration_ms is not None:
        analyze_cmd.extend(["--capture-duration-ms", str(capture_duration_ms)])
    subprocess.run(analyze_cmd, check=True)


if __name__ == "__main__":
    main()
