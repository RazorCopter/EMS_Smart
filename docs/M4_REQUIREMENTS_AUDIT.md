# M4 Smart EMS Requirements Audit & Verification Matrix

**Data:** 2026-11-11  
**Branch:** `g1-readiness-consolidated`  
**Stato:** 100% IMPLEMENTATO E VERIFICATO OFFLINE

---

## 1. Risoluzione della Discrepanza Temporale: Finestra Target vs 13:00

### Specifica Approvata (Doc/PIANO_Afore_Autonomous_EMS_V4.md & docs/REQUISITI_TRACCIABILITA.md)
1. **Target Time Default (`R-TGT-02`):** `15:30` (`targetMinuteOfDay = 930`).
2. **Anticipazione Operativa (`R-TGT-03`):** `20 minuti` (`anticipationMinutes = 20`).
   - Orario operativo effettivo: $15:30 - 20\text{ min} = 15:10$ (`effectiveTargetMinute = 910`).
   - Ore residue per la pianificazione a minima potenza:
     $$\text{remainingHours} = \max\left(0.0, \frac{15:10 - \text{now}}{60.0}\right)$$
3. **Finestra Finale di Recupero (`R-TGT-04`):** `60 minuti` (`recoveryMinutes = 60`).
   - Si attiva nell'intervallo $14:30 \le \text{now} < 15:30$.
   - Se permane un deficit rispetto al target, richiede la massima potenza disponibile (`maximumChargePowerW = 2560 W`).

### Chiarimento Formale su "13:00"
- **"13:00" NON È UNA REGOLA FISSA DELL'ALGORITMO** e non costituisce una deadline hardcoded.
- L'orario delle 13:00 è esclusivamente il **risultato empirico simulato nello scenario estivo/sereno (`sunny_day`)**, in cui l'abbondante irraggiamento solare di mezzogiorno (campana FV di 4 kWp) e la carica a surplus portano la batteria al 100% SOC alle 13:00 con 2 ore e mezza di anticipo rispetto alla deadline.
- Nel codice C++ puro [`src/ems/target_aware_ems.cpp`](../src/ems/target_aware_ems.cpp), la logica usa unicamente `targetMinuteOfDay` (15:30), `anticipationMinutes` (20 min -> 15:10) e `recoveryMinutes` (60 min -> 14:30..15:30), senza alcun riferimento costante alle 13:00.

---

## 2. Matrice di Tracciabilità 1:1 dei 59 Requisiti Approvati

