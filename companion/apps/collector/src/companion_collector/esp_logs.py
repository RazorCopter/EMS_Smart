from __future__ import annotations

import asyncio
from datetime import datetime, timezone
import json
import logging
from pathlib import Path

from energy_domain.esp_logs import ESP_LOG_DIR, esp_log_segments

from .rest_client import Esp32ReadOnlyClient


logger = logging.getLogger(__name__)
SEGMENT_BYTES = 32 * 1024**2
SEGMENT_COUNT = 8


def log_position(line: str) -> tuple[int, int, float | None]:
    sequence, uptime, _ = line.split(" ", 2)
    seq, millis = int(sequence), int(uptime)
    boot = None
    _, separator, metadata = line.rpartition(" {")
    if separator:
        try:
            stamp = json.loads("{" + metadata)
            if stamp.get("time_valid"):
                boot = datetime.fromisoformat(stamp["timestamp"]).timestamp() - millis / 1000
        except (ValueError, KeyError, TypeError):
            pass
    return seq, millis, boot


class EspLogArchive:
    """One collector appends logs. API readers can stream immutable prefixes."""

    def __init__(self, directory: Path = ESP_LOG_DIR) -> None:
        self.directory = directory
        self.directory.mkdir(parents=True, exist_ok=True)
        self.segments = esp_log_segments(directory)
        self.active: Path | None = None
        self.bytes_used = 0
        self.position: tuple[int, int, float | None] | None = None
        for segment in reversed(self.segments):
            with segment.open("rb") as stream:
                offset = max(0, segment.stat().st_size - 4096)
                stream.seek(offset)
                tail = stream.read()
            if segment == self.segments[-1] and (not tail or tail.endswith(b"\n")):
                self.active = segment
                self.bytes_used = offset + len(tail)
            # An interrupted write can leave a partial final line.
            for line in reversed(tail.split(b"\n")[int(offset != 0):-1]):
                try:
                    self.position = log_position(line.decode("utf-8"))
                    break
                except (ValueError, UnicodeError):
                    continue
            if self.position:
                break

    def append(self, data: bytes) -> None:
        if not data:
            return
        if len(data) > SEGMENT_BYTES:
            raise ValueError("Diagnostic batch exceeds segment size")
        if self.active is None or self.bytes_used + len(data) > SEGMENT_BYTES:
            number = int(self.segments[-1].stem[4:]) + 1 if self.segments else 0
            while len(self.segments) >= SEGMENT_COUNT:
                self.segments[0].unlink()
                self.segments.pop(0)
            new_file = self.directory / f"esp-{number:020}.log"
            with new_file.open("xb"):
                pass
            self.active = new_file
            self.segments.append(new_file)
            self.bytes_used = 0
        try:
            with self.active.open("ab") as stream:
                stream.write(data)
        except OSError:
            self.active = None
            raise
        self.bytes_used += len(data)

    def ingest(self, lines: list[str]) -> None:
        if not lines:
            return
        positions = [log_position(line) for line in lines]
        latest = positions[-1]
        previous = self.position
        records = []
        stamp = datetime.now(timezone.utc).isoformat()
        if previous:
            overlaps = any(position[:2] == previous[:2] for position in positions)
            boot_changed = previous[2] is not None and latest[2] is not None and abs(previous[2] - latest[2]) > 5
            if latest[0] < previous[0] or latest[1] < previous[1] or (boot_changed and not overlaps):
                records.append(f"# {stamp} ESP_RESTART_OR_CLOCK_CHANGE")
                previous = None
        for line, current in zip(lines, positions):
            if previous and current[0] <= previous[0]:
                continue
            if previous and current[0] > previous[0] + 1:
                records.append(f"# {stamp} GAP first={previous[0] + 1} last={current[0] - 1}")
            records.append(line)
            previous = current
        self.append("".join(line + "\n" for line in records).encode())
        self.position = previous


def diagnostic(event: str) -> bytes:
    return f"# {datetime.now(timezone.utc).isoformat()} {event}\n".encode()


async def esp_log_loop(client: Esp32ReadOnlyClient) -> None:
    archive = None
    failed = False
    while True:
        try:
            if archive is None:
                archive = await asyncio.to_thread(EspLogArchive)
            lines = await client.fetch_logs()
            if failed:
                await asyncio.to_thread(archive.append, diagnostic("COLLECTION_RESUMED"))
            await asyncio.to_thread(archive.ingest, lines)
        except Exception as exc:
            if not failed:
                logger.warning("ESP log capture interrupted: %s", type(exc).__name__)
                if archive is not None:
                    try:
                        await asyncio.to_thread(archive.append, diagnostic(f"COLLECTION_INTERRUPTED {type(exc).__name__}"))
                    except OSError:
                        pass
            failed = True
        else:
            if failed:
                logger.info("ESP log capture resumed")
            failed = False
        await asyncio.sleep(2)
