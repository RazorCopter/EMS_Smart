from __future__ import annotations

from pydantic_settings import BaseSettings, SettingsConfigDict


class ApiSettings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    companion_version: str = "0.1.0"
    companion_timezone: str = "Europe/Rome"
    companion_locale: str = "it-IT"
    database_url: str = "postgresql+asyncpg://afore_companion:afore_companion@timescaledb:5432/afore_companion"
    device_id: str = "afore-ems"

    companion_secret_key: str = "dev-only-override-in-env"
    companion_session_ttl_hours: int = 24
    companion_cookie_secure: bool = False
    companion_admin_user: str = "admin"
    companion_admin_password: str = "AforeCompanionAdmin!2026"
    companion_viewer_user: str = "viewer"
    companion_viewer_password: str = "AforeCompanionViewer!2026"

    mqtt_enabled: bool = True
    mqtt_host: str = "10.0.10.10"
    mqtt_port: int = 1883
    mqtt_user: str = ""
    mqtt_password: str = ""
    mqtt_base_topic: str = "afore/afore-ems/v1"

    esp32_enabled: bool = True
    esp32_url: str = "http://10.0.0.110"
    esp32_username: str = "admin"
    esp32_password: str = ""

    ha_rest_enabled: bool = False
    ha_url: str = "http://homeassistant.local:8123"
    ha_token: str = ""

    retention_raw_mqtt_days: int = 30
    retention_telemetry_days: int = 90
    retention_agg_1m_days: int = 730
    backup_retention_days: int = 14

    sem_concordance_watt_threshold: float = 100.0
    power_deadband_w: float = 20.0
    battery_capacity_wh: float = 10240.0
    battery_nominal_v: float = 51.2