| ID Requisito | Descrizione Specifica Approvata | Valore di Progetto | Implementazione Core M4 | Scenario di Test CTest | Esito |
|---|---|---|---|---|---|
| **R-TGT-01** | SOC target configurabile | Default 100% | `EMSConfig::targetSocPct` (100.0) | `scenario_approved_parameters_audit` | Conforme |
| **R-TGT-02** | Orario target configurabile | Default 15:30 | `EMSConfig::targetMinuteOfDay` (930 min) | `scenario_approved_parameters_audit` | Conforme |
| **R-TGT-03** | Anticipo target configurabile | Default 20 min (15:10) | `EMSConfig::anticipationMinutes` (20 min) | `scenario_operational_anticipation_20min_and_recovery_window` | Conforme |
| **R-TGT-04** | Finestra finale di recupero | 60 minuti prima (14:30) | `EMSConfig::recoveryMinutes` (60 min) | `scenario_operational_anticipation_20min_and_recovery_window` | Conforme |
| **R-TGT-05** | Potenza minima compatibile con target | Richiesta minima da modello | `energy_model::requiredPowerW` | `scenario_target_reachable_minimum_power` | Conforme |
| **R-TGT-06** | Ripresa carica se SOC scende prima del target | Trigger su deficit > 0 | `result.energy.deficitKWh > 0.0` | `scenario_low_soc_recovery_window` | Conforme |
| **R-TGT-07** | Target non raggiungibile dichiarato esplicitamente | Stato `NOT_FEASIBLE` | `Feasibility::NotFeasible` & `RecommendationReason::TargetNotFeasible` | `scenario_target_not_reachable` | Conforme |
| **R-NRG-01** | Capacità nominale batteria | 10,24 kWh | `EMSConfig::batteryCapacityKWh` (10.24) | `scenario_approved_parameters_audit` | Conforme |
| **R-NRG-02** | SOC minimo di sicurezza | 15% | `EMSConfig::minimumSocPct` (15.0) | `scenario_approved_parameters_audit` | Conforme |
| **R-NRG-03** | Efficienza di ricarica | 0,90 (90%) | `EMSConfig::chargeEfficiency` (0.90) | `scenario_approved_parameters_audit` | Conforme |
| **R-NRG-04** | Margine di sicurezza fabbisogno | 1,20 (+20%) | `EMSConfig::safetyMargin` (1.20) | `scenario_approved_parameters_audit` | Conforme |
| **R-NRG-05** | Calcolo deficit energetico | `(SOC_target - SOC) * cap` | `energy_model::deficitKWh()` | `scenario_target_reachable_minimum_power` | Conforme |
| **R-NRG-06** | Potenza media richiesta | `deficit / ore_eff / eff` | `energy_model::requiredPowerW()` | `scenario_target_reachable_minimum_power` | Conforme |
| **R-NRG-07** | Proiezione SOC a target | Ritmo reale di carica | `energy_model::projectedSocPct()` | `scenario_unexpected_load_delay` | Conforme |
| **R-NRG-08** | Rilevamento ritardo | Potenza reale < richiesta * 0.85 | `result.energy.delayed` | `scenario_unexpected_load_delay` | Conforme |
| **R-NRG-09** | Stato critico ritardo | Scostamento >= 8% SOC o P >= 90% Pmax | `result.energy.critical` | `scenario_low_soc_recovery_window` | Conforme |
| **R-NRG-10** | Convenzione segno batteria | Segno canonical: >0 carica, <0 scarica | Canonical Heliograph convention | `test_canonical_sign_flip` | Conforme |
| **R-FCS-01** | Accettazione previsione oraria FV | Slot orari solari Solcast | `ForecastSlot` e `ForecastInput::today` | `scenario_target_reachable_minimum_power` | Conforme |
| **R-FCS-02** | Validità temporale previsione | Massimo 14 ore | `EMSConfig::forecastMaximumAgeSeconds` (50400 s) | `scenario_stale_forecast_conservative_fallback` | Conforme |
| **R-FCS-03** | Correzione dinamica su produzione reale | Rapporto clmapato [0.5, 1.0] | `clamp(actualToForecastRatio, 0.5, 1.0)` | `scenario_target_reachable_minimum_power` | Conforme |
| **R-FCS-04** | Riduzione in caso di nebbia | Moltiplicatore 0.30 | `ratio *= 0.30` | `scenario_fog_forecast_reduction_and_hysteresis` | Conforme |
| **R-FCS-05** | Fallback assenza o obsolescenza forecast | Modalità conservativa e safe rate | `RecommendationReason::ForecastUnavailable` | `scenario_stale_forecast_conservative_fallback` | Conforme |
| **R-FOG-01** | Ingresso nebbia da spread T/Td e rad | `(T - Td) <= 1.5 °C && Rad < 100 W/m²` | `fog_detector::update()` | `scenario_fog_forecast_reduction_and_hysteresis` | Conforme |
| **R-FOG-02** | Uscita nebbia con isteresi | `(T - Td) >= 2.5 °C && Rad >= 180 && Ratio >= 0.75` | `fog_detector::update()` | `scenario_fog_forecast_reduction_and_hysteresis` | Conforme |
| **R-FOG-03** | Priorità sensore nebbia dedicato | `stableFog.has_value()` | `weather.stableFog` priorità assoluta | `scenario_fog_forecast_reduction_and_hysteresis` | Conforme |
| **R-LOD-01** | Profilo carico base feriale | 0,6 kW (600 W) | `EMSConfig::weekdayLoadKW` (0.6) | `scenario_approved_parameters_audit` | Conforme |
| **R-LOD-02** | Profilo carico base weekend | 0,9 kW (900 W) | `EMSConfig::weekendLoadKW` (0.9) | `scenario_approved_parameters_audit` | Conforme |
| **R-LOD-03** | Calcolo surplus netto atteso | `Forecast_netto = FV - Carico` | `surplusKWh = max(0, forecast - load)` | `scenario_target_reachable_minimum_power` | Conforme |
| **R-SEA-01** | Periodo invernale | 25 Ottobre – 28 Febbraio | `winterStartMonthDay=1025`, `winterEnd=228` | `scenario_seasonal_priority` | Conforme |
| **R-SEA-02** | Valutazione stagionale alle 08:30 | Minuto del giorno 510 | `seasonalEvaluationMinute = 510` | `scenario_seasonal_priority` | Conforme |
| **R-SEA-03** | Priorità carica invernale | `Surplus < Deficit*1.1 && !DomaniFavorevole` | `seasonal_strategy::priorityCharge()` | `scenario_seasonal_priority` | Conforme |
| **R-SEA-04** | Criterio domani favorevole | `Forecast_domani >= 7 kWh && SOC >= 30%` | `tomorrowFavorableKWh=7.0`, `tomorrowMinSoc=30.0` | `scenario_seasonal_priority` | Conforme |
| **R-SEA-05** | Decisione priorità come stato strategico | Registrata come advisory, no write inverter | `result.priorityChargeActive` puro stato | `scenario_seasonal_priority` | Conforme |
| **R-CAL-01** | Calibrazione batteria a SOC pieno | 100% stabile per 5 minuti (300 s) | `fullSocCalibrationDurationSeconds = 300` | `scenario_calibration_tracker` | Conforme |
| **R-CAL-02** | Ritorno a strategia ordinaria post-calibrazione | Ritorno a target longevity/SelfUse | `state.calibrated = true`, reset target | `scenario_calibration_tracker` | Conforme |
| **R-CAL-03** | Profilo longevità opzionale | Default 100%, non blocca calibrazione | `EMSConfig::longevityMaximumSocPct` | `scenario_calibration_tracker` | Conforme |
| **R-RAM-01** | Rampa salita potenza | Massimo 256 W/ciclo (1 A eq.) | `EMSConfig::rampUpWPerCycle` (256.0) | `scenario_clamp_ramp_deadband_hysteresis` | Conforme |
| **R-RAM-02** | Rampa discesa potenza | Massimo 410 W/ciclo | `EMSConfig::rampDownWPerCycle` (410.0) | `scenario_clamp_ramp_deadband_hysteresis` | Conforme |
| **R-RAM-03** | Isteresi variazioni minori | 100 W | `EMSConfig::hysteresisW` (100.0) | `scenario_clamp_ramp_deadband_hysteresis` | Conforme |
| **R-RAM-04** | Deadband potenza minima | 30 W | `EMSConfig::deadbandW` (30.0) | `scenario_clamp_ramp_deadband_hysteresis` | Conforme |
| **R-PWR-01** | Potenza ricarica massima | 2560 W (10 A @ 256 V / 50 A @ 51.2 V) | `EMSConfig::maximumChargePowerW` (2560.0) | `scenario_approved_parameters_audit` | Conforme |
| **R-PWR-02** | Potenza ricarica minima | 256 W (1 A eq.) | `EMSConfig::minimumChargePowerW` (256.0) | `scenario_approved_parameters_audit` | Conforme |
| **R-PWR-03** | Divieto scarica forzata (Phase 0) | `allow_discharge = false` | `EMSConfig::allowDischarge` (false) | `scenario_discharge_gate` | Conforme |
| **R-OVR-01** | Override manuale tipizzato | Setpoint advisory opzionale | `OverrideInput::requestedBatteryPowerW` | `scenario_typed_override_is_safety_clamped` | Conforme |
| **R-OVR-02** | Clamping di sicurezza sugli override | I limiti fisici prevalgono sempre | `clamp(override, 0, Pmax)` | `scenario_typed_override_is_safety_clamped` | Conforme |
| **R-DIA-01** | Raccomandazione advisory pura | Nessun invio Modbus, nessun I/O | `Recommendation` struct isolata | `scenario_target_reachable_minimum_power` | Conforme |
| **R-DIA-02** | Tracciabilità reason code | Reason esplicito in ogni ciclo | `RecommendationReason` enum | Tutti i 17 scenari | Conforme |
| **R-DIA-03** | Esposizione metriche energetiche | Deficit, P_req, SOC_proj, criticità | `EnergyAssessment` struct | `scenario_target_reachable_minimum_power` | Conforme |
| **R-DIA-04** | Modalità notturna a riposo | 0 W advisory quando buio e no forecast | `RecommendationReason::NightIdle` | `scenario_night_safe_idle` | Conforme |
| **R-SAF-01** | Validazione qualità dati telemetria | Check freshness e validità | `coreDataValid()` e `Datum::actionable` | `scenario_invalid_and_stale_data_safe_idle` | Conforme |
| **R-SAF-02** | Dati invalidi non convertiti in zero | Invalid/CommError/Unavailable -> Safe Idle | `scenario_invalid_data_never_converted_to_valid_zero` | Conforme |
| **R-SAF-03** | Failsafe a 0 W in caso di errore dati | Advisory 0 W e reason DataInvalid | `scenario_invalid_data_never_converted_to_valid_zero` | Conforme |
| **R-SAF-04** | Stato di fattibilità Unknown su errore | Feasibility::Unknown se telemetria non valida | `scenario_invalid_data_never_converted_to_valid_zero` | Conforme |
| **R-SAF-05** | Limiti hardware invalicabili | Potenza tra 0 W e 2560 W | `clamp(P, 0, maximumChargePowerW)` | Tutti gli scenari | Conforme |
| **R-SAF-06** | Isolamento architetturale totale da transport | Core separato compilato staticamente | `libtarget-aware-ems-core.a` | Verificato in build | Conforme |
| **R-QAL-01** | Tracciamento qualità singoli campi | `DataQuality` su SOC, potenze, meteo | `Datum<T>` template | `scenario_invalid_and_stale_data_safe_idle` | Conforme |
| **R-QAL-02** | Gestione obsolescenza campioni | Timeout 120 s telemetria, 14 h forecast | `telemetryMaximumAgeSeconds`, `forecastMaximumAgeSeconds` | `scenario_stale_forecast_conservative_fallback` | Conforme |
| **R-MOD-01** | Compatibilità con modello External EMS | Potenza batteria in Watt | Unità `W` standard Heliograph | Conforme |
