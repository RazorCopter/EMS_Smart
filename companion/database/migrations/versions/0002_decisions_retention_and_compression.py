"""Add retention policy on ems_decisions and compression on remaining hypertables."""
from __future__ import annotations

import os
from alembic import op

revision = "0002_decisions_retention_and_compression"
down_revision = "0001_initial_timescaledb"
branch_labels = None
depends_on = None


def upgrade() -> None:
    decisions_days = int(os.getenv("RETENTION_DECISIONS_DAYS", "180"))
    comparisons_days = int(os.getenv("RETENTION_COMPARISONS_DAYS", "365"))

    with op.get_context().autocommit_block():
        op.execute(
            f"SELECT add_retention_policy('ems_decisions', INTERVAL '{decisions_days} days', if_not_exists => TRUE);"
        )

        op.execute(
            "ALTER TABLE external_inputs SET (timescaledb.compress, timescaledb.compress_segmentby = 'device_id,input_kind');"
        )
        op.execute("SELECT add_compression_policy('external_inputs', INTERVAL '7 days', if_not_exists => TRUE);")

        op.execute(
            "ALTER TABLE energy_samples SET (timescaledb.compress, timescaledb.compress_segmentby = 'device_id');"
        )
        op.execute("SELECT add_compression_policy('energy_samples', INTERVAL '30 days', if_not_exists => TRUE);")

        op.execute(
            "ALTER TABLE sem_comparisons SET (timescaledb.compress, timescaledb.compress_segmentby = 'device_id');"
        )
        op.execute("SELECT add_compression_policy('sem_comparisons', INTERVAL '14 days', if_not_exists => TRUE);")
        op.execute(
            f"SELECT add_retention_policy('sem_comparisons', INTERVAL '{comparisons_days} days', if_not_exists => TRUE);"
        )


def downgrade() -> None:
    with op.get_context().autocommit_block():
        op.execute("SELECT remove_retention_policy('sem_comparisons', if_exists => TRUE);")
        op.execute("SELECT remove_compression_policy('sem_comparisons', if_exists => TRUE);")
        op.execute("SELECT remove_compression_policy('energy_samples', if_exists => TRUE);")
        op.execute("SELECT remove_compression_policy('external_inputs', if_exists => TRUE);")
        op.execute("SELECT remove_retention_policy('ems_decisions', if_exists => TRUE);")
