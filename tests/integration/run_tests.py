# SPDX-License-Identifier: GPL-2.0-or-later
"""End-to-end tests for OBS Selective Recording.

Run against a disposable, portable OBS (see README.md in this folder). The tests build
their own scenes from solid colours and test tones, record through the plugin, then check
the files pixel by pixel and by frequency.
"""

import argparse
import base64
import io
import os
import sys
import time

from PIL import Image

import media
from obsproc import ObsProcess
from obsws import Client, RequestError

VERTICAL = "Aitum Vertical"

RED, GREEN, BLUE, YELLOW, MAGENTA, CYAN = (
    0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFF00FFFF, 0xFFFF00FF, 0xFFFFFF00)

results = []


def check(name, condition, detail=""):
    results.append((name, bool(condition)))
    status = "PASS" if condition else "FAIL"
    print(f"  [{status}] {name}" + (f"  ({detail})" if detail else ""), flush=True)


def wait_for(predicate, timeout=20, interval=0.25):
    deadline = time.time() + timeout
    while time.time() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(interval)
    return None


class Harness:
    def __init__(self, obs, workdir):
        self.obs = obs
        self.workdir = workdir
        self.items = {}

    def status(self, canvas):
        for c in self.obs.vendor("GetStatus")["canvases"]:
            if c["canvas"] == canvas:
                return c
        return None

    def color(self, name, color, w, h):
        self.obs.call("CreateInput", {
            "sceneName": "SR Scratch", "inputName": name, "inputKind": "color_source_v3",
            "inputSettings": {"color": color, "width": w, "height": h}, "sceneItemEnabled": True})

    def add(self, scene, source, x, y, visible=True, scene_uuid=None):
        target = {"sceneUuid": scene_uuid} if scene_uuid else {"sceneName": scene}
        item = self.obs.call("CreateSceneItem", {**target, "sourceName": source, "sceneItemEnabled": visible})
        item_id = item["sceneItemId"]
        self.obs.call("SetSceneItemTransform", {**target, "sceneItemId": item_id,
                                                "sceneItemTransform": {"positionX": x, "positionY": y}})
        self.items[(scene or scene_uuid, source)] = item_id
        return item_id

    def snapshot(self, scene=None, scene_uuid=None):
        target = {"sceneUuid": scene_uuid} if scene_uuid else {"sceneName": scene}
        items = self.obs.call("GetSceneItemList", target)["sceneItems"]
        return sorted((i["sceneItemId"], i["sourceName"], i["sceneItemEnabled"],
                       round(i["sceneItemTransform"]["positionX"]), round(i["sceneItemTransform"]["positionY"]))
                      for i in items)

    def live_color(self, scene, x, y, width=1920, height=1080):
        shot = self.obs.call("GetSourceScreenshot", {
            "sourceName": scene, "imageFormat": "png", "imageWidth": width // 4, "imageHeight": height // 4})
        data = base64.b64decode(shot["imageData"].split(",", 1)[1])
        image = Image.open(io.BytesIO(data)).convert("RGB")
        return media.nearest_color(image.getpixel((x // 4, y // 4)))

    def record(self, canvas, seconds, during=None):
        self.obs.vendor("StartRecording", {"canvas": canvas})
        if not wait_for(lambda: self.status(canvas)["state"] == "recording", 10):
            raise RuntimeError(f"{canvas} did not start: {self.status(canvas)}")
        path = self.status(canvas)["file"]
        start = time.time()
        if during:
            during(start)
        remaining = seconds - (time.time() - start)
        if remaining > 0:
            time.sleep(remaining)
        self.obs.vendor("StopRecording", {"canvas": canvas})
        if not wait_for(lambda: self.status(canvas)["state"] in ("idle", "failed"), 20):
            raise RuntimeError(f"{canvas} did not stop")
        return path


def setup(h):
    obs = h.obs
    print("Setting up scenes")
    for scene in ("SR Scratch", "SR Test A", "SR Test B", "SR Inner"):
        obs.call("CreateScene", {"sceneName": scene}, allow_fail=True)

    h.color("SR BG Red", RED, 1920, 1080)
    h.color("SR BG Magenta", MAGENTA, 1920, 1080)
    h.color("SR Box Green", GREEN, 400, 400)
    h.color("SR Box Blue", BLUE, 400, 400)
    h.color("SR Box Yellow", YELLOW, 400, 400)
    h.color("SR Box Cyan", CYAN, 200, 200)
    h.color("SR V BG", RED, 1080, 1920)

    tone440 = os.path.join(h.workdir, "tone440.wav")
    tone1000 = os.path.join(h.workdir, "tone1000.wav")
    media.make_tone(tone440, 440)
    media.make_tone(tone1000, 1000)
    for name, path in (("SR Tone 440", tone440), ("SR Tone 1000", tone1000)):
        obs.call("CreateInput", {
            "sceneName": "SR Scratch", "inputName": name, "inputKind": "ffmpeg_source",
            "inputSettings": {"local_file": path, "looping": True, "is_local_file": True},
            "sceneItemEnabled": True})

    # Scene A: background, three boxes and two tones; a nested scene with its own boxes.
    h.add("SR Test A", "SR BG Red", 0, 0)
    h.add("SR Test A", "SR Box Green", 100, 100)
    h.add("SR Test A", "SR Box Blue", 700, 100, visible=False)
    h.add("SR Test A", "SR Box Yellow", 1300, 100)
    h.add("SR Test A", "SR Tone 440", 0, 0)
    h.add("SR Test A", "SR Tone 1000", 0, 0)
    h.add("SR Inner", "SR Box Cyan", 1500, 700)
    h.add("SR Inner", "SR Box Green", 1000, 700)
    h.add("SR Test A", "SR Inner", 0, 0)
    h.add("SR Test B", "SR BG Magenta", 0, 0)

    # "SR Scratch" stays: removing it would destroy sources that live only there.
    obs.call("SetCurrentProgramScene", {"sceneName": "SR Test A"})

    obs.vendor("SetRoute", {"source_name": "SR Box Green", "video": "stream_only"})
    obs.vendor("SetRoute", {"source_name": "SR Box Blue", "video": "recording_only"})
    obs.vendor("SetRoute", {"source_name": "SR Box Yellow", "video": "off"})
    obs.vendor("SetRoute", {"source_name": "SR Tone 1000", "audio": "off"})

    out = os.path.join(h.workdir, "out").replace("\\", "/")
    os.makedirs(out, exist_ok=True)
    for canvas in ("main", VERTICAL):
        obs.vendor("SetCanvasSettings", {"canvas": canvas, "output_directory": out, "video_encoder": "obs_x264"})
    time.sleep(2)


def test_discovery(h):
    print("Canvas discovery")
    status = h.obs.vendor("GetStatus")
    canvases = {c["canvas"] for c in status["canvases"]}
    check("main canvas discovered", "main" in canvases, sorted(canvases))
    check("Aitum Vertical canvas discovered", VERTICAL in canvases)


def test_main_recording(h):
    print("Main canvas: video routing, audio routing, transition")
    before = h.snapshot("SR Test A")

    def switch(start):
        time.sleep(max(0, 3.5 - (time.time() - start)))
        h.obs.call("SetCurrentProgramScene", {"sceneName": "SR Test B"})

    path = h.record("main", 7, during=switch)
    info = media.probe(path)
    video = media.video_stream(info)
    check("recording is 1920x1080", (video["width"], video["height"]) == (1920, 1080),
          f"{video['width']}x{video['height']}")
    check("recording has audio", media.has_audio(info))

    check("Both: background recorded", media.pixel(path, 1.5, 1000, 800) == "red")
    check("Stream only: green box left out", media.pixel(path, 1.5, 300, 300) == "red",
          media.pixel(path, 1.5, 300, 300))
    check("Recording only: hidden blue box recorded", media.pixel(path, 1.5, 900, 300) == "blue",
          media.pixel(path, 1.5, 900, 300))
    check("Off: yellow box left out", media.pixel(path, 1.5, 1500, 300) == "red",
          media.pixel(path, 1.5, 1500, 300))
    check("nested scene: cyan box recorded", media.pixel(path, 1.5, 1600, 800) == "cyan",
          media.pixel(path, 1.5, 1600, 800))
    check("nested scene: routing applies inside", media.pixel(path, 1.5, 1200, 900) == "red",
          media.pixel(path, 1.5, 1200, 900))
    check("scene switch followed", media.pixel(path, 6.0, 1000, 500) == "magenta",
          media.pixel(path, 6.0, 1000, 500))

    levels = media.tone_levels(path, [440, 1000], start=0.8, duration=2.0)
    check("audio: 440 Hz (Both) recorded", levels[440] > -6, f"{levels[440]:.1f} dB")
    check("audio: 1000 Hz (Off) left out", levels[1000] < -40, f"{levels[1000]:.1f} dB")

    h.obs.call("SetCurrentProgramScene", {"sceneName": "SR Test A"})
    time.sleep(1)
    after = h.snapshot("SR Test A")
    check("original scene items unchanged (NFR-001)", before == after)
    check("live: green box still on stream", h.live_color("SR Test A", 300, 300) == "green")
    check("live: hidden blue box still hidden", h.live_color("SR Test A", 900, 300) == "red")
    check("live: yellow box still on stream", h.live_color("SR Test A", 1500, 300) == "yellow")


def test_warnings(h):
    print("Warnings")
    warnings = h.obs.vendor("GetWarnings")["warnings"]
    by_name = {(w["canvas"], w["source_name"]): w for w in warnings}
    yellow = by_name.get(("main", "SR Box Yellow"))
    tone = by_name.get(("main", "SR Tone 1000"))
    check("Off source still visible live is flagged", yellow and yellow["video_live"])
    check("Off audio still on stream track is flagged", tone and tone["audio_live"])
    check("hidden recording-only source is not flagged", ("main", "SR Box Blue") not in by_name)


def test_vertical(h):
    print("Aitum Vertical canvas")
    scenes = h.obs.vendor("GetCanvasScenes", {"canvas": VERTICAL})["scenes"]
    check("vertical scenes listed", len(scenes) > 0, [s["name"] for s in scenes])
    if not scenes:
        return
    uuid = scenes[0]["uuid"]
    h.add(None, "SR V BG", 0, 0, scene_uuid=uuid)
    h.add(None, "SR Box Blue", 300, 300, visible=False, scene_uuid=uuid)
    h.add(None, "SR Box Green", 300, 900, scene_uuid=uuid)
    h.obs.vendor("switch_scene", {"scene": scenes[0]["name"]}, vendor="aitum-vertical-canvas")
    time.sleep(1)
    before = h.snapshot(scene_uuid=uuid)

    main_path = {}

    def start_main(start):
        h.obs.vendor("StartRecording", {"canvas": "main"})
        main_path["file"] = wait_for(lambda: h.status("main")["file"], 10)

    path = h.record(VERTICAL, 5, during=start_main)
    h.obs.vendor("StopRecording", {"canvas": "main"})
    wait_for(lambda: h.status("main")["state"] == "idle", 20)

    video = media.video_stream(media.probe(path))
    check("vertical recording is 1080x1920", (video["width"], video["height"]) == (1080, 1920),
          f"{video['width']}x{video['height']}")
    check("vertical: background recorded", media.pixel(path, 2.0, 540, 1600) == "red")
    check("vertical: hidden recording-only box recorded", media.pixel(path, 2.0, 500, 500) == "blue",
          media.pixel(path, 2.0, 500, 500))
    check("vertical: stream-only box left out", media.pixel(path, 2.0, 500, 1100) == "red",
          media.pixel(path, 2.0, 500, 1100))
    check("both canvases recorded at the same time", main_path.get("file") and os.path.exists(main_path["file"]))
    check("vertical scene items unchanged (NFR-001)", before == h.snapshot(scene_uuid=uuid))


def test_failure_isolation(h):
    print("Failure isolation (NFR-003)")
    h.obs.vendor("StartRecording", {"canvas": "main"})
    wait_for(lambda: h.status("main")["state"] == "recording", 10)
    h.obs.vendor("SetCanvasSettings", {"canvas": VERTICAL, "output_directory": "Q:/missing-drive/sr"})
    h.obs.vendor("StartRecording", {"canvas": VERTICAL})
    vertical = wait_for(lambda: h.status(VERTICAL) if h.status(VERTICAL)["state"] == "failed" else None, 10)
    check("vertical fails cleanly with a message", vertical and vertical["error"], vertical and vertical["error"])
    time.sleep(2)
    main = h.status("main")
    check("main keeps recording", main["state"] == "recording" and main["bytes"] > 0, main["state"])
    h.obs.vendor("StopRecording", {"canvas": "main"})
    wait_for(lambda: h.status("main")["state"] == "idle", 20)
    out = os.path.join(h.workdir, "out").replace("\\", "/")
    h.obs.vendor("SetCanvasSettings", {"canvas": VERTICAL, "output_directory": out})


def test_pause(h):
    print("Pause and resume")

    def pause(start):
        time.sleep(1.5)
        check("pause accepted", h.obs.vendor("PauseRecording", {"canvas": "main"}).get("ok"))
        time.sleep(2.0)
        check("resume accepted", h.obs.vendor("ResumeRecording", {"canvas": "main"}).get("ok"))

    path = h.record("main", 6, during=pause)
    duration = float(media.probe(path)["format"]["duration"])
    check("paused time left out of the file", 2.5 < duration < 5.0, f"{duration:.2f}s")


def test_groups(h):
    print("Groups")
    items = h.obs.call("GetSceneItemList", {"sceneName": "SR Group Scene"})["sceneItems"]
    group = next((i for i in items if i["sourceName"] == "SR G Group"), None)
    check("fixture group loaded", group and group["isGroup"], [i["sourceName"] for i in items])
    if not group:
        return

    h.obs.call("SetCurrentProgramScene", {"sceneName": "SR Group Scene"})
    h.obs.vendor("SetRoute", {"source_name": "SR G Box2", "video": "stream_only"})
    time.sleep(1)
    path = h.record("main", 3)
    check("group child recorded in place", media.pixel(path, 1.5, 700, 400) == "green",
          media.pixel(path, 1.5, 700, 400))
    check("routing applies inside a group", media.pixel(path, 1.5, 1000, 400) == "red",
          media.pixel(path, 1.5, 1000, 400))

    h.obs.call("SetSceneItemEnabled", {"sceneName": "SR Group Scene", "sceneItemId": group["sceneItemId"],
                                       "sceneItemEnabled": False})
    h.obs.vendor("SetRoute", {"source_name": "SR G Group", "video": "recording_only"})
    time.sleep(1)
    path = h.record("main", 3)
    check("hidden recording-only group recorded", media.pixel(path, 1.5, 700, 400) == "green",
          media.pixel(path, 1.5, 700, 400))
    check("live: hidden group stays hidden", h.live_color("SR Group Scene", 700, 400) == "red")
    h.obs.call("SetCurrentProgramScene", {"sceneName": "SR Test A"})


def test_av_sync(h):
    print("Audio/video sync against OBS's own recording")
    clip = os.path.join(h.workdir, "sync.mp4")
    media.make_sync_clip(clip)
    h.obs.call("CreateScene", {"sceneName": "SR Sync"}, allow_fail=True)
    h.obs.call("CreateInput", {
        "sceneName": "SR Sync", "inputName": "SR Sync Clip", "inputKind": "ffmpeg_source",
        "inputSettings": {"local_file": clip, "looping": True, "is_local_file": True}, "sceneItemEnabled": True})
    item = h.obs.call("GetSceneItemId", {"sceneName": "SR Sync", "sourceName": "SR Sync Clip"})["sceneItemId"]
    h.obs.call("SetSceneItemTransform", {"sceneName": "SR Sync", "sceneItemId": item,
                                         "sceneItemTransform": {"positionX": 1600, "positionY": 900}})
    h.obs.call("SetCurrentProgramScene", {"sceneName": "SR Sync"})
    time.sleep(2)

    reference = {}

    def start_obs_recording(start):
        h.obs.call("StartRecord")
        time.sleep(7)
        reference["path"] = h.obs.call("StopRecord")["outputPath"]

    path = h.record("main", 8, during=start_obs_recording)
    time.sleep(2)

    ours = media.sync_offsets(media.flash_times(path, 1600, 900, 320, 180), media.beep_times(path))
    theirs = media.sync_offsets(media.flash_times(reference["path"], 1600, 900, 320, 180),
                                media.beep_times(reference["path"]))
    check("sync markers found in our recording", len(ours) >= 4, f"{len(ours)} markers")
    check("sync markers found in OBS's recording", len(theirs) >= 4, f"{len(theirs)} markers")
    if ours and theirs:
        ours_ms = 1000 * sorted(ours)[len(ours) // 2]
        theirs_ms = 1000 * sorted(theirs)[len(theirs) // 2]
        check("A/V offset matches OBS within one frame", abs(ours_ms - theirs_ms) <= 34,
              f"ours {ours_ms:+.0f} ms, OBS {theirs_ms:+.0f} ms")
        spread = 1000 * (max(ours) - min(ours))
        check("A/V offset is stable (no drift)", spread <= 34, f"spread {spread:.0f} ms")
    h.obs.call("SetCurrentProgramScene", {"sceneName": "SR Test A"})


def test_studio_mode(h):
    print("Studio Mode follows program, not preview")
    h.obs.call("SetCurrentProgramScene", {"sceneName": "SR Test A"})
    h.obs.call("SetStudioModeEnabled", {"studioModeEnabled": True})
    time.sleep(1)
    h.obs.call("SetCurrentPreviewScene", {"sceneName": "SR Test B"})

    def transition(start):
        time.sleep(2.0)
        h.obs.call("TriggerStudioModeTransition")

    path = h.record("main", 4.5, during=transition)
    check("preview scene not recorded before transition", media.pixel(path, 1.0, 1000, 500) == "red",
          media.pixel(path, 1.0, 1000, 500))
    check("program scene recorded after transition", media.pixel(path, 3.8, 1000, 500) == "magenta",
          media.pixel(path, 3.8, 1000, 500))
    h.obs.call("SetStudioModeEnabled", {"studioModeEnabled": False})
    h.obs.call("SetCurrentProgramScene", {"sceneName": "SR Test A"})
    time.sleep(1)


def test_transition_blend(h):
    print("Animated transitions are mirrored")
    h.obs.call("SetCurrentSceneTransition", {"transitionName": "Fade"}, allow_fail=True)
    h.obs.call("SetCurrentSceneTransitionDuration", {"transitionDuration": 3000})

    def switch(start):
        time.sleep(1.0)
        h.obs.call("SetCurrentProgramScene", {"sceneName": "SR Test B"})

    path = h.record("main", 5.5, during=switch)
    r, g, b = media.pixel_rgb(path, 2.6, 1000, 500)
    check("mid-fade frame is a blend of both scenes", r > 200 and 50 < b < 220, f"rgb({r},{g},{b})")
    check("fade finishes on the new scene", media.pixel(path, 5.0, 1000, 500) == "magenta")
    h.obs.call("SetCurrentSceneTransitionDuration", {"transitionDuration": 300})
    h.obs.call("SetCurrentProgramScene", {"sceneName": "SR Test A"})
    time.sleep(1)


def test_source_removed(h):
    print("Deleting a source while recording")
    h.obs.call("CreateInput", {
        "sceneName": "SR Test A", "inputName": "SR Temp Box", "inputKind": "color_source_v3",
        "inputSettings": {"color": GREEN, "width": 200, "height": 200}, "sceneItemEnabled": True})
    item = h.obs.call("GetSceneItemId", {"sceneName": "SR Test A", "sourceName": "SR Temp Box"})["sceneItemId"]
    h.obs.call("SetSceneItemTransform", {"sceneName": "SR Test A", "sceneItemId": item,
                                         "sceneItemTransform": {"positionX": 800, "positionY": 750}})
    time.sleep(1)

    def remove(start):
        time.sleep(1.5)
        h.obs.call("RemoveInput", {"inputName": "SR Temp Box"})

    path = h.record("main", 4, during=remove)
    check("recording survives source deletion", h.status("main")["state"] == "idle", h.status("main")["state"])
    check("deleted source shown before deletion", media.pixel(path, 0.8, 900, 850) == "green")
    check("deleted source gone afterwards", media.pixel(path, 3.5, 900, 850) == "red")


def test_collection_switch(h):
    print("Scene collection switch while recording")
    h.obs.vendor("StartRecording", {"canvas": "main"})
    wait_for(lambda: h.status("main")["state"] == "recording", 10)
    h.obs.call("CreateSceneCollection", {"sceneCollectionName": "SR Other"}, allow_fail=True)
    time.sleep(3)
    h.obs.call("SetCurrentSceneCollection", {"sceneCollectionName": "Test"})
    time.sleep(3)
    state = h.status("main")["state"] if h.status("main") else "missing"
    check("recording continues through the switch", state == "recording", state)
    path = h.status("main")["file"] if h.status("main") else ""
    h.obs.vendor("StopRecording", {"canvas": "main"})
    wait_for(lambda: h.status("main")["state"] == "idle", 20)
    if path:
        duration = float(media.probe(path)["format"]["duration"])
        check("recording file valid after the switch", duration > 5, f"{duration:.1f}s")
    route = h.obs.vendor("GetRoute", {"source_name": "SR Box Green"})
    check("routes restored with the collection", route.get("video") == "stream_only", route)


def test_hotkey(h):
    print("Hotkeys")
    h.obs.call("TriggerHotkeyByName", {"hotkeyName": "selective_recording.toggle.main"})
    started = wait_for(lambda: h.status("main")["state"] == "recording", 5)
    check("hotkey starts recording", started)
    time.sleep(1.5)
    h.obs.call("TriggerHotkeyByName", {"hotkeyName": "selective_recording.toggle.main"})
    stopped = wait_for(lambda: h.status("main")["state"] == "idle", 10)
    check("hotkey stops recording", stopped)


def test_performance(h):
    print("Rendering overhead")
    before = h.obs.call("GetStats")
    h.obs.vendor("StartRecording")
    wait_for(lambda: all(c["state"] == "recording" for c in h.obs.vendor("GetStatus")["canvases"]), 10)
    time.sleep(8)
    during = h.obs.vendor("GetStatus")["canvases"]
    h.obs.vendor("StopRecording")
    wait_for(lambda: all(c["state"] == "idle" for c in h.obs.vendor("GetStatus")["canvases"]), 20)
    after = h.obs.call("GetStats")
    skipped = after["renderSkippedFrames"] - before["renderSkippedFrames"]
    lagged = after["outputSkippedFrames"] - before["outputSkippedFrames"]
    check("no render frames skipped with two recordings", skipped <= 2, f"{skipped} skipped")
    check("no encoder frames skipped with two recordings", lagged <= 2, f"{lagged} skipped")
    for c in during:
        check(f"{c['canvas']}: no dropped frames", c["dropped_frames"] == 0, c["dropped_frames"])
        check(f"{c['canvas']}: audio buffering under 250 ms", c["audio_latency_ms"] < 250,
              f"{c['audio_latency_ms']} ms")
    print(f"  OBS CPU {after['cpuUsage']:.1f}%, memory {after['memoryUsage']:.0f} MB, "
          f"average render {after['averageFrameRenderTime']:.2f} ms")


def test_disabled(h):
    print("Plugin disabled records everything as live")
    h.obs.vendor("SetEnabled", {"enabled": False})
    path = h.record("main", 3)
    h.obs.vendor("SetEnabled", {"enabled": True})
    check("disabled: stream-only box recorded", media.pixel(path, 1.5, 300, 300) == "green")
    check("disabled: hidden box not recorded", media.pixel(path, 1.5, 900, 300) == "red")


def connect(url):
    obs = Client(url, timeout=90)

    def ready():
        try:
            return len(obs.vendor("GetStatus").get("canvases", [])) > 1
        except RequestError:
            return False  # OBS answers "not ready" while it is still loading

    if not wait_for(ready, 90, interval=1):
        raise RuntimeError("the plugin never became ready")
    return obs


def test_crash_recovery(h, process, url):
    print("Crash recovery (FR-009)")
    h.obs.vendor("StartRecording", {"canvas": "main"})
    wait_for(lambda: h.status("main")["state"] == "recording", 10)
    path = h.status("main")["file"]
    time.sleep(4)
    h.obs.close()
    process.kill()

    process.start()
    h.obs = connect(url)
    found = h.obs.vendor("GetInterruptedRecordings")["recordings"]
    paths = [os.path.normcase(os.path.normpath(r["path"])) for r in found]
    check("interrupted recording reported after a crash", os.path.normcase(os.path.normpath(path)) in paths,
          f"{len(found)} reported")
    duration = float(media.probe(path)["format"].get("duration", 0))
    check("interrupted MKV is playable", duration > 2.0, f"{duration:.1f}s")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", default="ws://127.0.0.1:4466")
    parser.add_argument("--workdir", required=True)
    parser.add_argument("--obs-dir", required=True)
    args = parser.parse_args()
    os.makedirs(args.workdir, exist_ok=True)

    process = ObsProcess(args.obs_dir)
    process.start()
    h = Harness(connect(args.url), args.workdir)

    tests = [test_discovery, setup, test_main_recording, test_warnings, test_vertical,
             test_failure_isolation, test_pause, test_groups, test_studio_mode, test_transition_blend,
             test_source_removed, test_hotkey, test_av_sync, test_performance, test_disabled,
             test_collection_switch]
    for test in tests:
        try:
            test(h)
        except Exception as e:  # noqa: BLE001 - report and keep going
            check(f"{test.__name__} ran without errors", False, repr(e))

    try:
        test_crash_recovery(h, process, args.url)
    except Exception as e:  # noqa: BLE001
        check("test_crash_recovery ran without errors", False, repr(e))

    print("Clean shutdown")
    h.obs.close()
    closed = process.close()
    check("OBS exits cleanly", closed)
    log = process.latest_log()
    with open(log, encoding="utf-8", errors="replace") as f:
        text = f.read()
    leaks = [line for line in text.splitlines() if "Number of memory leaks" in line]
    check("no memory leaks at shutdown", leaks and leaks[-1].strip().endswith(": 0"),
          leaks[-1].strip() if leaks else "no leak line")
    check("plugin unloaded", "[obs-selective-recording] unloaded" in text)
    print(f"  log: {log}")

    failed = [name for name, ok in results if not ok]
    print(f"\n{len(results) - len(failed)}/{len(results)} checks passed")
    for name in failed:
        print(f"  FAILED: {name}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
