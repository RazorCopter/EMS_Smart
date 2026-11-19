# Afore Autonomous EMS — Functional Specification & Software Contracts (Milestone M1)

Date: 2026-11-13  
Standard: C++17 Core Library  
Applicable Inverter: Afore AF5K-SL Hybrid Inverter with Battery Storage  
Reference Blueprint: OpenEMS Modular Controller Paradigm

---

## 1. Domain Entities & Core Types

The common C++17 EMS library operates strictly on value types with deterministic copy semantics, zero dynamic allocation on the hot 1 Hz loop, and complete serialization support.

### 1.1 `TelemetrySnapshot`
Represents verified, calibrated instantaneous readings from physical instrumentation:
```cpp
struct TelemetrySnapshot {
    uint64_t timestamp_ms{0};           // Monotonic time or POSIX timestamp (ms)
    int32_t  grid_power_w{0};           // > 0 = import from grid, < 0 = export to grid
    int32_t  battery_power_w{0};        // > 0 = charging into battery, < 0 = discharging
    int32_t  pv_power_w{0};             // >= 0 solar PV generation
    int32_t  load_power_w{0};           // >= 0 domestic house consumption
    float    battery_soc_pct{0.0f};     // 0.0 to 100.0%
    float    battery_voltage_v{0.0f};   // Battery terminal voltage
    float    battery_current_a{0.0f};   // Battery current
    float    battery_temp_c{0.0f};      // Battery cell/pack temperature
    float    inverter_temp_c{0.0f};     // Inverter heatsink temperature
    uint16_t inverter_status_raw{0};    // Raw inverter operating status code
    bool     grid_meter_valid{false};   // True if grid meter frame is fresh (< 5s)
    bool     battery_bms_valid{false};  // True if battery telemetry is valid
};
```

### 1.2 `HardwareCapabilities`
Dynamic limits published by the inverter and battery BMS:
```cpp
struct HardwareCapabilities {
    int32_t max_charge_power_w{3000};      // Maximum continuous charge power limit
    int32_t max_discharge_power_w{3000};   // Maximum continuous discharge power limit
    int32_t inverter_max_apparent_va{5000};// Maximum inverter AC rating
    float   soc_min_reserve_pct{15.0f};    // Hard safety reserve floor
    float   soc_critical_pct{10.0f};       // Emergency grid recovery threshold
};
```

### 1.3 `EmsConfig`
User configuration parameters, versioned and persistable:
```cpp
enum class EmsStrategyMode : uint8_t {
    SELF_USE = 0,               // Standard closed-loop balancing
    CHARGE_FIRST = 1,           // Prioritize battery charging before household load
    SOLAR_OPTIMIZED = 2,        // Defer morning charge to avoid peak solar curtailment
    TOU_TARIFF = 3,             // Dynamic tariff arbitrage
    SCHEDULED_MAINTENANCE = 4   // BMS cell balancing full charge cycle
};

struct EmsConfig {
    uint32_t        config_revision{1};
    EmsStrategyMode strategy{EmsStrategyMode::SELF_USE};
    float           soc_reserve_floor_pct{15.0f};
    int32_t         max_charge_rate_w{3000};
    int32_t         max_discharge_rate_w{3000};
    int32_t         ramp_rate_w_per_s{250};
    int32_t         deadband_w{20};
    bool            allow_grid_charging{false};
    bool            allow_grid_arbitrage_export{false};
};
```

### 1.4 `EmsDecision`
Immutable explainable result generated every control cycle:
```cpp
enum class DecisionAuthority : uint8_t {
    LOCAL_AUTONOMOUS = 0,
    SCHEDULED_MAINTENANCE = 1,
    MQTT_REMOTE_OVERRIDE = 2,
    SAFETY_EMERGENCY = 3
};

struct EmsDecision {
    uint64_t          decision_id{0};
    uint64_t          timestamp_ms{0};
    EmsStrategyMode   selected_strategy{EmsStrategyMode::SELF_USE};
    DecisionAuthority authority{DecisionAuthority::LOCAL_AUTONOMOUS};
    int32_t           proposed_power_w{0};      // Raw intent prior to constraints
    int32_t           constrained_power_w{0};   // Setpoint after hard constraint pipeline
    int32_t           actuated_power_w{0};      // Actually dispatched (0 in shadow mode)
    uint32_t          active_constraints_mask{0}; // Bitfield of applied constraints
    char              justification[96]{""};    // Human-readable primary reason
};
```

---

## 2. Controller State Machines

### 2.1 Maintenance Cycle (`FixStateOfCharge`)
```
  [IDLE] 
     │ (Trigger: User manual start OR weekly schedule)
     ▼
  [CHARGING_TO_TARGET] ──(Timeout > 6h OR User Cancel)──> [ABORTED]
     │ (Condition: Battery SOC == 100% and Current < 2A)
     ▼
  [CELL_BALANCING_HOLD] ──(Hold duration >= 60 min)────> [COMPLETED]
     │                                                         │
     └───────────────────────> [RETURN_TO_SELF_USE] <──────────┘
```

### 2.2 Controller Actuator Machine (`M13/M14`)
```
  [DISARMED (Shadow Mode)]
     │ (Explicit write authorization + Pre-checks passed)
     ▼
  [PRECHECK] ──(Validation failed)──> [DISARMED]
     │
     ▼
  [ENTERING_EXTERNAL] (Write 0x0A08 = 0x0001)
     │ (Readback verified)
     ▼
  [ACTIVE] <───(Renew setpoint 0x0A09-0x0A0A every 30-60s)
     │
     ├──(Heartbeat timeout / Comm error / Stop commanded)──> [EXITING_EXTERNAL]
     │                                                            │
     ▼                                                            ▼
  [FAULT / DEGRADED] ───────────────────────────────────────> [DISARMED]
```

---

## 3. Communication & Integration Interfaces

1. **REST API**:
   - `GET /api/v1/ems/state`: Real-time state, decision record, active constraints, observed telemetry.
   - `GET /api/v1/ems/config`: Effective configuration and version stamp.
   - `PATCH /api/v1/ems/config`: Update configuration with optimistic concurrency control (`If-Match: revision`).
   - `POST /api/v1/ems/operations`: Launch lease-bounded maintenance or temporary override.
2. **MQTT Integration**:
   - Availability topic with LWT: `afore/status` (`online` / `offline`).
   - State topic (JSON): `afore/ems/state`.
   - Command topic: `afore/set/operation` (JSON payload with lease duration).
