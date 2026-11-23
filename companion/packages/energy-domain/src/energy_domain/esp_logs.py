from pathlib import Path


ESP_LOG_DIR = Path("/var/lib/companion/esp-logs")


def esp_log_segments(directory: Path = ESP_LOG_DIR) -> list[Path]:
    return sorted(directory.glob("esp-????????????????????.log"))
