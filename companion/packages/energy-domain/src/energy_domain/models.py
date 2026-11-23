from __future__ import annotations

from datetime import datetime, timezone
from enum import StrEnum
from typing import Any

from pydantic import BaseModel, ConfigDict, Field, field_validator


class OperatingMode(StrEnum):
    SHADOW = "SHADOW"


class StrategyName(StrEnum):
    SELF_USE = "SELF_USE"
    CHARGE_FIRST = "CHARGE_FIRST"
    NATIVE_INVERTER = "NATIVE_INVERTER"
    SAFE_MODE = "SAFE_MODE"
    TARGET_SOC = "TARGET_SOC"
    MAINTENANCE = "MAINTENANCE"


class QualityState(StrEnum):
    VALID = "valid"
    STALE = "stale"
    UNAVAILABLE = "unavailable"
    UNKNOWN = "unknown"


class ProvenanceKind(StrEnum):
    MEASURED = "measured"
    DERIVED = "derived"
    FORECAST = "forecast"
    SIMULATED = "simulated"
    CONFIGURED = "configured"
    PROVISIONAL = "provisional"
    UNAVAILABLE = "unavailable"


def parse_utc_timestamp(value: datetime | str | int | float | None) -> datetime | None:
    if value is None:
        return None
    if isinstance(value, datetime):
        return value if value.tzinfo else value.replace(tzinfo=timezone.utc)
    if isinstance(value, (int, float)):
        return datetime.fromtimestamp(float(value), tz=timezone.utc)
    if isinstance(value, str):
        s = value.strip()
        if not s:
            return None
        if s.endswith("Z"):
            s = s[:-1] + "+00:00"
        dt = datetime.fromisoformat(s)
        return dt if dt.tzinfo else dt.replace(tzinfo=timezone.utc)
    return None


class ChannelQuality(BaseModel):
    model_config = ConfigDict(extra="allow")

    age_ms: int | None = None
    source: str = "measured"
    state: str = "unavailable"


class TelemetryPayload(BaseModel):
    """Exact payload published by ESP32 v5.1 on /api/v1/telemetry and afore/.../telemetry."""

    model_config = ConfigDict(extra="allow")

    quality: dict[str, ChannelQuality] = Field(default_factory=dict)
    soc: float | None = None
    battery_w: float | None = None
    battery_v: float | None = None
    battery_a: float | None = None
    pv_w: float | None = None
    pv1_v: float | None = None
    pv1_a: float | None = None
    pv1_w: float | None = None
    pv2_v: float | None = None
    pv2_a: float | None = None
    pv2_w: float | None = None
    grid_w: float | None = None
    grid_v: float | None = None
    hz: float | None = None
    load_w: float | None = None
    inverter_w: float | None = None
    temp_c: float | None = None

    reads: int | None = None
    errors: int | None = None
    fault: bool | None = None
    telemetry_valid: bool | None = None
    shadow: bool | None = True
    aux1: str | None = None
    modbus_write_count: int = 0
    mqtt: bool | None = None

    timestamp: datetime | None = None
    uptime_ms: int | None = None
    time_source: str | None = None
    time_valid: bool | None = None

    @field_validator("timestamp", mode="before")
    @classmethod
    def _parse_ts(cls, v: Any) -> datetime | None:
        return parse_utc_timestamp(v)


class HaInputStatus(BaseModel):
    model_config = ConfigDict(extra="allow")

    status: str = "unavailable"
    age_s: int | None = None
    used: bool = False
    accepted: int = 0
    rejected: int = 0
    last_rejection: str | None = None
    valid_until: int | None = None
    expires_at: datetime | None = None
    data: dict[str, Any] | None = None

    @field_validator("expires_at", mode="before")
    @classmethod
    def _parse_exp(cls, v: Any) -> datetime | None:
        return parse_utc_timestamp(v)


class HaInputsSummary(BaseModel):
    model_config = ConfigDict(extra="allow")

    ha_online: bool = False
    forecast_solar: HaInputStatus | None = None
    forecast_ratio: HaInputStatus | None = None
    weather: HaInputStatus | None = None
    load_profile: HaInputStatus | None = None
    heartbeat: HaInputStatus | None = None


