# SPDX-License-Identifier: GPL-2.0-or-later
"""Long recording on both canvases: checks drift, dropped frames, duration accuracy and memory growth.

Run after run_tests.py has built its scenes (it reuses "SR Sync").
"""

import argparse
import os
import sys
import time

import media
from obsproc import ObsProcess
from obsws import Client, RequestError


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--obs-dir", required=True)
    parser.add_argument("--workdir", required=True)
    parser.add_argument("--minutes", type=float, default=10)
    parser.add_argument("--url", default="ws://127.0.0.1:4466")
    args = parser.parse_args()

    process = ObsProcess(args.obs_dir)
    process.start()
    obs = Client(args.url, timeout=90)
    deadline = time.time() + 90
    while time.time() < deadline:
        try:
            if len(obs.vendor("GetStatus")["canvases"]) > 1:
                break
        except RequestError:
            pass
        time.sleep(1)

    out = os.path.join(args.workdir, "soak").replace("\\", "/")
    os.makedirs(out, exist_ok=True)
    for canvas in ("main", "Aitum Vertical"):
        obs.vendor("SetCanvasSettings", {"canvas": canvas, "output_directory": out, "video_encoder": "obs_x264"})
    obs.call("SetCurrentProgramScene", {"sceneName": "SR Sync"})
    time.sleep(2)

    seconds = args.minutes * 60
    stats_before = obs.call("GetStats")
    obs.vendor("StartRecording")
    started = time.time()
    time.sleep(2)
    files = {c["canvas"]: c["file"] for c in obs.vendor("GetStatus")["canvases"]}
    while time.time() - started < seconds:
        time.sleep(30)
        status = obs.vendor("GetStatus")["canvases"]
        print(f"  {int(time.time() - started):5d}s " +
              " | ".join(f"{c['canvas']}: {c['state']} {c['bytes'] // 1048576} MB dropped {c['dropped_frames']} "
                         f"buffer {c['audio_latency_ms']} ms" for c in status), flush=True)
    status = obs.vendor("GetStatus")["canvases"]
    obs.vendor("StopRecording")
    wall = time.time() - started
    time.sleep(5)
    stats_after = obs.call("GetStats")
    obs.close()
    process.close()

    failures = 0

    def check(name, ok, detail):
        nonlocal failures
        failures += 0 if ok else 1
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}  ({detail})")

    for c in status:
        check(f"{c['canvas']}: no dropped frames", c["dropped_frames"] == 0, c["dropped_frames"])
        check(f"{c['canvas']}: audio buffering stable", c["audio_latency_ms"] < 250, f"{c['audio_latency_ms']} ms")
    skipped = stats_after["renderSkippedFrames"] - stats_before["renderSkippedFrames"]
    check("no render frames skipped", skipped <= 5, skipped)
    growth = stats_after["memoryUsage"] - stats_before["memoryUsage"]
    check("OBS memory growth under 150 MB", growth < 150, f"{growth:+.0f} MB")

    for canvas, path in files.items():
        duration = float(media.probe(path)["format"]["duration"])
        check(f"{canvas}: duration matches wall clock", abs(duration - wall) < 3, f"{duration:.1f}s vs {wall:.1f}s")
        if canvas != "main":
            continue
        head = media.sync_offsets(media.flash_times(path, 1600, 900, 320, 180), media.beep_times(path))
        early = [o for o in head[:5]]
        late = head[-5:]
        if early and late:
            drift = 1000 * (sorted(late)[len(late) // 2] - sorted(early)[len(early) // 2])
            check("main: no A/V drift over the recording", abs(drift) <= 34, f"{drift:+.0f} ms")
        else:
            check("main: sync markers found", False, len(head))

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
