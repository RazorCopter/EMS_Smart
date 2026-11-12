// SPDX-License-Identifier: MIT
#include "ems/power_balance_evaluator.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>


using namespace ems;

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TEST(cond, msg)                                                 \
  do {                                                                         \
    tests_run++;                                                               \
    if (!(cond)) {                                                             \
      std::printf("FAIL: %s (line %d)\n", msg, __LINE__);                      \
      std::exit(1);                                                            \
    } else {                                                                   \
      tests_passed++;                                                          \
    }                                                                          \
  } while (0)

void test_solarman_discharge_is_positive_charging() {
  PowerBalanceEvaluator eval(BatterySignConvention::DischargeIsPositive,
                             GridSignConvention::ImportIsPositive);

  PowerTelemetry t;
  t.pv_power_w = 3000.0f;
  t.battery_power_w = -2000.0f; // -2000W in Solarman means charging 2000W
  t.grid_power_w = -500.0f;     // -500W means export 500W
  t.load_power_w = 500.0f;

  NormalizedPowerFlow flow = eval.evaluate(t);
  ASSERT_TEST(flow.battery_charge_w == 2000.0f,
              "Solarman -2000W must decode to 2000W charge");
  ASSERT_TEST(flow.battery_discharge_w == 0.0f,
              "Discharge must be 0W while charging");
  ASSERT_TEST(flow.grid_export_w == 500.0f,
              "Grid -500W must decode to 500W export");
  ASSERT_TEST(flow.grid_import_w == 0.0f, "Grid import must be 0W");
  ASSERT_TEST(flow.is_balanced,
              "Power flow must be balanced (3000 in = 2000 + 500 + 500 out)");
}

void test_solarman_discharge_is_positive_discharging() {
  PowerBalanceEvaluator eval(BatterySignConvention::DischargeIsPositive,
                             GridSignConvention::ImportIsPositive);

  PowerTelemetry t;
  t.pv_power_w = 0.0f;
  t.battery_power_w = 800.0f; // +800W in Solarman means discharging 800W
  t.grid_power_w = 0.0f;
  t.load_power_w = 800.0f;

  NormalizedPowerFlow flow = eval.evaluate(t);
  ASSERT_TEST(flow.battery_discharge_w == 800.0f,
              "Solarman +800W must decode to 800W discharge");
  ASSERT_TEST(flow.battery_charge_w == 0.0f,
              "Charge must be 0W while discharging");
  ASSERT_TEST(flow.is_balanced,
              "Power flow must be balanced (800 in = 800 out)");
}

void test_charge_is_positive_convention() {
  PowerBalanceEvaluator eval(BatterySignConvention::ChargeIsPositive,
                             GridSignConvention::ImportIsPositive);

  PowerTelemetry t;
  t.pv_power_w = 2500.0f;
  t.battery_power_w = 1500.0f; // +1500W in SunSpec means charging 1500W
  t.grid_power_w = 0.0f;
  t.load_power_w = 1000.0f;

  NormalizedPowerFlow flow = eval.evaluate(t);
  ASSERT_TEST(flow.battery_charge_w == 1500.0f,
              "Charge convention +1500W must decode to 1500W charge");
  ASSERT_TEST(flow.battery_discharge_w == 0.0f, "Discharge must be 0W");
  ASSERT_TEST(flow.is_balanced,
              "Power flow must be balanced (2500 in = 1500 + 1000 out)");
}

void test_grid_export_and_import() {
  PowerBalanceEvaluator eval(BatterySignConvention::DischargeIsPositive,
                             GridSignConvention::ImportIsPositive);

  // Import scenario
  PowerTelemetry t_import;
  t_import.pv_power_w = 0.0f;
  t_import.battery_power_w = 0.0f;
  t_import.grid_power_w = 1200.0f; // Import 1200W
  t_import.load_power_w = 1200.0f;

  NormalizedPowerFlow f_imp = eval.evaluate(t_import);
  ASSERT_TEST(f_imp.grid_import_w == 1200.0f, "Import power decoded correctly");
  ASSERT_TEST(f_imp.grid_export_w == 0.0f, "Export power is 0");
  ASSERT_TEST(f_imp.is_balanced, "Import balance verified");

  // Export scenario
  PowerTelemetry t_export;
  t_export.pv_power_w = 3500.0f;
  t_export.battery_power_w = 0.0f;
  t_export.grid_power_w = -3000.0f; // Export 3000W
  t_export.load_power_w = 500.0f;

  NormalizedPowerFlow f_exp = eval.evaluate(t_export);
  ASSERT_TEST(f_exp.grid_export_w == 3000.0f, "Export power decoded correctly");
  ASSERT_TEST(f_exp.grid_import_w == 0.0f, "Import power is 0");
  ASSERT_TEST(f_exp.is_balanced, "Export balance verified");
}

void test_unbalanced_detection() {
  PowerBalanceEvaluator eval(BatterySignConvention::DischargeIsPositive,
                             GridSignConvention::ImportIsPositive);

  PowerTelemetry t;
  t.pv_power_w = 5000.0f;
  t.battery_power_w = 0.0f;
  t.grid_power_w = 0.0f;
  t.load_power_w = 100.0f; // Grossly unbalanced (4900W missing sink)

  NormalizedPowerFlow flow = eval.evaluate(t);
  ASSERT_TEST(!flow.is_balanced,
              "Severe power imbalance must be flagged as false");
  ASSERT_TEST(flow.balance_imbalance_w > 4000.0f,
              "Imbalance residual must reflect missing power");
}

void test_infer_battery_convention() {
  PowerBalanceEvaluator eval;
  BatterySignConvention inferred;

  // Night discharge: PV=0, delta_soc=-1.5%, raw battery = +1200
  bool ok1 = eval.inferBatteryConvention(0.0f, 1200.0f, -1.5f, inferred);
  ASSERT_TEST(ok1,
              "Must successfully infer convention under nocturnal discharge");
  ASSERT_TEST(inferred == BatterySignConvention::DischargeIsPositive,
              "Inferred convention must be DischargeIsPositive when +1200 "
              "accompanies delta_soc < 0");

  // Night discharge: PV=0, delta_soc=-1.5%, raw battery = -1200
  bool ok2 = eval.inferBatteryConvention(0.0f, -1200.0f, -1.5f, inferred);
  ASSERT_TEST(ok2, "Must successfully infer convention");
  ASSERT_TEST(inferred == BatterySignConvention::ChargeIsPositive,
              "Inferred convention must be ChargeIsPositive when -1200 "
              "accompanies delta_soc < 0");

  // Noise/ambiguous condition: raw power too small (50W)
  bool ok3 = eval.inferBatteryConvention(0.0f, 50.0f, -0.1f, inferred);
  ASSERT_TEST(!ok3, "Ambiguous near-zero condition must return false");
}

int main() {
  std::printf(
      "Running Power Balance & Sign Convention Test Suite (Milestone M8)...\n");
  test_solarman_discharge_is_positive_charging();
  test_solarman_discharge_is_positive_discharging();
  test_charge_is_positive_convention();
  test_grid_export_and_import();
  test_unbalanced_detection();
  test_infer_battery_convention();

  std::printf("PASS: All %d assertions passed successfully in %d tests.\n",
              tests_passed, tests_run);
  return 0;
}