class DecisionAuditPayload(BaseModel):
    """Canonical decision audit payload from ESP32 v5.1 ems/state, ems/decision, and /api/v1/ems/decision."""

    model_config = ConfigDict(extra="allow")

    schema_version: int = Field(default=1, alias="schema")
    source: str = "MEASURED"
    boot_id: str | None = None
    seq: int
    timestamp: datetime | None = None
    uptime_ms: int
    time_source: str = "UNSYNCED"
    time_valid: bool = False

    strategy: str
    configured: str
    previous: str
    state: str
    transition: str

    soc: float | None = None
    grid_w: float | None = None
    battery_w: float | None = None
    load_w: float | None = None
    pv_w: float | None = None
    quality: dict[str, str] = Field(default_factory=dict)

    safety_reason: str = "NONE"
    limited_w: float | None = None
    candidate_w: float | None = None
    simulated_w: float | None = None
    safety_min_w: float | None = None
    safety_max_w: float | None = None
    missing_wh: float | None = None
    average_w: float | None = None
    unreachable: bool = False
    reason: str = "UNKNOWN"

    executed: bool = False
    applied: Any = None
    aux1: str | None = None
    modbus_write_count: int = 0
    ha_inputs: HaInputsSummary | None = None

    @field_validator("timestamp", mode="before")
    @classmethod
    def _parse_ts(cls, v: Any) -> datetime | None:
        return parse_utc_timestamp(v)

    @field_validator("executed")
    @classmethod
    def _enforce_shadow_executed(cls, v: bool) -> bool:
        if v:
            raise ValueError("Companion only supports SHADOW audits (executed must be false)")
        return False

    @field_validator("modbus_write_count")
    @classmethod
    def _enforce_zero_writes(cls, v: int) -> int:
        if v != 0:
            raise ValueError("SHADOW audit modbus_write_count must be 0")
        return 0


class DiagnosticsPayload(BaseModel):
    model_config = ConfigDict(extra="allow")

    rssi: int | None = None
    heap: int | None = None
    heap_total: int | None = None
    heap_min: int | None = None
    heap_max_alloc: int | None = None
    heap_used_pct: float | None = None
    uptime_ms: int | None = None
    psram: int | None = None
    psram_free: int | None = None
    psram_min: int | None = None
    psram_used_pct: float | None = None
    cpu_pct: float | None = None
    cpu0_pct: float | None = None
    cpu1_pct: float | None = None
    cpu_mhz: int | None = None
    task_count: int | None = None
    min_task_stack_free_b: int | None = None
    nvs_used_entries: int | None = None
    nvs_free_entries: int | None = None
    nvs_total_entries: int | None = None
    nvs_used_pct: float | None = None
    flash_size: int | None = None
    sketch_size: int | None = None
    sketch_free: int | None = None
    reset_reason: int | None = None
    aux1: str | None = None
    firmware: str | None = None
    reads: int | None = None
    errors: int | None = None
    modbus_write_count: int = 0
    timestamp: datetime | None = None
    time_source: str | None = None
    time_valid: bool | None = None

    @field_validator("timestamp", mode="before")
    @classmethod
    def _parse_ts(cls, v: Any) -> datetime | None:
        return parse_utc_timestamp(v)


class ConfigStatePayload(BaseModel):
    model_config = ConfigDict(extra="allow")

    strategy: int | None = None
    strategy_name: str | None = None
    fallback: int | None = None
    soc_min: int | None = None
    soc_max: int | None = None
    soc_target: int | None = None
    soc_resume: int | None = None
    charge_w: int | None = None
    discharge_w: int | None = None
    charge_a: int | None = None
    discharge_a: int | None = None
    voltage_min: int | None = None
    voltage_max: int | None = None
    capacity_wh: int | None = None
    nominal_mv: int | None = None
    efficiency_pct: int | None = None
    deadband_w: int | None = None
    ramp_w_s: int | None = None
    export_limit_w: int | None = None
    grid_target_w: int | None = None
    revision: int | None = None
    battery_profile_source: str | None = None
    battery_model_provisional: bool | None = None
    efficiency_provisional: bool | None = None
    timestamp: datetime | None = None

    @field_validator("timestamp", mode="before")
    @classmethod
    def _parse_ts(cls, v: Any) -> datetime | None:
        return parse_utc_timestamp(v)


class ManualStatePayload(BaseModel):
    model_config = ConfigDict(extra="allow")

    operating_mode: str | None = None
    live_lock: str | None = None
    aux1: str | None = None
    physical_feedback: Any = None
    last_aux_origin: str | None = None
    last_aux_action: str | None = None
    last_aux_reason: str | None = None
    aux_auto_off_ms: int | None = None
    timestamp: datetime | None = None
    uptime_ms: int | None = None

    @field_validator("timestamp", mode="before")
    @classmethod
    def _parse_ts(cls, v: Any) -> datetime | None:
        return parse_utc_timestamp(v)


