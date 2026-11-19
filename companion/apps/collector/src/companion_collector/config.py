from __future__ import annotations

from pydantic_settings import BaseSettings, SettingsConfigDict


class CollectorSettings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    database_url: str = "postgresql+asyncpg://afore_companion:afore_companion@timescaledb:5432/afore_companion"
    device_id: str = "afore-ems"

    mqtt_enabled: bool = True
    mqtt_host: str = "10.0.10.10"
    mqtt_port: int = 1883
    mqtt_tls: bool = False
    mqtt_user: str = ""
    mqtt_password: str = ""
    mqtt_client_id: str = "afore-ems-companion"
    mqtt_base_topic: str = "afore/afore-ems/v1"
    mqtt_sem_topic_prefix: str = "afore/afore-ems/v1/sem"

    esp32_enabled: bool = True
    esp32_url: str = "http://10.0.0.110"
    esp32_username: str = "admin"
    esp32_password: str = ""
    esp32_poll_health_s: int = 30
    esp32_backfill_interval_s: int = 60

    ha_rest_enabled: bool = False
    ha_url: str = "http://homeassistant.local:8123"
    ha_token: str = ""

    max_energy_gap_s: float = 60.0
    sem_concordance_watt_threshold: float = 100.0
    power_deadband_w: float = 20.0
    battery_nominal_v: float = 51.2
    metrics_port: int = 9101
