# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Read-only process memory samples for isolated resource tests."""

import ctypes
import os
from pathlib import Path
import subprocess
import sys


def process_memory(pid):
    if os.name == "nt":
        from ctypes import wintypes

        class Counters(ctypes.Structure):
            _fields_ = [("cb", wintypes.DWORD), ("faults", wintypes.DWORD)] + [
                (name, ctypes.c_size_t) for name in ("peak", "working", "pool_peak",
                    "pool", "nonpaged_peak", "nonpaged", "pagefile", "pagefile_peak", "private")]

        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        query = kernel.K32GetProcessMemoryInfo
        query.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
        handle = kernel.OpenProcess(0x0400 | 0x0010, False, pid)
        if not handle:
            return {}
        try:
            counters = Counters()
            counters.cb = ctypes.sizeof(counters)
            if query(handle, ctypes.byref(counters), counters.cb):
                return {"resident_bytes": counters.working, "private_bytes": counters.private}
            return {}
        finally:
            kernel.CloseHandle(handle)
    status = Path("/proc/{}/status".format(pid))
    if status.exists():
        values = {}
        names = {"VmRSS": "resident_bytes", "RssAnon": "anonymous_resident_bytes",
                 "VmSize": "virtual_bytes"}
        for line in status.read_text().splitlines():
            key, _, value = line.partition(":")
            if key in names:
                values[names[key]] = int(value.split()[0]) * 1024
        return values
    if sys.platform == "darwin":
        result = subprocess.run(["ps", "-o", "rss=", "-p", str(pid)],
                                capture_output=True, text=True, check=True)
        return {"resident_bytes": int(result.stdout.strip()) * 1024}
    return {}


def resident_bytes(pid):
    return process_memory(pid).get("resident_bytes", 0)
