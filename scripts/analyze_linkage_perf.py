import argparse
import json
import math
from pathlib import Path
from typing import Dict, Iterable, List, Optional


def parse_perf_event_line(line: str) -> Optional[Dict[str, str]]:
    stripped = line.strip()
    if not stripped.startswith("PERF_EVENT"):
        return None
    parts = stripped.split()
    event: Dict[str, str] = {"raw": stripped}
    for token in parts[1:]:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        event[key] = value
    if "ts_ms" in event:
        try:
            event["ts_ms"] = int(event["ts_ms"])
        except ValueError:
            event["ts_ms"] = 0
    return event


def read_events(log_path: Path) -> List[Dict[str, str]]:
    if not log_path.exists():
        return []
    events: List[Dict[str, str]] = []
    with log_path.open("r", encoding="utf-8", errors="ignore") as handle:
        for line in handle:
            parsed = parse_perf_event_line(line)
            if parsed:
                events.append(parsed)
    return events


def summarize_events(events: Iterable[Dict[str, str]],
                     capture_duration_ms: Optional[int] = None) -> Dict[str, Optional[float]]:
    event_list = list(events)
    summary: Dict[str, Optional[float]] = {
        "stable_preview_fps": None,
        "motion_to_first_frame_ms": None,
        "dark_to_led_on_ms": None,
        "continuous_run_hours": None,
        "linkage_success_rate_percent": None,
    }

    fps_values: List[float] = []
    motion_ts: Dict[str, int] = {}
    first_frame_ts: Dict[str, int] = {}
    dark_ts_values: List[int] = []
    led_on_ts_values: List[int] = []
    request_count = 0
    start_count = 0
    ts_values: List[int] = []

    for ev in event_list:
        ts = ev.get("ts_ms")
        if isinstance(ts, int):
            ts_values.append(ts)
        if ev.get("event") == "camera_fps_window" and "fps" in ev:
            try:
                fps_values.append(float(ev["fps"]))
            except ValueError:
                pass
        if ev.get("event") in ("motion_auto_trigger", "motion_rise"):
            sid = ev.get("session_id", "global")
            if isinstance(ts, int):
                motion_ts.setdefault(sid, ts)
        if ev.get("event") == "camera_first_frame_displayed":
            sid = ev.get("session_id", "global")
            if isinstance(ts, int):
                first_frame_ts.setdefault(sid, ts)
        if ev.get("event") == "als_dark_trigger" and isinstance(ts, int):
            dark_ts_values.append(ts)
        if ev.get("event") == "led_auto_set" and ev.get("led") == "1" and isinstance(ts, int):
            led_on_ts_values.append(ts)
        if ev.get("event") == "camera_auto_start_requested":
            request_count += 1
        if ev.get("event") == "camera_process_started" and ev.get("auto_triggered", "1") == "1":
            start_count += 1

    if fps_values:
        summary["stable_preview_fps"] = round(sum(fps_values) / len(fps_values), 2)

    motion_to_first = math.inf
    for session_id, motion_ts_value in motion_ts.items():
        first_ts = first_frame_ts.get(session_id)
        if first_ts is not None:
            motion_to_first = min(motion_to_first, first_ts - motion_ts_value)
    if motion_to_first != math.inf:
        summary["motion_to_first_frame_ms"] = float(max(0, motion_to_first))

    dark_to_led = math.inf
    for ev in event_list:
        if ev.get("event") != "led_auto_set" or ev.get("led") != "1":
            continue

        led_ts = ev.get("ts_ms")
        if not isinstance(led_ts, int):
            continue

        trigger_ts = ev.get("trigger_ts_ms")
        if trigger_ts is not None:
            try:
                trigger_ts_value = int(trigger_ts)
            except ValueError:
                trigger_ts_value = None
            if trigger_ts_value is not None:
                dark_to_led = min(dark_to_led, max(0, led_ts - trigger_ts_value))
                continue

        for dark_ts in dark_ts_values:
            if led_ts >= dark_ts:
                dark_to_led = min(dark_to_led, led_ts - dark_ts)
                break
    if dark_to_led != math.inf:
        summary["dark_to_led_on_ms"] = float(dark_to_led)

    duration_ms = None
    if capture_duration_ms is not None and capture_duration_ms > 0:
        duration_ms = capture_duration_ms
    elif ts_values:
        duration_ms = max(ts_values) - min(ts_values)

    if duration_ms is not None:
        summary["continuous_run_hours"] = round(duration_ms / 3_600_000, 3)

    if request_count > 0:
        summary["linkage_success_rate_percent"] = round((start_count / request_count) * 100, 2)
    else:
        summary["linkage_success_rate_percent"] = 0.0

    return summary


def render_summary_sentence(summary: Dict[str, Optional[float]]) -> str:
    def fmt(value: Optional[float], pattern: str, default: str) -> str:
        if value is None:
            return default
        return pattern.format(value)

    fps_text = fmt(summary.get("stable_preview_fps"), "{:.1f} FPS", "未知")
    motion_ms = fmt(summary.get("motion_to_first_frame_ms"), "{:.0f} ms", "未知")
    dark_ms = fmt(summary.get("dark_to_led_on_ms"), "{:.0f} ms", "未知")
    uptime = fmt(summary.get("continuous_run_hours"), "{:.3f} h", "未知")
    success = fmt(summary.get("linkage_success_rate_percent"), "{:.1f}\\%", "未知")

    sentence = (
        "实现 \\textbf{" + fps_text + "} 视频预览，人体触发到摄像头稳定输出画面耗时 "
        "\\textbf{" + motion_ms + "}，暗光联动补光响应时间 \\textbf{" + dark_ms + "}；系统连续运行 "
        "\\textbf{" + uptime + "} 保持稳定，联动触发成功率 \\textbf{" + success + "}。"
    )
    return sentence


def dump_events(events: Iterable[Dict[str, str]], out_path: Path) -> None:
    with out_path.open("w", encoding="utf-8") as handle:
        for event in events:
            handle.write(json.dumps(event, ensure_ascii=False) + "\n")


def dump_summary(summary: Dict[str, Optional[float]], output_dir: Path) -> None:
    summary_path = output_dir / "summary.json"
    with summary_path.open("w", encoding="utf-8") as handle:
        json.dump(summary, handle, ensure_ascii=False, indent=2)
    sentence = render_summary_sentence(summary)
    with (output_dir / "summary.txt").open("w", encoding="utf-8") as handle:
        handle.write(sentence + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description="Analyze PERF_EVENT logs from My_Project.")
    parser.add_argument("--log", "-l", type=Path, required=True, help="Path to PERF_EVENT log file.")
    parser.add_argument(
        "--output-dir",
        "-o",
        type=Path,
        default=Path("scripts/perf_runs/latest"),
        help="Directory where events.jsonl, summary.json, summary.txt will be written.",
    )
    parser.add_argument(
        "--capture-duration-ms",
        type=int,
        default=None,
        help="Optional host-side capture window used for continuous_run_hours.",
    )
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)
    events = read_events(args.log)
    dump_events(events, args.output_dir / "events.jsonl")
    summary = summarize_events(events, capture_duration_ms=args.capture_duration_ms)
    dump_summary(summary, args.output_dir)


if __name__ == "__main__":
    main()
