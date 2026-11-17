# SPDX-License-Identifier: GPL-2.0-or-later
"""Starts, closes and kills the portable OBS used by the integration tests (Windows)."""

import ctypes
import glob
import os
import subprocess
import time
from ctypes import wintypes

user32 = ctypes.windll.user32
WM_CLOSE = 0x0010


class ObsProcess:
    def __init__(self, obs_dir):
        self.obs_dir = obs_dir
        self.bin = os.path.join(obs_dir, "bin", "64bit")
        self.proc = None

    def start(self):
        self.proc = subprocess.Popen(
            [os.path.join(self.bin, "obs64.exe"), "--portable", "--multi", "--disable-shutdown-check",
             "--disable-updater"],
            cwd=self.bin)
        return self.proc.pid

    def _windows(self):
        handles = []

        @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
        def callback(hwnd, _):
            pid = wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            if pid.value == self.proc.pid and user32.IsWindowVisible(hwnd):
                handles.append(hwnd)
            return True

        user32.EnumWindows(callback, 0)
        return handles

    def close(self, timeout=30):
        """Asks OBS to exit normally so it runs its full shutdown."""
        if not self.proc or self.proc.poll() is not None:
            return True
        for hwnd in self._windows():
            user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
        try:
            self.proc.wait(timeout)
            return True
        except subprocess.TimeoutExpired:
            self.capture_hang_report()
            self.kill()
            return False

    def capture_hang_report(self):
        """Makes a hung OBS crash on purpose so its crash handler logs every thread's stack."""
        kernel32 = ctypes.windll.kernel32
        kernel32.OpenProcess.restype = wintypes.HANDLE
        kernel32.CreateRemoteThread.restype = wintypes.HANDLE
        kernel32.CreateRemoteThread.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p,
                                                ctypes.c_void_p, wintypes.DWORD, ctypes.c_void_p]
        handle = kernel32.OpenProcess(0x1F0FFF, False, self.proc.pid)
        if handle:
            kernel32.CreateRemoteThread(handle, None, 0, ctypes.c_void_p(0x10), None, 0, None)
            try:
                self.proc.wait(15)
            except subprocess.TimeoutExpired:
                pass
            kernel32.CloseHandle(handle)

    def latest_crash(self):
        crashes = glob.glob(os.path.join(self.obs_dir, "config", "obs-studio", "crashes", "*.txt"))
        return max(crashes, key=os.path.getmtime) if crashes else None

    def kill(self):
        if self.proc and self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait(10)

    def latest_log(self):
        logs = glob.glob(os.path.join(self.obs_dir, "config", "obs-studio", "logs", "*.txt"))
        return max(logs, key=os.path.getmtime) if logs else None

    def wait_for_log(self, text, timeout=30):
        deadline = time.time() + timeout
        while time.time() < deadline:
            log = self.latest_log()
            if log:
                with open(log, encoding="utf-8", errors="replace") as f:
                    if text in f.read():
                        return True
            time.sleep(0.5)
        return False
