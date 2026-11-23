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
    # Some third-party services watch for processes named obs64.exe and take them over: the
    # Streamlabs plugin's sl-browser-service injects its plugin and restarts OBS without
    # --portable. A copy of obs64.exe under this name in the test OBS avoids that.
    TEST_EXE = "obs64-sr-test.exe"

    def __init__(self, obs_dir):
        self.obs_dir = obs_dir
        self.bin = os.path.join(obs_dir, "bin", "64bit")
        renamed = os.path.join(self.bin, self.TEST_EXE)
        self.exe = renamed if os.path.exists(renamed) else os.path.join(self.bin, "obs64.exe")
        self.proc = None

    def _pids_from_this_folder(self):
        """PIDs of every OBS running from this OBS folder, including ones the runner didn't start."""
        kernel32 = ctypes.windll.kernel32
        psapi = ctypes.windll.psapi
        kernel32.OpenProcess.restype = wintypes.HANDLE
        exes = {os.path.normcase(os.path.abspath(os.path.join(self.bin, name))) for name in ("obs64.exe", self.TEST_EXE)}
        pids = (wintypes.DWORD * 4096)()
        needed = wintypes.DWORD()
        psapi.EnumProcesses(ctypes.byref(pids), ctypes.sizeof(pids), ctypes.byref(needed))
        found = []
        for pid in pids[:needed.value // ctypes.sizeof(wintypes.DWORD)]:
            handle = kernel32.OpenProcess(0x1000, False, pid)  # PROCESS_QUERY_LIMITED_INFORMATION
            if not handle:
                continue
            path = ctypes.create_unicode_buffer(1024)
            size = wintypes.DWORD(len(path))
            if kernel32.QueryFullProcessImageNameW(handle, 0, path, ctypes.byref(size)):
                if os.path.normcase(path.value) in exes:
                    found.append(pid)
            kernel32.CloseHandle(handle)
        return found

    def _close_strays(self):
        """Closes other OBS instances using this folder; two would share its config and port.

        A plugin with a watchdog, such as Streamlabs' (sl-browser-service), restarts an OBS that
        exits uncleanly, and without --portable, so such a copy loads every ProgramData plugin.
        It's closed the way a user would, so the watchdog doesn't start it again.
        """
        strays = [pid for pid in self._pids_from_this_folder() if not self.proc or pid != self.proc.pid]
        if not strays:
            return
        print(f"  closing other OBS instances running from {self.obs_dir}: {strays}", flush=True)
        for pid in strays:
            hwnd = self._main_window(pid)
            if hwnd:
                user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
        deadline = time.time() + 30
        while time.time() < deadline and any(p in self._pids_from_this_folder() for p in strays):
            time.sleep(0.5)
        left = [p for p in strays if p in self._pids_from_this_folder()]
        if left:
            raise RuntimeError(f"another OBS is running from the test folder (PID {left}) and didn't close; "
                               "close it, then run the tests again")

    def start(self):
        self._close_strays()
        self.proc = subprocess.Popen(
            [self.exe, "--portable", "--multi", "--disable-shutdown-check",
             "--disable-updater"],
            cwd=self.bin)
        return self.proc.pid

    def windows(self, pid=None):
        """(hwnd, title) for each visible top-level window of the test OBS (or another PID)."""
        found = []
        pid_wanted = pid or self.proc.pid

        @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
        def callback(hwnd, _):
            pid = wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            if pid.value == pid_wanted and user32.IsWindowVisible(hwnd):
                title = ctypes.create_unicode_buffer(256)
                user32.GetWindowTextW(hwnd, title, 256)
                found.append((hwnd, title.value))
            return True

        user32.EnumWindows(callback, 0)
        return found

    def _main_window(self, pid=None):
        """OBS's main window; floating docks are separate windows and must not be closed."""
        found = [hwnd for hwnd, title in self.windows(pid) if title.startswith("OBS ")]
        return found[0] if found else None

    def close(self, timeout=30):
        """Closes the main window like a user would, so OBS runs its full shutdown."""
        if not self.proc or self.proc.poll() is not None:
            return True
        hwnd = self._main_window()
        if hwnd:
            user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
        try:
            self.proc.wait(timeout)
            return True
        except subprocess.TimeoutExpired:
            # A modal dialog is the usual cause; its title says which one.
            print(f"  OBS did not exit; open windows: {[title for _, title in self.windows()]}", flush=True)
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
