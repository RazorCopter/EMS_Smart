from __future__ import annotations

from datetime import datetime
from typing import Any

from sqlalchemy import (
    BigInteger,
    Boolean,
    DateTime,
    Float,
    Index,
    Integer,
    JSON,
    String,
    Text,
    UniqueConstraint,
    func,
)
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


JSONType = JSON().with_variant(JSONB(), "postgresql")


class Base(DeclarativeBase):
    pass


class Device(Base):
    __tablename__ = "devices"

    device_id: Mapped[str] = mapped_column(String(64), primary_key=True)
    name: Mapped[str] = mapped_column(String(128), default="Afore Autonomous EMS")
    manufacturer: Mapped[str] = mapped_column(String(64), default="Afore")
    model: Mapped[str | None] = mapped_column(String(64), nullable=True)
    firmware_version: Mapped[str | None] = mapped_column(String(64), nullable=True)
    operating_mode: Mapped[str] = mapped_column(String(32), default="SHADOW")
    current_boot_id: Mapped[str | None] = mapped_column(String(64), nullable=True)
    last_seq: Mapped[int | None] = mapped_column(BigInteger, nullable=True)
    last_uptime_ms: Mapped[int | None] = mapped_column(BigInteger, nullable=True)
    last_seen_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    metadata_json: Mapped[dict[str, Any]] = mapped_column(JSONType, default=dict)


class MqttMessage(Base):
    __tablename__ = "mqtt_messages"

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    received_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), primary_key=True, server_default=func.now()
    )
    topic: Mapped[str] = mapped_column(String(255), index=True)
    qos: Mapped[int] = mapped_column(Integer, default=0)
    retain: Mapped[bool] = mapped_column(Boolean, default=False)
    payload_bytes: Mapped[int] = mapped_column(Integer, default=0)
    payload_json: Mapped[dict[str, Any] | None] = mapped_column(JSONType, nullable=True)
    raw_text: Mapped[str | None] = mapped_column(Text, nullable=True)
    schema_valid: Mapped[bool] = mapped_column(Boolean, default=True)
    validation_error: Mapped[str | None] = mapped_column(String(255), nullable=True)


class TelemetrySample(Base):
    __tablename__ = "telemetry_samples"
    __table_args__ = (
        UniqueConstraint("device_id", "source_timestamp", "uptime_ms", name="uq_telemetry_dev_ts_up"),
        Index("ix_telemetry_device_ts", "device_id", "source_timestamp"),
    )

    source_timestamp: Mapped[datetime] = mapped_column(DateTime(timezone=True), primary_key=True)
    device_id: Mapped[str] = mapped_column(String(64), primary_key=True)
    uptime_ms: Mapped[int] = mapped_column(BigInteger, primary_key=True, default=0)
    received_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    boot_id: Mapped[str] = mapped_column(String(64), default="unknown")
    time_source: Mapped[str] = mapped_column(String(16), default="UNSYNCED")
    time_valid: Mapped[bool] = mapped_column(Boolean, default=False)

    soc: Mapped[float | None] = mapped_column(Float, nullable=True)
    battery_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    battery_v: Mapped[float | None] = mapped_column(Float, nullable=True)
    battery_a: Mapped[float | None] = mapped_column(Float, nullable=True)
    pv_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    pv1_v: Mapped[float | None] = mapped_column(Float, nullable=True)
    pv1_a: Mapped[float | None] = mapped_column(Float, nullable=True)
    pv1_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    pv2_v: Mapped[float | None] = mapped_column(Float, nullable=True)
    pv2_a: Mapped[float | None] = mapped_column(Float, nullable=True)
    pv2_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    grid_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    grid_v: Mapped[float | None] = mapped_column(Float, nullable=True)
    hz: Mapped[float | None] = mapped_column(Float, nullable=True)
    load_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    inverter_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    temp_c: Mapped[float | None] = mapped_column(Float, nullable=True)

    reads: Mapped[int | None] = mapped_column(BigInteger, nullable=True)
    errors: Mapped[int | None] = mapped_column(BigInteger, nullable=True)
    fault: Mapped[bool | None] = mapped_column(Boolean, nullable=True)
    telemetry_valid: Mapped[bool | None] = mapped_column(Boolean, nullable=True)
    shadow: Mapped[bool] = mapped_column(Boolean, default=True)
    aux1: Mapped[str | None] = mapped_column(String(8), nullable=True)
    modbus_write_count: Mapped[int] = mapped_column(Integer, default=0)
    mqtt_connected: Mapped[bool | None] = mapped_column(Boolean, nullable=True)

    quality_json: Mapped[dict[str, Any]] = mapped_column(JSONType, default=dict)
    provenance: Mapped[str] = mapped_column(String(32), default="mqtt")
    source_topic: Mapped[str] = mapped_column(String(255), default="")
    raw_message_id: Mapped[int | None] = mapped_column(BigInteger, nullable=True)


