// SPDX-License-Identifier: MIT
#include "power_balance_evaluator.h"

namespace ems {

PowerBalanceEvaluator::PowerBalanceEvaluator(BatterySignConvention batt_conv,
                                             GridSignConvention grid_conv,
                                             float max_imbalance_tolerance_pct)
    : m_battery_conv(batt_conv), m_grid_conv(grid_conv),
      m_tolerance_pct(max_imbalance_tolerance_pct) {}

void PowerBalanceEvaluator::setBatterySignConvention(
    BatterySignConvention conv) {
  m_battery_conv = conv;
}

void PowerBalanceEvaluator::setGridSignConvention(GridSignConvention conv) {
  m_grid_conv = conv;
}

void PowerBalanceEvaluator::setTolerancePct(float tolerance_pct) {
  m_tolerance_pct = tolerance_pct;
}

NormalizedPowerFlow
PowerBalanceEvaluator::evaluate(const PowerTelemetry &telemetry) const {
  NormalizedPowerFlow flow;
  flow.pv_power_w = (telemetry.pv_power_w > 0.0f) ? telemetry.pv_power_w : 0.0f;

  // 1. Normalize battery power
  float raw_batt = telemetry.battery_power_w;
  if (m_battery_conv == BatterySignConvention::DischargeIsPositive) {
    if (raw_batt >= 0.0f) {
      flow.battery_discharge_w = raw_batt;
      flow.battery_charge_w = 0.0f;
    } else {
      flow.battery_discharge_w = 0.0f;
      flow.battery_charge_w = -raw_batt;
    }
  } else { // ChargeIsPositive
    if (raw_batt >= 0.0f) {
      flow.battery_charge_w = raw_batt;
      flow.battery_discharge_w = 0.0f;
    } else {
      flow.battery_charge_w = 0.0f;
      flow.battery_discharge_w = -raw_batt;
    }
  }

  // 2. Normalize grid power
  float raw_grid = telemetry.grid_power_w;
  if (m_grid_conv == GridSignConvention::ImportIsPositive) {
    if (raw_grid >= 0.0f) {
      flow.grid_import_w = raw_grid;
      flow.grid_export_w = 0.0f;
    } else {
      flow.grid_import_w = 0.0f;
      flow.grid_export_w = -raw_grid;
    }
  } else { // ExportIsPositive
    if (raw_grid >= 0.0f) {
      flow.grid_export_w = raw_grid;
      flow.grid_import_w = 0.0f;
    } else {
      flow.grid_export_w = 0.0f;
      flow.grid_import_w = -raw_grid;
    }
  }

  // 3. Load consumption
  // If load is explicitly measured and > 0, use it;
  // otherwise compute deduced load = PV + GridImport + BattDischarge -
  // GridExport - BattCharge
  if (telemetry.load_power_w > 0.0f) {
    flow.load_power_w = telemetry.load_power_w;
  } else {
    float deduced = flow.pv_power_w + flow.grid_import_w +
                    flow.battery_discharge_w - flow.grid_export_w -
                    flow.battery_charge_w;
    flow.load_power_w = (deduced > 0.0f) ? deduced : 0.0f;
  }

  // 4. Energy Conservation Check (Kirchhoff's power conservation):
  // Total Sources = PV + GridImport + BatteryDischarge
  // Total Sinks   = Load + GridExport + BatteryCharge
  float total_sources =
      flow.pv_power_w + flow.grid_import_w + flow.battery_discharge_w;
  float total_sinks =
      flow.load_power_w + flow.grid_export_w + flow.battery_charge_w;

  float imbalance = total_sources - total_sinks;
  flow.balance_imbalance_w = imbalance;

  float max_reference =
      (total_sources > total_sinks) ? total_sources : total_sinks;
  float allowed_delta = 50.0f; // Absolute baseline noise (50W)
  if (max_reference > 100.0f) {
    allowed_delta += max_reference * m_tolerance_pct;
  }

  float abs_imbalance = (imbalance < 0.0f) ? -imbalance : imbalance;
  flow.is_balanced = (abs_imbalance <= allowed_delta);

  return flow;
}

bool PowerBalanceEvaluator::inferBatteryConvention(
    float pv_power_w, float battery_power_raw, float delta_soc,
    BatterySignConvention &inferred_out) const {
  // Need significant power magnitude to avoid noise around 0W
  float abs_raw =
      (battery_power_raw < 0.0f) ? -battery_power_raw : battery_power_raw;
  if (abs_raw < 100.0f) {
    return false;
  }

  // Clear discharge condition: night (PV == 0) and SOC decreased significantly
  if (pv_power_w < 20.0f && delta_soc < -0.5f) {
    // Battery must be discharging
    if (battery_power_raw > 0.0f) {
      inferred_out = BatterySignConvention::DischargeIsPositive;
      return true;
    } else {
      inferred_out = BatterySignConvention::ChargeIsPositive;
      return true;
    }
  }

  // Clear charge condition: strong solar and SOC increased significantly
  if (pv_power_w > 1000.0f && delta_soc > 0.5f) {
    // Battery must be charging
    if (battery_power_raw < 0.0f) {
      inferred_out =
          BatterySignConvention::DischargeIsPositive; // Negative means charging
      return true;
    } else {
      inferred_out =
          BatterySignConvention::ChargeIsPositive; // Positive means charging
      return true;
    }
  }

  return false;
}

} // namespace ems