class ExternalInputEnvelope(BaseModel):
    model_config = ConfigDict(extra="allow")

    schema_version: int = Field(default=2, alias="schema")
    source: str = "home_assistant"
    message_id: str | None = None
    timestamp: datetime | None = None
    valid_until: datetime | None = None
    quality: str | None = None
    units: dict[str, Any] = Field(default_factory=dict)
    payload: dict[str, Any] | None = None
    data: dict[str, Any] | None = None

    @field_validator("timestamp", "valid_until", mode="before")
    @classmethod
    def _parse_dt(cls, v: Any) -> datetime | None:
        return parse_utc_timestamp(v)


class AuxEventPayload(BaseModel):
    model_config = ConfigDict(extra="allow")

    schema_version: int | None = Field(default=None, alias="schema")
    message_id: str | None = None
    accepted: bool | None = None
    reason: str | None = None
    executed: bool = False
    aux1: str | None = None
    physical_feedback: Any = None
    timestamp: datetime | None = None

    @field_validator("timestamp", mode="before")
    @classmethod
    def _parse_ts(cls, v: Any) -> datetime | None:
        return parse_utc_timestamp(v)


class SemSnapshotPayload(BaseModel):
    """Snapshot published by Home Assistant for SEM/ESP comparison."""

    model_config = ConfigDict(extra="allow")

    timestamp: datetime | None = None
    sem_decision: str | None = None
    sem_amps: float | None = None
    sem_reason: str | None = None
    solarman_mode: str | None = None
    solarman_charge_a: float | None = None
    sem_automation_enabled: bool | None = None
    forecast_ok: bool | None = None
    today_ok: bool | None = None
    tomorrow_ok: bool | None = None
    fog: bool | None = None
    soc: float | None = None

    @field_validator("timestamp", mode="before")
    @classmethod
    def _parse_ts(cls, v: Any) -> datetime | None:
        return parse_utc_timestamp(v)


class NormalizedTelemetry(BaseModel):
    device_id: str
    boot_id: str
    source_timestamp: datetime
    received_at: datetime
    time_valid: bool
    time_source: str
    uptime_ms: int | None
    soc: float | None = None
    battery_w: float | None = None
    battery_v: float | None = None
    battery_a: float | None = None
    pv_w: float | None = None
    pv1_v: float | None = None
    pv1_a: float | None = None
    pv1_w: float | None = None
    pv2_v: float | None = None
    pv2_a: float | None = None
    pv2_w: float | None = None
    grid_w: float | None = None
    grid_v: float | None = None
    hz: float | None = None
    load_w: float | None = None
    inverter_w: float | None = None
    temp_c: float | None = None
    reads: int | None = None
    errors: int | None = None
    fault: bool | None = None
    telemetry_valid: bool | None = None
    shadow: bool = True
    aux1: str | None = None
    modbus_write_count: int = 0
    mqtt_connected: bool | None = None
    quality_json: dict[str, Any] = Field(default_factory=dict)
    provenance: str = "mqtt"
    source_topic: str


class NormalizedDecision(BaseModel):
    device_id: str
    boot_id: str
    seq: int
    source_timestamp: datetime
    received_at: datetime
    time_valid: bool
    time_source: str
    uptime_ms: int
    data_source: str
    strategy: str
    configured_strategy: str
    previous_strategy: str
    state: str
    transition: str
    soc: float | None = None
    grid_w: float | None = None
    battery_w: float | None = None
    load_w: float | None = None
    pv_w: float | None = None
    candidate_w: float | None = None
    limited_w: float | None = None
    simulated_w: float | None = None
    safety_min_w: float | None = None
    safety_max_w: float | None = None
    missing_wh: float | None = None
    average_w: float | None = None
    unreachable: bool = False
    safety_reason: str = "NONE"
    reason: str = "UNKNOWN"
    executed: bool = False
    applied: Any = None
    aux1: str | None = None
    modbus_write_count: int = 0
    quality_json: dict[str, Any] = Field(default_factory=dict)
    ha_inputs_json: dict[str, Any] | None = None
    raw_payload_json: dict[str, Any] = Field(default_factory=dict)