class EnergySample(Base):
    __tablename__ = "energy_samples"
    __table_args__ = (
        Index("ix_energy_device_bucket", "device_id", "bucket_start"),
    )

    bucket_start: Mapped[datetime] = mapped_column(DateTime(timezone=True), primary_key=True)
    device_id: Mapped[str] = mapped_column(String(64), primary_key=True)
    bucket_end: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    pv_wh: Mapped[float | None] = mapped_column(Float, nullable=True)
    load_wh: Mapped[float | None] = mapped_column(Float, nullable=True)
    grid_import_wh: Mapped[float | None] = mapped_column(Float, nullable=True)
    grid_export_wh: Mapped[float | None] = mapped_column(Float, nullable=True)
    battery_charge_wh: Mapped[float | None] = mapped_column(Float, nullable=True)
    battery_discharge_wh: Mapped[float | None] = mapped_column(Float, nullable=True)
    self_consumed_pv_wh: Mapped[float | None] = mapped_column(Float, nullable=True)
    valid_seconds: Mapped[float] = mapped_column(Float, default=0.0)
    gap_seconds: Mapped[float] = mapped_column(Float, default=0.0)
    provenance: Mapped[str] = mapped_column(String(32), default="derived")


class EmsDecision(Base):
    __tablename__ = "ems_decisions"
    __table_args__ = (
        UniqueConstraint("device_id", "boot_id", "seq", "source_timestamp", name="uq_decision_key"),
        Index("ix_decision_device_ts", "device_id", "source_timestamp"),
        Index("ix_decision_strategy", "strategy", "source_timestamp"),
    )

    source_timestamp: Mapped[datetime] = mapped_column(DateTime(timezone=True), primary_key=True)
    device_id: Mapped[str] = mapped_column(String(64), primary_key=True)
    boot_id: Mapped[str] = mapped_column(String(64), primary_key=True)
    seq: Mapped[int] = mapped_column(BigInteger, primary_key=True)
    received_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    uptime_ms: Mapped[int] = mapped_column(BigInteger)
    time_source: Mapped[str] = mapped_column(String(16), default="UNSYNCED")
    time_valid: Mapped[bool] = mapped_column(Boolean, default=False)
    data_source: Mapped[str] = mapped_column(String(24), default="MEASURED")

    strategy: Mapped[str] = mapped_column(String(32))
    configured_strategy: Mapped[str] = mapped_column(String(32))
    previous_strategy: Mapped[str] = mapped_column(String(32))
    state: Mapped[str] = mapped_column(String(24))
    transition: Mapped[str] = mapped_column(String(48))

    soc: Mapped[float | None] = mapped_column(Float, nullable=True)
    grid_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    battery_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    load_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    pv_w: Mapped[float | None] = mapped_column(Float, nullable=True)

    candidate_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    limited_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    simulated_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    safety_min_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    safety_max_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    missing_wh: Mapped[float | None] = mapped_column(Float, nullable=True)
    average_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    unreachable: Mapped[bool] = mapped_column(Boolean, default=False)

    safety_reason: Mapped[str] = mapped_column(String(64), default="NONE")
    reason: Mapped[str] = mapped_column(String(64), default="UNKNOWN")
    executed: Mapped[bool] = mapped_column(Boolean, default=False)
    applied: Mapped[str | None] = mapped_column(String(32), nullable=True)
    aux1: Mapped[str | None] = mapped_column(String(8), nullable=True)
    modbus_write_count: Mapped[int] = mapped_column(Integer, default=0)

    quality_json: Mapped[dict[str, Any]] = mapped_column(JSONType, default=dict)
    ha_inputs_json: Mapped[dict[str, Any] | None] = mapped_column(JSONType, nullable=True)
    raw_payload_json: Mapped[dict[str, Any]] = mapped_column(JSONType, default=dict)


class EmsEvent(Base):
    __tablename__ = "ems_events"
    __table_args__ = (Index("ix_events_device_time", "device_id", "occurred_at"),)

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    occurred_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), index=True)
    received_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    device_id: Mapped[str] = mapped_column(String(64))
    boot_id: Mapped[str | None] = mapped_column(String(64), nullable=True)
    event_type: Mapped[str] = mapped_column(String(48), index=True)
    severity: Mapped[str] = mapped_column(String(16), default="info")
    title: Mapped[str] = mapped_column(String(160))
    detail: Mapped[str | None] = mapped_column(Text, nullable=True)
    metadata_json: Mapped[dict[str, Any]] = mapped_column(JSONType, default=dict)


