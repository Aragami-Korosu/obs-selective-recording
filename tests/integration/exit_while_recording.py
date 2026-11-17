# SPDX-License-Identifier: GPL-2.0-or-later
"""Quick check: close OBS while both canvases record, then report how far shutdown got."""

import argparse
import time

from obsproc import ObsProcess
from obsws import Client, RequestError


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--obs-dir", required=True)
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

    obs.vendor("StartRecording")
    time.sleep(4)
    print([(c["canvas"], c["state"]) for c in obs.vendor("GetStatus")["canvases"]])
    obs.close()
    start = time.time()
    closed = process.close(timeout=40)
    print(f"closed={closed} after {time.time() - start:.1f}s")
    with open(process.latest_log(), encoding="utf-8", errors="replace") as f:
        lines = f.read().splitlines()
    begin = next((i for i, line in enumerate(lines) if "Shutting down" in line), len(lines) - 5)
    print("\n".join(lines[begin:begin + 40]))


if __name__ == "__main__":
    main()
