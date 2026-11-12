// SPDX-License-Identifier: MIT
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ems {

enum class BatterySignConvention {
  DischargeIsPositive, // Standard: > 0 discharge, < 0 charge
  ChargeIsPositive     // Standard SunSpec/Alt:     > 0 charge,    < 0 discharge
};

enum class GridSignConvention {
  ImportIsPositive, // Standard Meter: > 0 import from grid, < 0 export to grid
  ExportIsPositive // Inverter-centric: > 0 export to grid, < 0 import from grid
};

struct PowerTelemetry {
  float pv_power_w = 0.0f;      // Always >= 0
  float battery_power_w = 0.0f; // Raw signed power as reported by register
  float grid_power_w = 0.0f; // Raw signed power as reported by meter/inverter
  float load_power_w =
      0.0f;                 // House consumption (if measured or computed, >= 0)
  float battery_soc = 0.0f; // 0..100%
};

struct NormalizedPowerFlow {
  float pv_power_w = 0.0f;          // >= 0
  float battery_charge_w = 0.0f;    // Power flowing into battery (>= 0)
  float battery_discharge_w = 0.0f; // Power flowing out of battery (>= 0)
  float grid_import_w = 0.0f;       // Power flowing from grid (>= 0)
  float grid_export_w = 0.0f;       // Power flowing to grid (>= 0)
  float load_power_w = 0.0f;        // Total consumption (>= 0)
  float balance_imbalance_w =
      0.0f;                 // Residual: generation + imports - consumptions
  bool is_balanced = false; // Within acceptable tolerance
};

class PowerBalanceEvaluator {
public:
  explicit PowerBalanceEvaluator(
      BatterySignConvention batt_conv =
          BatterySignConvention::DischargeIsPositive,
      GridSignConvention grid_conv = GridSignConvention::ImportIsPositive,
      float max_imbalance_tolerance_pct =
          0.12f // 12% tolerance for conversion losses
  );

  void setBatterySignConvention(BatterySignConvention conv);
  void setGridSignConvention(GridSignConvention conv);
  void setTolerancePct(float tolerance_pct);

  BatterySignConvention batterySignConvention() const { return m_battery_conv; }
  GridSignConvention gridSignConvention() const { return m_grid_conv; }

  // Normalize raw powers according to current sign conventions
  NormalizedPowerFlow evaluate(const PowerTelemetry &telemetry) const;

  // Auto-detect sign convention based on physics when conditions allow:
  // returns true if convention could be conclusively determined.
  bool inferBatteryConvention(float pv_power_w, float battery_power_raw,
                              float delta_soc,
                              BatterySignConvention &inferred_out) const;

private:
  BatterySignConvention m_battery_conv;
  GridSignConvention m_grid_conv;
  float m_tolerance_pct;
};

} // namespace ems
