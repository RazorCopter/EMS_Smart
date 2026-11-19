"""Lightweight container & host resource reader (cgroup v2, /proc, disk)."""
from __future__ import annotations

from pathlib import Path
import os
import shutil
import time
from typing import Any


def _read_int(path: Path) -> int | None:
    try:
        raw = path.read_text(encoding="utf-8").strip()
        if not raw or raw == "max":
            return None
        return int(raw)
    except OSError:
        return None
    except ValueError:
        return None


def _read_cpu_usec() -> int | None:
    try:
        for line in Path("/sys/fs/cgroup/cpu.stat").read_text(encoding="utf-8").splitlines():
            if line.startswith("usage_usec "):
                return int(line.split()[1])
    except OSError:
        return None
    except (IndexError, ValueError):
        return None
    return None


def _read_process_rss_bytes() -> int | None:
    try:
        for line in Path("/proc/self/status").read_text(encoding="utf-8").splitlines():
            if line.startswith("VmRSS:"):
                kb = int(line.split()[1])
                return kb * 1024
    except OSError:
        return None
    except (IndexError, ValueError):
        return None
    return None


class ProcessResourceSampler:
    """Computes delta cgroup CPU % and reads memory/disk metrics cleanly."""

    def __init__(self, disk_path: str = "/") -> None:
        self.disk_path = disk_path
        self._prev_mono: float | None = None
        self._prev_cpu_usec: int | None = None
        self._last_cpu_pct: float | None = None

    def sample(self) -> dict[str, Any]:
        now = time.monotonic()
        cpu_usec = _read_cpu_usec()
        if cpu_usec is not None and self._prev_cpu_usec is not None and self._prev_mono is not None:
            dt = now - self._prev_mono
            if dt >= 0.5:
                dusec = cpu_usec - self._prev_cpu_usec
                ncpus = max(1, os.cpu_count() or 1)
                # Normalize both total-core % and per-core %; here 100% = 1 full vCPU core, matching docker stats
                self._last_cpu_pct = round(max(0.0, (dusec / (dt * 1_000_000.0)) * 100.0), 2)
                self._prev_mono = now
                self._prev_cpu_usec = cpu_usec
        elif cpu_usec is not None:
            self._prev_mono = now
            self._prev_cpu_usec = cpu_usec

        mem_cur = _read_int(Path("/sys/fs/cgroup/memory.current"))
        mem_peak = _read_int(Path("/sys/fs/cgroup/memory.peak"))
        mem_max = _read_int(Path("/sys/fs/cgroup/memory.max"))
        rss = _read_process_rss_bytes()

        disk_info: dict[str, Any] = {}
        try:
            du = shutil.disk_usage(self.disk_path)
            disk_info = {
                "disk_total_gb": round(du.total / (1024**3), 2),
                "disk_used_gb": round(du.used / (1024**3), 2),
                "disk_free_gb": round(du.free / (1024**3), 2),
                "disk_used_pct": round((du.used / du.total) * 100.0, 1) if du.total > 0 else None,
            }
        except OSError:
            pass

        return {
            "cpu_pct": self._last_cpu_pct,
            "cgroup_mem_mb": round(mem_cur / (1024 * 1024), 1) if mem_cur is not None else None,
            "cgroup_mem_peak_mb": round(mem_peak / (1024 * 1024), 1) if mem_peak is not None else None,
            "cgroup_mem_limit_mb": round(mem_max / (1024 * 1024), 1) if mem_max is not None else None,
            "process_rss_mb": round(rss / (1024 * 1024), 1) if rss is not None else None,
            **disk_info,
        }