class EmsStrategyInterval(Base):
    __tablename__ = "ems_strategy_intervals"
    __table_args__ = (Index("ix_intervals_device_start", "device_id", "started_at"),)

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(String(64))
    boot_id: Mapped[str] = mapped_column(String(64))
    strategy: Mapped[str] = mapped_column(String(32))
    configured_strategy: Mapped[str] = mapped_column(String(32))
    state: Mapped[str] = mapped_column(String(24))
    started_at: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    ended_at: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    duration_s: Mapped[int] = mapped_column(Integer, default=0)
    decision_count: Mapped[int] = mapped_column(Integer, default=1)


class ExternalInput(Base):
    __tablename__ = "external_inputs"
    __table_args__ = (Index("ix_ext_input_kind_time", "input_kind", "received_at"),)

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    received_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), primary_key=True, server_default=func.now()
    )
    source_timestamp: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    valid_until: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    device_id: Mapped[str] = mapped_column(String(64), default="afore-ems")
    input_kind: Mapped[str] = mapped_column(String(48))
    status: Mapped[str] = mapped_column(String(24), default="valid")
    age_s: Mapped[int | None] = mapped_column(Integer, nullable=True)
    used: Mapped[bool] = mapped_column(Boolean, default=False)
    quality: Mapped[str | None] = mapped_column(String(32), nullable=True)
    payload_json: Mapped[dict[str, Any]] = mapped_column(JSONType, default=dict)


class SemComparison(Base):
    __tablename__ = "sem_comparisons"
    __table_args__ = (Index("ix_sem_comp_device_ts", "device_id", "compared_at"),)

    compared_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), primary_key=True)
    device_id: Mapped[str] = mapped_column(String(64), primary_key=True)
    sem_decision: Mapped[str | None] = mapped_column(String(64), nullable=True)
    sem_mapped_strategy: Mapped[str | None] = mapped_column(String(32), nullable=True)
    sem_amps: Mapped[float | None] = mapped_column(Float, nullable=True)
    sem_target_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    esp_strategy: Mapped[str | None] = mapped_column(String(32), nullable=True)
    esp_simulated_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    actual_battery_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    actual_grid_w: Mapped[float | None] = mapped_column(Float, nullable=True)
    actual_soc: Mapped[float | None] = mapped_column(Float, nullable=True)
    comparable: Mapped[bool] = mapped_column(Boolean, default=False)
    concordant: Mapped[bool | None] = mapped_column(Boolean, nullable=True)
    direction_match: Mapped[bool | None] = mapped_column(Boolean, nullable=True)
    strategy_match: Mapped[bool | None] = mapped_column(Boolean, nullable=True)
    watt_diff: Mapped[float | None] = mapped_column(Float, nullable=True)
    reason: Mapped[str] = mapped_column(String(128), default="UNKNOWN")


class ConfigurationRevision(Base):
    __tablename__ = "configuration_revisions"

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    recorded_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    device_id: Mapped[str] = mapped_column(String(64))
    source: Mapped[str] = mapped_column(String(32), default="mqtt_config_state")
    revision: Mapped[int | None] = mapped_column(Integer, nullable=True)
    strategy_name: Mapped[str | None] = mapped_column(String(32), nullable=True)
    config_json: Mapped[dict[str, Any]] = mapped_column(JSONType, default=dict)


class SystemHealth(Base):
    __tablename__ = "system_health"
    __table_args__ = (UniqueConstraint("device_id", "component", name="uq_sys_health_dev_comp"),)

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    recorded_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
    device_id: Mapped[str] = mapped_column(String(64))
    component: Mapped[str] = mapped_column(String(32))
    status: Mapped[str] = mapped_column(String(24), default="unknown")
    detail_json: Mapped[dict[str, Any]] = mapped_column(JSONType, default=dict)


class AuxEvent(Base):
    __tablename__ = "aux_events"

    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    occurred_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), index=True)
    device_id: Mapped[str] = mapped_column(String(64))
    origin: Mapped[str | None] = mapped_column(String(32), nullable=True)
    action: Mapped[str | None] = mapped_column(String(32), nullable=True)
    outcome: Mapped[str | None] = mapped_column(String(32), nullable=True)
    aux_state: Mapped[str | None] = mapped_column(String(8), nullable=True)
    auto_off_ms: Mapped[int | None] = mapped_column(Integer, nullable=True)
    physical_feedback: Mapped[str | None] = mapped_column(String(32), nullable=True)
    raw_json: Mapped[dict[str, Any]] = mapped_column(JSONType, default=dict)


class CompanionUser(Base):
    __tablename__ = "companion_users"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    username: Mapped[str] = mapped_column(String(64), unique=True)
    password_hash: Mapped[str] = mapped_column(String(255))
    role: Mapped[str] = mapped_column(String(16), default="viewer")
    enabled: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())


class CompanionSetting(Base):
    __tablename__ = "companion_settings"

    key: Mapped[str] = mapped_column(String(64), primary_key=True)
    value_json: Mapped[dict[str, Any]] = mapped_column(JSONType, default=dict)
    updated_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())
