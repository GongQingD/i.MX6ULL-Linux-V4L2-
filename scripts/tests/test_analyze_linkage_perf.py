import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from analyze_linkage_perf import (
    parse_perf_event_line,
    render_summary_sentence,
    summarize_events,
)
from collect_linkage_perf import ensure_perf_events_exist


class PerfAnalysisTests(unittest.TestCase):
    def test_parse_perf_event_line(self):
        line = "PERF_EVENT ts_ms=123 app=My_Project event=motion_rise session_id=7 value=1"
        parsed = parse_perf_event_line(line)
        self.assertEqual(parsed["ts_ms"], 123)
        self.assertEqual(parsed["app"], "My_Project")
        self.assertEqual(parsed["event"], "motion_rise")
        self.assertEqual(parsed["session_id"], "7")
        self.assertIn("raw", parsed)

    def test_summarize_metrics(self):
        events = [
            {"event": "camera_fps_window", "fps": "33.1", "ts_ms": 1000},
            {"event": "camera_fps_window", "fps": "32.9", "ts_ms": 2000},
            {"event": "motion_auto_trigger", "ts_ms": 3000, "session_id": "a"},
            {"event": "camera_first_frame_displayed", "ts_ms": 3600, "session_id": "a"},
            {"event": "als_dark_trigger", "ts_ms": 4000},
            {"event": "led_auto_set", "ts_ms": 4200, "led": "1", "trigger_ts_ms": "4000"},
            {"event": "camera_auto_start_requested", "ts_ms": 4500},
            {"event": "camera_process_started", "ts_ms": 4600, "auto_triggered": "1"},
        ]
        summary = summarize_events(events)
        self.assertAlmostEqual(summary["stable_preview_fps"], 33.0, places=2)
        self.assertEqual(summary["motion_to_first_frame_ms"], 600.0)
        self.assertEqual(summary["dark_to_led_on_ms"], 200.0)
        self.assertAlmostEqual(summary["continuous_run_hours"], 0.001, places=5)
        self.assertEqual(summary["linkage_success_rate_percent"], 100.0)

    def test_capture_duration_overrides_sparse_event_span(self):
        events = [
            {"event": "my_project_started", "ts_ms": 1000},
            {"event": "motion_auto_trigger", "ts_ms": 2000, "session_id": "a"},
        ]
        summary = summarize_events(events, capture_duration_ms=7_200_000)
        self.assertEqual(summary["continuous_run_hours"], 2.0)

    def test_ensure_perf_events_exist_rejects_empty_capture(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            log_path = Path(tmpdir) / "board.log"
            log_path.write_text("ssh banner only\n", encoding="utf-8")
            with self.assertRaises(RuntimeError):
                ensure_perf_events_exist(log_path)

    def test_render_summary_sentence(self):
        summary = {
            "stable_preview_fps": 31.5,
            "motion_to_first_frame_ms": 620.0,
            "dark_to_led_on_ms": 180.0,
            "continuous_run_hours": 1.5,
            "linkage_success_rate_percent": 100.0,
        }
        sentence = render_summary_sentence(summary)
        self.assertIn("31.5", sentence)
        self.assertIn("620 ms", sentence)
        self.assertIn("180 ms", sentence)
        self.assertIn("1.500 h", sentence)
        self.assertIn("100.0\\%", sentence)


if __name__ == "__main__":
    unittest.main()
