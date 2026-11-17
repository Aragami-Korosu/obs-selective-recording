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


def pixel(path, seconds, x, y):
    """Colour name of the pixel at (x, y) at a time in the recording."""
    raw = subprocess.run(
        ["ffmpeg", "-v", "error", "-ss", f"{seconds:.3f}", "-i", path,
         "-vf", f"crop=2:2:{x}:{y}", "-frames:v", "1", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"],
        check=True, capture_output=True).stdout
    if len(raw) < 3:
        raise RuntimeError(f"no frame at {seconds}s in {path}")
    return nearest_color(tuple(raw[:3]))


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


def make_tone(path, freq, seconds=20):
    subprocess.run(
        ["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i",
         f"sine=frequency={freq}:sample_rate=48000:duration={seconds}",
         "-af", "volume=0.3", "-ac", "2", path],
        check=True)
