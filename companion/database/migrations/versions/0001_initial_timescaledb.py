"""Initial Afore EMS Companion schema with TimescaleDB hypertables and rollups."""
from __future__ import annotations

import os
from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects import postgresql

revision = "0001_initial_timescaledb"
down_revision = None
branch_labels = None
depends_on = None


def upgrade() -> None:
    op.execute("CREATE EXTENSION IF NOT EXISTS timescaledb;")

    op.create_table(
        "devices",
        sa.Column("device_id", sa.String(64), primary_key=True),
        sa.Column("name", sa.String(128), nullable=False, server_default="Afore Autonomous EMS"),
        sa.Column("manufacturer", sa.String(64), nullable=False, server_default="Afore"),
        sa.Column("model", sa.String(64), nullable=True),
        sa.Column("firmware_version", sa.String(64), nullable=True),
        sa.Column("operating_mode", sa.String(32), nullable=False, server_default="SHADOW"),
        sa.Column("current_boot_id", sa.String(64), nullable=True),
        sa.Column("last_seq", sa.BigInteger(), nullable=True),
        sa.Column("last_uptime_ms", sa.BigInteger(), nullable=True),
        sa.Column("last_seen_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("metadata_json", postgresql.JSONB(), nullable=False, server_default=sa.text("'{}'::jsonb")),
    )

    op.create_table(
        "mqtt_messages",
        sa.Column("id", sa.BigInteger(), autoincrement=True, nullable=False),
        sa.Column("received_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.func.now()),
        sa.Column("topic", sa.String(255), nullable=False),
        sa.Column("qos", sa.Integer(), nullable=False, server_default="0"),
        sa.Column("retain", sa.Boolean(), nullable=False, server_default=sa.text("false")),
        sa.Column("payload_bytes", sa.Integer(), nullable=False, server_default="0"),
        sa.Column("payload_json", postgresql.JSONB(), nullable=True),
        sa.Column("raw_text", sa.Text(), nullable=True),
        sa.Column("schema_valid", sa.Boolean(), nullable=False, server_default=sa.text("true")),
        sa.Column("validation_error", sa.String(255), nullable=True),
        sa.PrimaryKeyConstraint("id", "received_at"),
    )
    op.create_index("ix_mqtt_messages_topic_time", "mqtt_messages", ["topic", "received_at"])

    op.create_table(
        "telemetry_samples",
        sa.Column("source_timestamp", sa.DateTime(timezone=True), nullable=False),
        sa.Column("device_id", sa.String(64), nullable=False),
        sa.Column("uptime_ms", sa.BigInteger(), nullable=False, server_default="0"),
        sa.Column("received_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.func.now()),
        sa.Column("boot_id", sa.String(64), nullable=False, server_default="unknown"),
        sa.Column("time_source", sa.String(16), nullable=False, server_default="UNSYNCED"),
        sa.Column("time_valid", sa.Boolean(), nullable=False, server_default=sa.text("false")),
        sa.Column("soc", sa.Float(), nullable=True),
        sa.Column("battery_w", sa.Float(), nullable=True),
        sa.Column("battery_v", sa.Float(), nullable=True),
        sa.Column("battery_a", sa.Float(), nullable=True),
        sa.Column("pv_w", sa.Float(), nullable=True),
        sa.Column("pv1_v", sa.Float(), nullable=True),
        sa.Column("pv1_a", sa.Float(), nullable=True),
        sa.Column("pv1_w", sa.Float(), nullable=True),
        sa.Column("pv2_v", sa.Float(), nullable=True),
        sa.Column("pv2_a", sa.Float(), nullable=True),
        sa.Column("pv2_w", sa.Float(), nullable=True),
        sa.Column("grid_w", sa.Float(), nullable=True),
        sa.Column("grid_v", sa.Float(), nullable=True),
        sa.Column("hz", sa.Float(), nullable=True),
        sa.Column("load_w", sa.Float(), nullable=True),
        sa.Column("inverter_w", sa.Float(), nullable=True),
        sa.Column("temp_c", sa.Float(), nullable=True),
        sa.Column("reads", sa.BigInteger(), nullable=True),
        sa.Column("errors", sa.BigInteger(), nullable=True),
        sa.Column("fault", sa.Boolean(), nullable=True),
        sa.Column("telemetry_valid", sa.Boolean(), nullable=True),
        sa.Column("shadow", sa.Boolean(), nullable=False, server_default=sa.text("true")),
        sa.Column("aux1", sa.String(8), nullable=True),
        sa.Column("modbus_write_count", sa.Integer(), nullable=False, server_default="0"),
        sa.Column("mqtt_connected", sa.Boolean(), nullable=True),
        sa.Column("quality_json", postgresql.JSONB(), nullable=False, server_default=sa.text("'{}'::jsonb")),
        sa.Column("provenance", sa.String(32), nullable=False, server_default="mqtt"),
        sa.Column("source_topic", sa.String(255), nullable=False, server_default=""),
        sa.Column("raw_message_id", sa.BigInteger(), nullable=True),
        sa.PrimaryKeyConstraint("source_timestamp", "device_id", "uptime_ms"),
    )
    op.create_index("ix_telemetry_device_ts", "telemetry_samples", ["device_id", "source_timestamp"])

    op.create_table(
        "energy_samples",
        sa.Column("bucket_start", sa.DateTime(timezone=True), nullable=False),
        sa.Column("device_id", sa.String(64), nullable=False),
        sa.Column("bucket_end", sa.DateTime(timezone=True), nullable=False),
        sa.Column("pv_wh", sa.Float(), nullable=True),
        sa.Column("load_wh", sa.Float(), nullable=True),
        sa.Column("grid_import_wh", sa.Float(), nullable=True),
        sa.Column("grid_export_wh", sa.Float(), nullable=True),
        sa.Column("battery_charge_wh", sa.Float(), nullable=True),
        sa.Column("battery_discharge_wh", sa.Float(), nullable=True),
        sa.Column("self_consumed_pv_wh", sa.Float(), nullable=True),
        sa.Column("valid_seconds", sa.Float(), nullable=False, server_default="0"),
        sa.Column("gap_seconds", sa.Float(), nullable=False, server_default="0"),
        sa.Column("provenance", sa.String(32), nullable=False, server_default="derived"),
        sa.PrimaryKeyConstraint("bucket_start", "device_id"),
    )

    op.create_table(
        "ems_decisions",
        sa.Column("source_timestamp", sa.DateTime(timezone=True), nullable=False),
        sa.Column("device_id", sa.String(64), nullable=False),
        sa.Column("boot_id", sa.String(64), nullable=False),
        sa.Column("seq", sa.BigInteger(), nullable=False),
        sa.Column("received_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.func.now()),
        sa.Column("uptime_ms", sa.BigInteger(), nullable=False),
        sa.Column("time_source", sa.String(16), nullable=False, server_default="UNSYNCED"),
        sa.Column("time_valid", sa.Boolean(), nullable=False, server_default=sa.text("false")),
        sa.Column("data_source", sa.String(24), nullable=False, server_default="MEASURED"),
        sa.Column("strategy", sa.String(32), nullable=False),
        sa.Column("configured_strategy", sa.String(32), nullable=False),
        sa.Column("previous_strategy", sa.String(32), nullable=False),
        sa.Column("state", sa.String(24), nullable=False),
        sa.Column("transition", sa.String(48), nullable=False),
        sa.Column("soc", sa.Float(), nullable=True),
        sa.Column("grid_w", sa.Float(), nullable=True),
        sa.Column("battery_w", sa.Float(), nullable=True),
        sa.Column("load_w", sa.Float(), nullable=True),
        sa.Column("pv_w", sa.Float(), nullable=True),
        sa.Column("candidate_w", sa.Float(), nullable=True),
        sa.Column("limited_w", sa.Float(), nullable=True),
        sa.Column("simulated_w", sa.Float(), nullable=True),
        sa.Column("safety_min_w", sa.Float(), nullable=True),
        sa.Column("safety_max_w", sa.Float(), nullable=True),
        sa.Column("missing_wh", sa.Float(), nullable=True),
        sa.Column("average_w", sa.Float(), nullable=True),
        sa.Column("unreachable", sa.Boolean(), nullable=False, server_default=sa.text("false")),
        sa.Column("safety_reason", sa.String(64), nullable=False, server_default="NONE"),
        sa.Column("reason", sa.String(64), nullable=False, server_default="UNKNOWN"),
        sa.Column("executed", sa.Boolean(), nullable=False, server_default=sa.text("false")),
        sa.Column("applied", sa.String(32), nullable=True),
        sa.Column("aux1", sa.String(8), nullable=True),
        sa.Column("modbus_write_count", sa.Integer(), nullable=False, server_default="0"),
        sa.Column("quality_json", postgresql.JSONB(), nullable=False, server_default=sa.text("'{}'::jsonb")),
        sa.Column("ha_inputs_json", postgresql.JSONB(), nullable=True),
        sa.Column("raw_payload_json", postgresql.JSONB(), nullable=False, server_default=sa.text("'{}'::jsonb")),
        sa.PrimaryKeyConstraint("source_timestamp", "device_id", "boot_id", "seq"),
    )
    op.create_index("ix_decision_device_ts", "ems_decisions", ["device_id", "source_timestamp"])
    op.create_index("ix_decision_strategy", "ems_decisions", ["strategy", "source_timestamp"])

    op.create_table(
        "ems_events",
        sa.Column("id", sa.BigInteger(), primary_key=True, autoincrement=True),
        sa.Column("occurred_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("received_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.func.now()),
        sa.Column("device_id", sa.String(64), nullable=False),
        sa.Column("boot_id", sa.String(64), nullable=True),
        sa.Column("event_type", sa.String(48), nullable=False),
        sa.Column("severity", sa.String(16), nullable=False, server_default="info"),
        sa.Column("title", sa.String(160), nullable=False),
        sa.Column("detail", sa.Text(), nullable=True),
        sa.Column("metadata_json", postgresql.JSONB(), nullable=False, server_default=sa.text("'{}'::jsonb")),
    )
    op.create_index("ix_events_device_time", "ems_events", ["device_id", "occurred_at"])
    op.create_index("ix_events_type", "ems_events", ["event_type"])

    op.create_table(
        "ems_strategy_intervals",
        sa.Column("id", sa.BigInteger(), primary_key=True, autoincrement=True),
        sa.Column("device_id", sa.String(64), nullable=False),
        sa.Column("boot_id", sa.String(64), nullable=False),
        sa.Column("strategy", sa.String(32), nullable=False),
        sa.Column("configured_strategy", sa.String(32), nullable=False),
        sa.Column("state", sa.String(24), nullable=False),
        sa.Column("started_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("ended_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("duration_s", sa.Integer(), nullable=False, server_default="0"),
        sa.Column("decision_count", sa.Integer(), nullable=False, server_default="1"),
    )
    op.create_index("ix_intervals_device_start", "ems_strategy_intervals", ["device_id", "started_at"])

    op.create_table(
        "external_inputs",
        sa.Column("id", sa.BigInteger(), autoincrement=True, nullable=False),
        sa.Column("received_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.func.now()),
        sa.Column("source_timestamp", sa.DateTime(timezone=True), nullable=True),
        sa.Column("valid_until", sa.DateTime(timezone=True), nullable=True),
        sa.Column("device_id", sa.String(64), nullable=False, server_default="afore-ems"),
        sa.Column("input_kind", sa.String(48), nullable=False),
        sa.Column("status", sa.String(24), nullable=False, server_default="valid"),
        sa.Column("age_s", sa.Integer(), nullable=True),
        sa.Column("used", sa.Boolean(), nullable=False, server_default=sa.text("false")),
        sa.Column("quality", sa.String(32), nullable=True),
        sa.Column("payload_json", postgresql.JSONB(), nullable=False, server_default=sa.text("'{}'::jsonb")),
        sa.PrimaryKeyConstraint("id", "received_at"),
    )
    op.create_index("ix_ext_input_kind_time", "external_inputs", ["input_kind", "received_at"])

    op.create_table(
        "sem_comparisons",
        sa.Column("compared_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("device_id", sa.String(64), nullable=False),
        sa.Column("sem_decision", sa.String(64), nullable=True),
        sa.Column("sem_mapped_strategy", sa.String(32), nullable=True),
        sa.Column("sem_amps", sa.Float(), nullable=True),
        sa.Column("sem_target_w", sa.Float(), nullable=True),
        sa.Column("esp_strategy", sa.String(32), nullable=True),
        sa.Column("esp_simulated_w", sa.Float(), nullable=True),
        sa.Column("actual_battery_w", sa.Float(), nullable=True),
        sa.Column("actual_grid_w", sa.Float(), nullable=True),
        sa.Column("actual_soc", sa.Float(), nullable=True),
        sa.Column("comparable", sa.Boolean(), nullable=False, server_default=sa.text("false")),
        sa.Column("concordant", sa.Boolean(), nullable=True),
        sa.Column("direction_match", sa.Boolean(), nullable=True),
        sa.Column("strategy_match", sa.Boolean(), nullable=True),
        sa.Column("watt_diff", sa.Float(), nullable=True),
        sa.Column("reason", sa.String(128), nullable=False, server_default="UNKNOWN"),
        sa.PrimaryKeyConstraint("compared_at", "device_id"),
    )

    op.create_table(
        "configuration_revisions",
        sa.Column("id", sa.BigInteger(), primary_key=True, autoincrement=True),
        sa.Column("recorded_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.func.now()),
        sa.Column("device_id", sa.String(64), nullable=False),
        sa.Column("source", sa.String(32), nullable=False, server_default="mqtt_config_state"),
        sa.Column("revision", sa.Integer(), nullable=True),
        sa.Column("strategy_name", sa.String(32), nullable=True),
        sa.Column("config_json", postgresql.JSONB(), nullable=False, server_default=sa.text("'{}'::jsonb")),
    )

    op.create_table(
        "system_health",
        sa.Column("id", sa.BigInteger(), primary_key=True, autoincrement=True),
        sa.Column("recorded_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.func.now()),
        sa.Column("device_id", sa.String(64), nullable=False),
        sa.Column("component", sa.String(32), nullable=False),
        sa.Column("status", sa.String(24), nullable=False, server_default="unknown"),
        sa.Column("detail_json", postgresql.JSONB(), nullable=False, server_default=sa.text("'{}'::jsonb")),
        sa.UniqueConstraint("device_id", "component", name="uq_sys_health_dev_comp"),
    )

    op.create_table(
        "aux_events",
        sa.Column("id", sa.BigInteger(), primary_key=True, autoincrement=True),
        sa.Column("occurred_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("device_id", sa.String(64), nullable=False),
        sa.Column("origin", sa.String(32), nullable=True),
        sa.Column("action", sa.String(32), nullable=True),
        sa.Column("outcome", sa.String(32), nullable=True),
        sa.Column("aux_state", sa.String(8), nullable=True),
        sa.Column("auto_off_ms", sa.Integer(), nullable=True),
        sa.Column("physical_feedback", sa.String(32), nullable=True),
        sa.Column("raw_json", postgresql.JSONB(), nullable=False, server_default=sa.text("'{}'::jsonb")),
    )
    op.create_index("ix_aux_events_time", "aux_events", ["occurred_at"])

    op.create_table(
        "companion_users",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("username", sa.String(64), nullable=False, unique=True),
        sa.Column("password_hash", sa.String(255), nullable=False),
        sa.Column("role", sa.String(16), nullable=False, server_default="viewer"),
        sa.Column("enabled", sa.Boolean(), nullable=False, server_default=sa.text("true")),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.func.now()),
    )

    op.create_table(
        "companion_settings",
        sa.Column("key", sa.String(64), primary_key=True),
        sa.Column("value_json", postgresql.JSONB(), nullable=False, server_default=sa.text("'{}'::jsonb")),
        sa.Column("updated_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.func.now()),
    )

    # Convert time-series tables to TimescaleDB hypertables
    op.execute("SELECT create_hypertable('mqtt_messages', 'received_at', if_not_exists => TRUE);")
    op.execute("SELECT create_hypertable('telemetry_samples', 'source_timestamp', if_not_exists => TRUE);")
    op.execute("SELECT create_hypertable('energy_samples', 'bucket_start', if_not_exists => TRUE);")
    op.execute("SELECT create_hypertable('ems_decisions', 'source_timestamp', if_not_exists => TRUE);")
    op.execute("SELECT create_hypertable('external_inputs', 'received_at', if_not_exists => TRUE);")
    op.execute("SELECT create_hypertable('sem_comparisons', 'compared_at', if_not_exists => TRUE);")

    raw_mqtt_days = int(os.getenv("RETENTION_RAW_MQTT_DAYS", "30"))
    telemetry_days = int(os.getenv("RETENTION_TELEMETRY_DAYS", "90"))
    agg_1m_days = int(os.getenv("RETENTION_AGG_1M_DAYS", "730"))

    with op.get_context().autocommit_block():
        # 1-minute continuous aggregate (preserves NULL when all inputs are NULL)
        op.execute(
            """
            CREATE MATERIALIZED VIEW IF NOT EXISTS telemetry_1m
            WITH (timescaledb.continuous) AS
            SELECT
                time_bucket('1 minute', source_timestamp) AS bucket,
                device_id,
                AVG(soc) AS soc_avg,
                MIN(soc) AS soc_min,
                MAX(soc) AS soc_max,
                AVG(pv_w) AS pv_w_avg,
                AVG(load_w) AS load_w_avg,
                AVG(battery_w) AS battery_w_avg,
                AVG(grid_w) AS grid_w_avg,
                AVG(battery_v) AS battery_v_avg,
                AVG(battery_a) AS battery_a_avg,
                AVG(temp_c) AS temp_c_avg,
                COUNT(*) AS sample_count,
                COUNT(pv_w) AS valid_pv_count
            FROM telemetry_samples
            GROUP BY bucket, device_id
            WITH NO DATA;
            """
        )
        op.execute(
            """
            SELECT add_continuous_aggregate_policy('telemetry_1m',
                start_offset => INTERVAL '2 days',
                end_offset => INTERVAL '1 minute',
                schedule_interval => INTERVAL '1 minute',
                if_not_exists => TRUE);
            """
        )

        # 1-hour continuous aggregate (long-term history without expiry)
        op.execute(
            """
            CREATE MATERIALIZED VIEW IF NOT EXISTS telemetry_1h
            WITH (timescaledb.continuous) AS
            SELECT
                time_bucket('1 hour', source_timestamp) AS bucket,
                device_id,
                AVG(soc) AS soc_avg,
                MIN(soc) AS soc_min,
                MAX(soc) AS soc_max,
                AVG(pv_w) AS pv_w_avg,
                AVG(load_w) AS load_w_avg,
                AVG(battery_w) AS battery_w_avg,
                AVG(grid_w) AS grid_w_avg,
                COUNT(*) AS sample_count
            FROM telemetry_samples
            GROUP BY bucket, device_id
            WITH NO DATA;
            """
        )
        op.execute(
            """
            SELECT add_continuous_aggregate_policy('telemetry_1h',
                start_offset => INTERVAL '7 days',
                end_offset => INTERVAL '1 hour',
                schedule_interval => INTERVAL '15 minutes',
                if_not_exists => TRUE);
            """
        )

        # Compression & retention policies
        op.execute("ALTER TABLE mqtt_messages SET (timescaledb.compress, timescaledb.compress_segmentby = 'topic');")
        op.execute("SELECT add_compression_policy('mqtt_messages', INTERVAL '3 days', if_not_exists => TRUE);")
        op.execute(f"SELECT add_retention_policy('mqtt_messages', INTERVAL '{raw_mqtt_days} days', if_not_exists => TRUE);")

        op.execute("ALTER TABLE telemetry_samples SET (timescaledb.compress, timescaledb.compress_segmentby = 'device_id');")
        op.execute("SELECT add_compression_policy('telemetry_samples', INTERVAL '7 days', if_not_exists => TRUE);")
        op.execute(f"SELECT add_retention_policy('telemetry_samples', INTERVAL '{telemetry_days} days', if_not_exists => TRUE);")

        op.execute("ALTER TABLE ems_decisions SET (timescaledb.compress, timescaledb.compress_segmentby = 'device_id');")
        op.execute("SELECT add_compression_policy('ems_decisions', INTERVAL '14 days', if_not_exists => TRUE);")

        op.execute(f"SELECT add_retention_policy('telemetry_1m', INTERVAL '{agg_1m_days} days', if_not_exists => TRUE);")
        op.execute("SELECT add_retention_policy('external_inputs', INTERVAL '90 days', if_not_exists => TRUE);")


def downgrade() -> None:
    with op.get_context().autocommit_block():
        op.execute("DROP MATERIALIZED VIEW IF EXISTS telemetry_1h;")
        op.execute("DROP MATERIALIZED VIEW IF EXISTS telemetry_1m;")
    for table in [
        "companion_settings",
        "companion_users",
        "aux_events",
        "system_health",
        "configuration_revisions",
        "sem_comparisons",
        "external_inputs",
        "ems_strategy_intervals",
        "ems_events",
        "ems_decisions",
        "energy_samples",
        "telemetry_samples",
        "mqtt_messages",
        "devices",
    ]:
        op.drop_table(table)
