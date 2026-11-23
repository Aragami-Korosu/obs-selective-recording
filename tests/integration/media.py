# SPDX-License-Identifier: GPL-2.0-or-later
"""Helpers that inspect recordings with ffmpeg."""

import json
import subprocess

import numpy as np

COLORS = {
    "red": (255, 0, 0),
    "green": (0, 255, 0),
    "blue": (0, 0, 255),
    "yellow": (255, 255, 0),
    "magenta": (255, 0, 255),
    "cyan": (0, 255, 255),
    "black": (0, 0, 0),
}


def nearest_color(rgb):
    return min(COLORS, key=lambda name: sum((a - b) ** 2 for a, b in zip(rgb, COLORS[name])))


def probe(path):
    out = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries",
         "stream=codec_type,codec_name,width,height,sample_rate,channels:format=duration",
         "-of", "json", path],
        check=True, capture_output=True, text=True).stdout
    return json.loads(out)


def video_stream(info):
    return next(s for s in info["streams"] if s["codec_type"] == "video")


def has_audio(info):
    return any(s["codec_type"] == "audio" for s in info["streams"])


def pixel_rgb(path, seconds, x, y):
    """RGB of the pixel at (x, y) at a time in the recording."""
    raw = subprocess.run(
        ["ffmpeg", "-v", "error", "-ss", f"{seconds:.3f}", "-i", path,
         "-vf", f"crop=2:2:{x}:{y}", "-frames:v", "1", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"],
        check=True, capture_output=True).stdout
    if len(raw) < 3:
        raise RuntimeError(f"no frame at {seconds}s in {path}")
    return tuple(raw[:3])


def pixel(path, seconds, x, y):
    """Colour name of the pixel at (x, y) at a time in the recording."""
    return nearest_color(pixel_rgb(path, seconds, x, y))


def tone_levels(path, freqs, start=0.5, duration=2.0):
    """Level in dB of each frequency relative to the strongest bin."""
    raw = subprocess.run(
        ["ffmpeg", "-v", "error", "-ss", f"{start}", "-t", f"{duration}", "-i", path,
         "-vn", "-ac", "1", "-ar", "48000", "-f", "f32le", "-"],
        check=True, capture_output=True).stdout
    samples = np.frombuffer(raw, dtype=np.float32)
    if samples.size < 4800:
        raise RuntimeError(f"not enough audio in {path}")
    spectrum = np.abs(np.fft.rfft(samples * np.hanning(samples.size)))
    hz = np.fft.rfftfreq(samples.size, 1 / 48000)
    peak = spectrum.max() + 1e-12
    levels = {}
    for f in freqs:
        band = spectrum[(hz > f - 5) & (hz < f + 5)]
        levels[f] = 20 * np.log10((band.max() if band.size else 0) / peak + 1e-12)
    levels["rms"] = float(np.sqrt(np.mean(samples ** 2)))
    return levels


def make_sync_clip(path, seconds=20):
    """Black video that flashes white for one frame each second, with a 3 kHz beep at the same instant."""
    subprocess.run(
        ["ffmpeg", "-v", "error", "-y",
         "-f", "lavfi", "-i", f"color=c=black:s=320x180:r=30:d={seconds}",
         "-f", "lavfi", "-i",
         f"aevalsrc='if(lt(mod(t,1),0.05),0.5*sin(2*PI*3000*t),0)':s=48000:d={seconds}",
         "-vf", "geq=lum='if(lt(mod(T,1),0.03),235,16)':cb=128:cr=128",
         "-c:v", "libx264", "-preset", "ultrafast", "-pix_fmt", "yuv420p", "-c:a", "aac", "-shortest", path],
        check=True)


def flash_times(path, x, y, w, h, fps=30.0):
    """Times of frames where a region turns bright."""
    raw = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", path, "-vf", f"crop={w}:{h}:{x}:{y},scale=1:1,format=gray",
         "-f", "rawvideo", "-"],
        check=True, capture_output=True).stdout
    values = np.frombuffer(raw, dtype=np.uint8)
    times = []
    for i in range(1, values.size):
        if values[i] > 128 and values[i - 1] <= 128:
            times.append(i / fps)
    return times


def beep_times(path):
    """Onset times of 3 kHz beeps."""
    raw = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", path, "-vn", "-ac", "1", "-ar", "48000",
         "-af", "highpass=f=2500,highpass=f=2500", "-f", "f32le", "-"],
        check=True, capture_output=True).stdout
    samples = np.abs(np.frombuffer(raw, dtype=np.float32))
    window = 96  # 2 ms
    envelope = np.convolve(samples, np.ones(window) / window, mode="same")
    threshold = envelope.max() * 0.3
    onsets = []
    above = False
    for i in range(0, envelope.size, 24):
        if envelope[i] > threshold and not above:
            onsets.append(i / 48000)
            above = True
        elif envelope[i] < threshold * 0.5:
            above = False
    return onsets


def sync_offsets(flashes, beeps):
    """Audio minus video offset for each flash that has a beep within half a second."""
    offsets = []
    for f in flashes:
        nearest = min(beeps, key=lambda b: abs(b - f), default=None)
        if nearest is not None and abs(nearest - f) < 0.5:
            offsets.append(nearest - f)
    return offsets


def make_tone(path, freq, seconds=20):
    subprocess.run(
        ["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i",
         f"sine=frequency={freq}:sample_rate=48000:duration={seconds}",
         "-af", "volume=0.3", "-ac", "2", path],
        check=True)
