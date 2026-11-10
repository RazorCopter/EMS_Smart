# Matrice di Tracciabilità Requisiti — Smart Energy Manager V4

**Data:** 2026-11-10  
**Milestone:** M1  
**Riferimento:** [`Doc/PIANO_Afore_Autonomous_EMS_V4.md`](../../Doc/PIANO_Afore_Autonomous_EMS_V4.md) sezione 2

---

## 0. Legenda

### Status
- **ABSENT**: requisito non implementato, nessun codice presente
- **PARTIAL**: requisito implementato parzialmente o in modo preliminare/generico
- **IMPLEMENTED_NOT_TESTED**: codice authored che copre il requisito, mai eseguito (pending M3)
- **VERIFIED**: requisito coperto da test passato (nessuno: esecuzione test pending M3+)

### Colonne
| Colonna | Descrizione |
|---|---|
| ID | Identificativo del requisito (dal piano) |
| Full requirement | Testo completo del requisito |
| Source type | Tassonomia controllata: APPROVED_PLAN, BLUEPRINT_V3_3, AFORE_DOCUMENTATION, EXPLICIT_PROJECT_DECISION, PROPOSED_NOT_APPROVED |
| Source reference | Documento, sezione, file o decisione puntuale |
| Existing file/function | File e/o funzione C++ che implementa (se presente) |
| Status | Uno tra ABSENT, PARTIAL, IMPLEMENTED_NOT_TESTED, VERIFIED |
| Compatibility | ALIGNED, CONFLICTING, NOT_APPLICABLE (conflitti con altri requisiti) |
| Gap | Descrizione sintetica di cosa manca o è incompleto |
| Target module | Modulo previsto per l'implementazione completa (M4+) |
| Planned unit test | Nome del test unitario progettato |
| Planned integration test | Scenario di integrazione per coprire il requisito |
| Dependency | Altri requisiti o dati esterni necessari |
| Milestone | M4 (core EMS) o M5 (replay) o successiva |
| Notes | Osservazioni, ambiguità, decisioni |


### Tassonomia delle fonti
- **APPROVED_PLAN**: requisito contenuto nel piano approvato.
- **BLUEPRINT_V3_3**: comportamento derivato dalla blueprint Home Assistant V3.3.
- **AFORE_DOCUMENTATION**: comportamento documentato dal costruttore.
- **EXPLICIT_PROJECT_DECISION**: decisione esplicita assunta nel progetto.
- **PROPOSED_NOT_APPROVED**: proposta non ancora approvata, non vincolante.

### Notazione funzioni CP4
Le funzioni del controller CP4 existente sono indicate come:
- `shadow_ems_controller.cpp::applySelfConsumptionStrategy()`: funzione preliminare generica
- `shadow_ems_types.h::BatteryConstraints`: tipo preliminare
- Questi sono authored, non compilati né testati (IMPLEMENTED_NOT_TESTED se coprono un requisito, altrimenti contribuiscono a uno stato PARTIAL).

---

## 1. R-TGT — Target di carica

### R-TGT-01
| Campo | Valore |
|---|---|
| **ID** | R-TGT-01 |
| **Full requirement** | SOC target configurabile (default 100%) |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.1, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** — `shadow_ems_controller` non ha campo target SOC |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessun campo `targetSOC` in config o stato; controller CP4 non ragiona per target |
| **Target module** | `target_planner` (M4) |
| **Planned unit test** | `test_target_planner::test_target_soc_configurable` |
| **Planned integration test** | Scenario M4 #1, #5: target SOC diverso da 100% |
| **Dependency** | R-TGT-02 (orario), R-NRG-05 (deficit), R-NRG-06 (potenza media) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3 input: `input_number.sem_target_soc` |

### R-TGT-02
| Campo | Valore |
|---|---|
| **ID** | R-TGT-02 |
| **Full requirement** | Orario target configurabile (default 15:30) |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.1, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessun campo `targetTime`; controller non usa tempo corrente né deadline |
| **Target module** | `target_planner` (M4) |
| **Planned unit test** | `test_target_planner::test_target_time_configurable` |
| **Planned integration test** | Scenario M4 #1: orario target 15:30; #6: target non raggiungibile per tempo insufficiente |
| **Dependency** | R-TGT-01, R-TGT-03, R-TGT-04, R-NRG-06 (potenza media richiesta = deficit / ore effettive) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3 input: `input_datetime.sem_target_time`; gestisce fuso orario |

### R-TGT-03
| Campo | Valore |
|---|---|
| **ID** | R-TGT-03 |
| **Full requirement** | Anticipo operativo sul target (20 min) |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.1, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna logica di anticipo; finestra critica non implementata |
| **Target module** | `target_planner::computeCriticalWindow()` (M4) |
| **Planned unit test** | `test_target_planner::test_anticipation_window_20min` |
| **Planned integration test** | Scenario M4 #1: anticipo 15:10–15:30 |
| **Dependency** | R-TGT-02, R-TGT-04 (finestra recupero) |
| **Milestone** | **M4** |
| **Notes** | Anticipo serve per riserva temporale; finestra critica = target_time − 20min fino a target_time |

### R-TGT-04
| Campo | Valore |
|---|---|
| **ID** | R-TGT-04 |
| **Full requirement** | Finestra di recupero finale (priorità massima al target, 60 min) |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.1 |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna finestra di recovery; priorità target non implementata |
| **Target module** | `target_planner::isInRecoveryWindow()` (M4) |
| **Planned unit test** | `test_target_planner::test_recovery_window_60min` |
| **Planned integration test** | Scenario M4 #5: SOC scende sotto target dopo 100%, ripresa carica entro finestra |
| **Dependency** | R-TGT-02, R-TGT-03, R-NRG-09 (stato critico) |
| **Milestone** | **M4** |
| **Notes** | Recovery window = target_time − 60min fino a target_time; in questa finestra: carica massima consentita da vincoli, ignora ottimizzazioni |

### R-TGT-05
| Campo | Valore |
|---|---|
| **ID** | R-TGT-05 |
| **Full requirement** | Usare la potenza minima compatibile con il raggiungimento del target |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.1, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** — CP4 usa tutta la potenza disponibile (export o power limits) |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Controller CP4 non calcola potenza minima necessaria; usa massima disponibile |
| **Target module** | `target_planner::computeMinimumPowerRequired()` (M4) |
| **Planned unit test** | `test_target_planner::test_minimum_power_for_target` |
| **Planned integration test** | Scenario M4 #1: energy_deficit_kwh=2.5, remaining_effective_time_h=5, charge_efficiency=0.90 → required_average_charge_w=2.5/5/0.90*1000=556W; available_surplus_w=3000W → requested_charge_w=min(3000, 556, maximum_allowed_w)=556W |
| **Dependency** | R-TGT-02, R-NRG-06 (potenza media richiesta) |
| **Milestone** | **M4** |
| **Notes** | Ottimizzazione: fuori dalla finestra critica, carica solo il minimo necessario; preserva batteria e riduce cicli. Esempio calcolo: deficit 2.5 kWh, tempo residuo 5h, efficienza 0.90 → potenza media richiesta 556W |

### R-TGT-06
| Campo | Valore |
|---|---|
| **ID** | R-TGT-06 |
| **Full requirement** | Se il SOC scende sotto target prima dell'orario (es. lavastoviglie dopo il 100%), riprendere la ricarica |
| **Source type** | APPROVED_PLAN, EXPLICIT_PROJECT_DECISION |
| **Source reference** | Piano V4 §2.1, comportamento osservato 8/11 e accettato |
| **Existing file/function** | **ABSENT** — CP4 non confronta SOC con target; carica solo su export |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Logica "SOC < target && time < target_time" non presente |
| **Target module** | `target_planner::shouldResumeCharge()` (M4) |
| **Planned unit test** | `test_target_planner::test_resume_charge_after_drop` |
| **Planned integration test** | Scenario M4 #5: SOC 100% alle 13:00, carico 2kW → SOC 85% alle 14:00 → ripresa carica prima delle 15:30 |
| **Dependency** | R-TGT-01, R-TGT-02 |
| **Milestone** | **M4** |
| **Notes** | Caso reale: dopo raggiungimento 100% mattina, carico pomeridiano fa scendere SOC → EMS deve riprendere carica se ancora prima di target_time |

### R-TGT-07
| Campo | Valore |
|---|---|
| **ID** | R-TGT-07 |
| **Full requirement** | Dichiarare esplicitamente target non raggiungibile quando energia/limiti non bastano (stato `NOT_FEASIBLE`) |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.1 |
| **Existing file/function** | **ABSENT** — CP4 non ha stati/reason `NOT_FEASIBLE` |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna logica di feasibility check; decisione sempre ottimistica |
| **Target module** | `target_planner::checkFeasibility()` (M4) |
| **Planned unit test** | `test_target_planner::test_not_feasible_declaration` |
| **Planned integration test** | Scenario M4 #4, #6: FV insufficiente o nebbia persistente → target dichiarato NOT_FEASIBLE |
| **Dependency** | R-NRG-05 (deficit), R-PV-03 (surplus previsto), R-NRG-06 (potenza media richiesta vs limiti) |
| **Milestone** | **M4** |
| **Notes** | Caso 9/11 reale: SOC 19%, FV previsto 2,7 kWh, deficit 8 kWh → NOT_FEASIBLE; evita carica massima inutile e segnala presto all'utente |

---

## 2. R-NRG — Modello energetico

### R-NRG-01
| Campo | Valore |
|---|---|
| **ID** | R-NRG-01 |
| **Full requirement** | Capacità batteria = 10,24 kWh |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.2, impianto reale |
| **Existing file/function** | `shadow_ems_types.h::BatteryConstraints::ratedCapacityKWh = 10.24` |
| **Status** | **IMPLEMENTED_NOT_TESTED** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Valore presente in CP4 ma solo per uso interno constraint SOC; non usato per calcoli energetici completi |
| **Target module** | `energy_model` (M4); già presente in constraints |
| **Planned unit test** | `test_energy_model::test_capacity_kwh` (verificare uso corretto nei calcoli) |
| **Planned integration test** | Scenario M4 tutti: capacity usata per calcolo deficit/surplus |
| **Dependency** | R-NRG-05, R-NRG-06 |
| **Milestone** | **M4** (estendere uso) |
| **Notes** | Valore authored in CP4; va esteso l'uso per calcoli deficit, proiezione SOC, feasibility |

### R-NRG-02
| Campo | Valore |
|---|---|
| **ID** | R-NRG-02 |
| **Full requirement** | Tensione nominale (per conversioni A↔W) = 51,2 V |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.2, spec batteria |
| **Existing file/function** | **ABSENT** (opzionale: conversione non necessaria ma disponibile se serve diagnostica) |
| **Status** | **ABSENT** |
| **Compatibility** | **NOT_APPLICABLE** — Requisito opzionale/negativo |
| **Gap** | Non presente; protocollo Afore fornisce già potenza in W (`0x07D8`) e corrente in A (`0x07D5`) separatamente → conversione non necessaria nel codice EMS |
| **Target module** | `energy_model` (opzionale M4 se serve conversione) |
| **Planned unit test** | `test_energy_model::test_voltage_nominal` (opzionale) |
| **Planned integration test** | M4 (opzionale): conversione A↔W per diagnostica |
| **Dependency** | Nessuna; valore di riferimento per comprensione umana |
| **Milestone** | M4 (opzionale, bassa priorità) |
| **Notes** | Blueprint V3.3 converte limite corrente A→W; nel piano V4 i comandi sono già in W (R-CMD-01); conversione opzionale se serve diagnostica. Requisito mantenuto per completezza documentale |

### R-NRG-03
| Campo | Valore |
|---|---|
| **ID** | R-NRG-03 |
| **Full requirement** | Efficienza di carica = 0,90 |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.2, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Efficienza non considerata nei calcoli potenza/deficit |
| **Target module** | `energy_model::chargeEfficiency` (M4) |
| **Planned unit test** | `test_energy_model::test_charge_efficiency` |
| **Planned integration test** | Scenario M4 #1, #6: calcolo potenza media richiesta considera efficienza → richiesta maggiore del deficit nominale |
| **Dependency** | R-NRG-06 (potenza media richiesta = deficit / ore / efficienza) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `charge_efficiency: 0.9`; essenziale per feasibility check (deficit reale > deficit nominale) |

### R-NRG-04
| Campo | Valore |
|---|---|
| **ID** | R-NRG-04 |
| **Full requirement** | Margine di sicurezza sul fabbisogno = 1,2 |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.2, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessun margine applicato ai calcoli |
| **Target module** | `energy_model::safetyMargin` (M4) |
| **Planned unit test** | `test_energy_model::test_safety_margin` |
| **Planned integration test** | Scenario M4 #6: deficit nominale 5 kWh × 1,2 = 6 kWh richiesto |
| **Dependency** | R-NRG-05, R-NRG-06 |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `safety_margin: 1.2`; compensa incertezze (forecast, load, efficienza variabile) |

### R-NRG-05
| Campo | Valore |
|---|---|
| **ID** | R-NRG-05 |
| **Full requirement** | Deficit = (SOC target − SOC attuale) × capacità [kWh] |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.2, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** — CP4 non calcola deficit |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Formula assente; controller CP4 non usa concetto di deficit energetico |
| **Target module** | `energy_model::computeDeficit()` (M4) |
| **Planned unit test** | `test_energy_model::test_deficit_computation` |
| **Planned integration test** | Scenario M4 #1: SOC 30%, target 100%, capacity 10.24 kWh → deficit 7.168 kWh |
| **Dependency** | R-TGT-01 (target SOC), R-NRG-01 (capacity) |
| **Milestone** | **M4** |
| **Notes** | Fondamentale per R-NRG-06 (potenza media richiesta) e R-TGT-07 (feasibility) |

### R-NRG-06
| Campo | Valore |
|---|---|
| **ID** | R-NRG-06 |
| **Full requirement** | Potenza media richiesta = deficit / ore effettive / efficienza [W] |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.2, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Formula assente; controller CP4 usa potenza massima disponibile, non calcolata |
| **Target module** | `energy_model::computeRequiredPower()` o `target_planner::computeMinimumPowerRequired()` (M4) |
| **Planned unit test** | `test_energy_model::test_required_power_computation` |
| **Planned integration test** | Scenario M4 #1: deficit 7.168 kWh, ore 5.5, eff 0.9 → richiesta ≈ 1448 W |
| **Dependency** | R-NRG-05 (deficit), R-TGT-02 (target time), R-NRG-03 (efficienza) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `required_charge_power_w`; usato per R-TGT-05 (potenza minima) e R-NRG-09 (stato critico) |

### R-NRG-07
| Campo | Valore |
|---|---|
| **ID** | R-NRG-07 |
| **Full requirement** | Proiezione SOC al target dal ritmo reale di carica [%] |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.2, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna proiezione; controller non stima SOC futuro |
| **Target module** | `energy_model::projectSOC()` (M4) |
| **Planned unit test** | `test_energy_model::test_soc_projection` |
| **Planned integration test** | Scenario M4 #1, #6: proiezione SOC alle 15:30 dato ritmo attuale |
| **Dependency** | R-NRG-06 (potenza richiesta vs attuale), R-NRG-08 (feedback ritmo) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `projected_soc_at_target`; confronto con target SOC determina scostamento (R-NRG-09) |

### R-NRG-08
| Campo | Valore |
|---|---|
| **ID** | R-NRG-08 |
| **Full requirement** | Feedback sul ritmo reale: ritardo se potenza reale < richiesta × 0,85 (tolleranza 0,85) |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.2, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessun confronto potenza reale vs richiesta; nessun rilevamento ritardo |
| **Target module** | `energy_model::detectDelay()` (M4) |
| **Planned unit test** | `test_energy_model::test_delay_detection` |
| **Planned integration test** | Scenario M4 #6: richiesta 1500W, reale 1000W → ritardo rilevato |
| **Dependency** | R-NRG-06 (potenza richiesta), batteria power attuale |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `is_delayed`; segnala che ritmo attuale insufficiente → allerta o stato critico (R-NRG-09) |

### R-NRG-09
| Campo | Valore |
|---|---|
| **ID** | R-NRG-09 |
| **Full requirement** | Stato critico se scostamento proiettato ≥ 8 punti SOC o richiesta ≥ 90% del massimo |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.2, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna logica di criticità; controller non alza priorità in situazioni critiche |
| **Target module** | `energy_model::isCritical()` (M4) |
| **Planned unit test** | `test_energy_model::test_critical_state` |
| **Planned integration test** | Scenario M4 #4, #6: scostamento ≥ 8% o potenza ≥ 90% max → stato CRITICAL → no rampa lenta (R-CMD-04) |
| **Dependency** | R-NRG-07 (proiezione SOC), R-NRG-06 (potenza richiesta vs max) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `is_critical`; segnala urgenza → rampa immediata, no smoothing (R-CMD-04) |

### R-NRG-10
| Campo | Valore |
|---|---|
| **ID** | R-NRG-10 |
| **Full requirement** | Potenza batteria Afore: negativa in carica, positiva in scarica (da confermare empiricamente) |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.2, osservazione preliminare Solarman |
| **Existing file/function** | `afore_telemetry_engine.cpp::decodeBatteryBlock()`: scale=-1 sul registro `0x07D8` |
| **Status** | **IMPLEMENTED_NOT_TESTED** |
| **Compatibility** | **ALIGNED** — trasformazione coerente col requisito, segno non ancora verificato empiricamente |
| **Risk** | **CRITICAL_UNVERIFIED_SIGN_CONVENTION** |
| **Gap** | Convenzione implementata (flip segno) ma **mai confermata su bus reale** |
| **Target module** | `afore_telemetry_engine` (CP3 authored) |
| **Planned unit test** | `test_afore_telemetry::test_battery_power_sign_flip` (authored, pending M3) |
| **Planned integration test** | **M8 read-only**: correlare raw `0x07D8`, Battery Status, corrente e variazione SOC durante carica/scarica certa; nessuna scrittura |
| **Dependency** | Validazione empirica M7/M8 con inverter reale |
| **Milestone** | **M8** (validazione empirica, non M4) |
| **Notes** | Critico: se convenzione errata, tutte le decisioni EMS sono invertite; profilo TOML ha `scale = -1`; conferma attende M8 |

---

## 3. R-PV — Previsione fotovoltaica

### R-PV-01
| Campo | Valore |
|---|---|
| **ID** | R-PV-01 |
| **Full requirement** | Usare `detailedForecast` a slot di 30 min, stima selezionabile (`pv_estimate`, alternative 10/90) |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.3, Solcast API via HA |
| **Existing file/function** | **ABSENT** — nessuna integrazione Solcast |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessun forecast; controller CP4 usa solo stato istantaneo |
| **Target module** | `pv_forecast::loadFromHA()` (M4) |
| **Planned unit test** | `test_pv_forecast::test_load_detailed_forecast` |
| **Planned integration test** | Scenario M4 #1, #2, #3: forecast Solcast fornito da HA → EMS usa `detailedForecast` a 30min |
| **Dependency** | Integrazione HA (M10), sensor `sensor.solcast_pv_forecast_forecast_*` |
| **Milestone** | **M4** (logica) + **M10** (ingresso MQTT da HA) |
| **Notes** | Blueprint V3.3: `solcast_forecast_today`, `solcast_forecast_tomorrow`; stima selezionabile (`pv_estimate` default, `pv_estimate10`, `pv_estimate90`) |

### R-PV-02
| Campo | Valore |
|---|---|
| **ID** | R-PV-02 |
| **Full requirement** | Età massima previsione valida = 14 h |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.3, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna verifica età forecast; nessuna scadenza |
| **Target module** | `pv_forecast::isStale()` (M4) |
| **Planned unit test** | `test_pv_forecast::test_forecast_expiry` |
| **Planned integration test** | Scenario M4 #7: Solcast scaduto (> 14h) → modalità conservativa (R-PV-05) |
| **Dependency** | R-PV-01, timestamp ultimo polling Solcast da HA |
| **Milestone** | **M4** |
| **Notes** | Solcast aggiorna ogni ~ora; 14h margine generoso; se scaduto → R-PV-05 (fallback conservativo) |

### R-PV-03
| Campo | Valore |
|---|---|
| **ID** | R-PV-03 |
| **Full requirement** | Surplus utile = previsione corretta − carico domestico, solo fino all'orario target [kWh] |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.3, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — estimated-load è limitazione accuracy, non conflitto con R-LOAD-04 |
| **Gap** | Nessun calcolo surplus previsto; nessuna integrazione carico |
| **Target module** | `pv_forecast::computeSurplus()` (M4) |
| **Planned unit test** | `test_pv_forecast::test_surplus_computation` |
| **Planned integration test** | Scenario M4 #1: forecast oggi 8 kWh, carico 0.6 kW × 7h = 4.2 kWh → surplus 3.8 kWh |
| **Dependency** | R-PV-01, R-PV-04 (correzione FV reale), R-LOAD-01/02 (carico), R-TGT-02 (orario target) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `surplus_energy_kwh`; solo energia fino a target_time conta per feasibility |

### R-PV-04
| Campo | Valore |
|---|---|
| **ID** | R-PV-04 |
| **Full requirement** | Correzione con FV reale (rapporto reale/previsto) sulle prossime ore (orizzonte 2 h, minimo 0,5, massimo 1,0) |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.3, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna correzione dinamica forecast; nessun confronto FV reale vs previsto |
| **Target module** | `pv_forecast::applyRealCorrection()` (M4) |
| **Planned unit test** | `test_pv_forecast::test_real_correction_factor` |
| **Planned integration test** | Scenario M4 #2: FV reale 80% del previsto → forecast prossime 2h ridotto a 0.8× |
| **Dependency** | R-PV-01, PV power attuale (`dc.power.total`) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `forecast_correction_factor`; migliora accuracy usando dato reale corrente; clamp [0.5, 1.0] per stabilità. Valore 1.5 disponibile come PROPOSED_NOT_APPROVED per test M5 se necessario |

### R-PV-05
| Campo | Valore |
|---|---|
| **ID** | R-PV-05 |
| **Full requirement** | Senza Solcast valido: modalità conservativa (priorità target, nessuna ottimizzazione aggressiva) |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.3 |
| **Existing file/function** | **ABSENT** — CP4 non ha modalità conservativa/aggressiva |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna logica fallback senza forecast |
| **Target module** | `pv_forecast::isAvailable()` + `target_planner` (M4) |
| **Planned unit test** | `test_pv_forecast::test_conservative_mode_without_solcast` |
| **Planned integration test** | Scenario M4 #7: Solcast assente o scaduto → EMS usa solo target, ignora ottimizzazioni |
| **Dependency** | R-PV-02 (scadenza), R-TGT-05 (potenza minima vs ottimizzazione) |
| **Milestone** | **M4** |
| **Notes** | Modalità conservativa: carica per raggiungere target senza cercare di minimizzare potenza (ignora R-TGT-05); fail-safe ragionevole |

### R-PV-06
| Campo | Valore |
|---|---|
| **ID** | R-PV-06 |
| **Full requirement** | Il controller deve funzionare anche senza Solcast usando solo dati locali (degrado dichiarato) |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.3, R-SAF-06 (autonomia senza HA) |
| **Existing file/function** | **PARTIAL** — CP4 usa solo dati locali (non Solcast) ma non implementa target-aware |
| **Status** | **PARTIAL** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Controller CP4 funziona senza Solcast (usa solo battery/grid/pv locali) ma è generico (no target); modalità degradata target-aware assente |
| **Target module** | `target_planner::degradedMode()` (M4) |
| **Planned unit test** | `test_target_planner::test_degraded_operation_without_solcast` |
| **Planned integration test** | Scenario M4 #7: EMS funziona senza Solcast, con strategia conservativa |
| **Dependency** | R-PV-05, R-SAF-06 |
| **Milestone** | **M4** |
| **Notes** | CP4 già usa solo dati locali ma è self-consumption generico; M4 estende con target-aware degradato |

---

## 4. R-FOG — Nebbia dinamica

### R-FOG-01
| Campo | Valore |
|---|---|
| **ID** | R-FOG-01 |
| **Full requirement** | Nebbia se (T − Td) ≤ 1,5 °C e radiazione < 100 W/m² |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.4, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna rilevazione nebbia; controller ignora condizioni meteo |
| **Target module** | `fog_detector::detectEntry()` (M4) |
| **Planned unit test** | `test_fog_detector::test_fog_entry_condition` |
| **Planned integration test** | Scenario M4 #3: T=5°C, Td=4°C, rad=50 W/m² → nebbia rilevata |
| **Dependency** | Sensori HA: `sensor.temperatura_esterna`, `sensor.punto_rugiada`, `sensor.radiazione` (M10) |
| **Milestone** | **M4** (logica) + **M10** (ingresso dati HA) |
| **Notes** | Blueprint V3.3: `is_fog`; formula con isteresi (R-FOG-02) |

### R-FOG-02
| Campo | Valore |
|---|---|
| **ID** | R-FOG-02 |
| **Full requirement** | Uscita nebbia se (T − Td) ≥ 2,5 °C, radiazione ≥ 180 W/m², FV/Solcast ≥ 0,75 (isteresi) |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.4, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna uscita nebbia con isteresi |
| **Target module** | `fog_detector::detectExit()` (M4) |
| **Planned unit test** | `test_fog_detector::test_fog_exit_hysteresis` |
| **Planned integration test** | Scenario M4 #3: nebbia iniziale, poi T−Td ≥ 2.5°C + rad ≥ 180 W/m² → uscita nebbia |
| **Dependency** | R-FOG-01, R-PV-04 (rapporto FV reale/previsto) |
| **Milestone** | **M4** |
| **Notes** | Isteresi evita oscillazioni; condizioni uscita più stringenti di ingresso |

### R-FOG-03
| Campo | Valore |
|---|---|
| **ID** | R-FOG-03 |
| **Full requirement** | Preferire il sensore stabilizzato `binary_sensor.sem_nebbia_stabile` (fallback sul calcolo istantaneo) |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.4, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessun ingresso da HA; nessun fallback |
| **Target module** | `fog_detector::loadFromHA()` con fallback (M4) |
| **Planned unit test** | `test_fog_detector::test_prefer_stable_sensor` |
| **Planned integration test** | Scenario M4 #3: usare `binary_sensor.sem_nebbia_stabile` se disponibile, altrimenti calcolo interno |
| **Dependency** | R-FOG-01, R-FOG-02, integrazione HA (M10) |
| **Milestone** | **M4** (logica) + **M10** (ingresso HA) |
| **Notes** | Blueprint V3.3 già ha sensore stabilizzato con template e ritardo; EMS usa quello quando disponibile |

### R-FOG-04
| Campo | Valore |
|---|---|
| **ID** | R-FOG-04 |
| **Full requirement** | Con nebbia: fattore FV sulle prossime ore = 0,3 |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.4, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessun fattore di riduzione FV per nebbia |
| **Target module** | `pv_forecast::applyFogFactor()` (M4) |
| **Planned unit test** | `test_pv_forecast::test_fog_reduction_factor` |
| **Planned integration test** | Scenario M4 #3, #4: nebbia → forecast × 0.3 → target potenzialmente NOT_FEASIBLE |
| **Dependency** | R-FOG-01, R-PV-01, R-PV-03 (surplus) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `fog_reduction_factor: 0.3`; riduzione drastica necessaria perché nebbia persiste ore |

### R-FOG-05
| Campo | Valore |
|---|---|
| **ID** | R-FOG-05 |
| **Full requirement** | Nessun orario rigido di dissoluzione nebbia (solo diagnostica) |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.4 |
| **Existing file/function** | **ABSENT** — requisito negativo: verifica assenza hard-coded time |
| **Status** | **ABSENT** |
| **Compatibility** | **NOT_APPLICABLE** — Requisito opzionale/negativo |
| **Gap** | Requisito negativo: verificare assenza costanti orarie hard-coded |
| **Target module** | N/A |
| **Planned unit test** | N/A (verifica assenza di costanti orarie hard-coded) |
| **Planned integration test** | Scenario M4 #3: nebbia persiste oltre le 10:00 senza dissoluzione forzata |
| **Dependency** | Nessuna |
| **Milestone** | M4 (verifica negativa) |
| **Notes** | Requisito negativo: blueprint V3.2 aveva orario fisso dissoluzione (rimosso in V3.3) → EMS non deve assumerlo. Verifica M4: grep per costanti orarie hard-coded |

---

## 5. R-LOAD — Carichi domestici

### R-LOAD-01
| Campo | Valore |
|---|---|
| **ID** | R-LOAD-01 |
| **Full requirement** | Carico feriale stimato = 0,6 kW |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.5, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna stima carico; controller ignora consumo domestico |
| **Target module** | `load_profile::weekdayLoad` (M4) |
| **Planned unit test** | `test_load_profile::test_weekday_load` |
| **Planned integration test** | Scenario M4 #1, #9: lun-ven → carico 0.6 kW usato in calcolo surplus (R-PV-03) |
| **Dependency** | R-PV-03 (surplus = FV − carico) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `base_load_weekday: 0.6`; valore configurabile; usato finché R-LOAD-04 non disponibile |

### R-LOAD-02
| Campo | Valore |
|---|---|
| **ID** | R-LOAD-02 |
| **Full requirement** | Carico weekend stimato = 0,9 kW |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.5, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna stima weekend |
| **Target module** | `load_profile::weekendLoad` (M4) |
| **Planned unit test** | `test_load_profile::test_weekend_load` |
| **Planned integration test** | Scenario M4 #9: sabato/domenica → carico 0.9 kW |
| **Dependency** | R-LOAD-03 (giorni weekend) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `base_load_weekend: 0.9`; carico maggiore weekend per maggiore presenza domestica |

### R-LOAD-03
| Campo | Valore |
|---|---|
| **ID** | R-LOAD-03 |
| **Full requirement** | Giorni weekend configurabili (default: sab, dom) |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.5, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna configurazione giorni weekend |
| **Target module** | `load_profile::weekendDays` (M4) |
| **Planned unit test** | `test_load_profile::test_weekend_days_configurable` |
| **Planned integration test** | Scenario M4 #9: config weekend [Sat, Sun] → carico 0.9 kW solo sab/dom |
| **Dependency** | R-LOAD-02 |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `weekend_days: [6, 0]` (0=dom, 6=sab); configurabile per festività o turni |

### R-LOAD-04
| Campo | Valore |
|---|---|
| **ID** | R-LOAD-04 |
| **Full requirement** | Quando validato, sostituire la stima con il carico misurato dal bilancio (fase successiva) |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.5 |
| **Existing file/function** | **ABSENT** — non prioritario, fase successiva |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Carico reale non misurato; solo stima |
| **Target module** | `load_profile::useMeasured()` (fase post-M6) |
| **Planned unit test** | `test_load_profile::test_measured_load_override` (fase successiva) |
| **Planned integration test** | Scenario post-M6: carico misurato da `grid + pv − battery` sostituisce stima |
| **Dependency** | Bilancio energetico, validazione power balance (M8) |
| **Milestone** | **Fase successiva** (post-M6, non bloccante per M4) |
| **Notes** | Blueprint V3.3 usa stima; misurato migliora accuracy ma richiede bilancio valido (±150W) |

---

## 6. R-SEA — Strategia stagionale

### R-SEA-01
| Campo | Valore |
|---|---|
| **ID** | R-SEA-01 |
| **Full requirement** | Valutazione giornaliera della priorità di carica (ore 08:30) |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.6, Blueprint V3.3 HA (automation trigger time) |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna valutazione giornaliera; controller non ha concetto di tempo assoluto |
| **Target module** | `seasonal_strategy::evaluateDaily()` (M4) |
| **Planned unit test** | `test_seasonal_strategy::test_daily_evaluation_time` |
| **Planned integration test** | Scenario M4 #8: valutazione alle 08:30 → decisione priorità carica per oggi |
| **Dependency** | R-SEA-02, R-SEA-03, R-SEA-04, time manager |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: automation trigger `08:30`; dopo questo orario, decisione fissata per giornata |

### R-SEA-02
| Campo | Valore |
|---|---|
| **ID** | R-SEA-02 |
| **Full requirement** | Stagione invernale configurabile (default: 25/10 – 28/02) |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.6, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna configurazione stagione; logica stagionale assente |
| **Target module** | `seasonal_strategy::isWinterSeason()` (M4) |
| **Planned unit test** | `test_seasonal_strategy::test_winter_season_dates` |
| **Planned integration test** | Scenario M4 #8: 15/11 (inverno) → valutazione attiva; 15/05 (estate) → valutazione disabilitata o diversa |
| **Dependency** | R-SEA-01 |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `winter_start: "10-25"`, `winter_end: "02-28"`; configurable; fuori inverno strategia può disabilitarsi |

### R-SEA-03
| Campo | Valore |
|---|---|
| **ID** | R-SEA-03 |
| **Full requirement** | Attivare priorità carica se surplus oggi < deficit al 100% × 1,1 **e** domani non favorevole (margine 1,1) |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.6, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna logica priorità carica; controller non valuta giorni consecutivi |
| **Target module** | `seasonal_strategy::shouldActivatePriority()` (M4) |
| **Planned unit test** | `test_seasonal_strategy::test_priority_activation_logic` |
| **Planned integration test** | Scenario M4 #8: surplus oggi 5 kWh, deficit 100% = 10.24 kWh → 5 < 10.24×1.1 **e** domani sfavorevole → priorità ON |
| **Dependency** | R-PV-01 (forecast oggi/domani), R-SEA-04 (domani favorevole), R-NRG-01 (capacity) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: equivalente ChgFst di Afore (priorità batteria su export); margine 1,1 evita attivazioni eccessive |

### R-SEA-04
| Campo | Valore |
|---|---|
| **ID** | R-SEA-04 |
| **Full requirement** | Domani favorevole se surplus domani ≥ 7 kWh e SOC ≥ SOC minimo per domani (SOC min 30%) |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.6, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna valutazione giorno successivo |
| **Target module** | `seasonal_strategy::isTomorrowFavorable()` (M4) |
| **Planned unit test** | `test_seasonal_strategy::test_tomorrow_favorable_condition` |
| **Planned integration test** | Scenario M4 #8: forecast domani 8 kWh, SOC 50% → domani favorevole → priorità OFF |
| **Dependency** | R-PV-01 (forecast domani), R-SEA-03 |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `minimum_energy_tomorrow: 7`, `minimum_soc_for_tomorrow: 30`; logica: se domani buono, non serve priorità oggi |

### R-SEA-05
| Campo | Valore |
|---|---|
| **ID** | R-SEA-05 |
| **Full requirement** | Nella V4 shadow la decisione "ChgFst" è registrata come stato strategico, non come cambio modalità inverter |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.6 |
| **Existing file/function** | **ABSENT** — stato "priority charge" non presente in CP4 |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | CP4 non ha stati strategici (solo operating mode generico) |
| **Target module** | `seasonal_strategy::priorityChargeActive` (stato, M4) |
| **Planned unit test** | `test_seasonal_strategy::test_priority_state_recorded` |
| **Planned integration test** | Scenario M4 #8: stato priorità registrato in log/MQTT ma non invia comando Afore (shadow mode) |
| **Dependency** | R-SEA-03 |
| **Milestone** | **M4** |
| **Notes** | In shadow mode: decisione calcolata e pubblicata, non attuata; confronto shadow vs stock Afore |

---

## 7. R-CAL — Calibrazione BMS

### R-CAL-01
| Campo | Valore |
|---|---|
| **ID** | R-CAL-01 |
| **Full requirement** | Considerare calibrata la batteria con SOC pieno stabile (100% per 5 min) |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.7, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **CONFLICTING** — CONF-03: calibrazione 100% vs CP4 maxSOC 95% |
| **Gap** | Nessun tracking calibrazione; controller ignora stato calibrazione |
| **Target module** | `calibration_tracker::detectCalibration()` (M4) |
| **Planned unit test** | `test_calibration_tracker::test_calibration_detection` |
| **Planned integration test** | Scenario M4 #12: SOC 100% stabile per 5 min → calibrazione confermata → evento R-DIA-03 |
| **Dependency** | Battery SOC, timestamp |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: automation trigger SOC=100% per 5min → notifica; calibrazione BMS importante per accuracy SOC |

### R-CAL-02
| Campo | Valore |
|---|---|
| **ID** | R-CAL-02 |
| **Full requirement** | Dopo calibrazione, ritorno alla strategia ordinaria (equivalente ritorno SelfUse) |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.7 |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna logica post-calibrazione |
| **Target module** | `calibration_tracker::resetAfterCalibration()` (M4) |
| **Planned unit test** | `test_calibration_tracker::test_return_to_normal_after_cal` |
| **Planned integration test** | Scenario M4 #12: dopo calibrazione 100% × 5min → strategia torna a target normale |
| **Dependency** | R-CAL-01 |
| **Milestone** | **M4** |
| **Notes** | Calibrazione può richiedere target temporaneo 100% per alcuni giorni; dopo calibrazione conferma si torna a normale |

### R-CAL-03
| Campo | Valore |
|---|---|
| **ID** | R-CAL-03 |
| **Full requirement** | Il profilo longevità (es. SOC max 95%) è opzionale e non deve impedire la calibrazione al 100% |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.7 |
| **Existing file/function** | `shadow_ems_types.h::BatteryConstraints::maxSOC = 95.0` — conflitto potenziale |
| **Status** | **PARTIAL** |
| **Compatibility** | **CONFLICTING** — CONF-03: calibrazione 100% override vs CP4 maxSOC 95% |
| **Gap** | CP4 ha maxSOC 95% hard-coded; nessuna logica per override temporaneo calibrazione |
| **Target module** | `calibration_tracker::overrideMaxSOC()` o config (M4) |
| **Planned unit test** | `test_calibration_tracker::test_calibration_overrides_longevity` |
| **Planned integration test** | Scenario M4 #12: profilo longevità 95% ma calibrazione richiede 100% → override temporaneo |
| **Dependency** | R-CAL-01, R-CAL-02 |
| **Milestone** | **M4** |
| **Notes** | CP4 ha maxSOC 95% (longevità); M4 deve gestire override quando calibrazione necessaria; configurabile |

---

## 8. R-CMD — Comandi e stabilità

### R-CMD-01
| Campo | Valore |
|---|---|
| **ID** | R-CMD-01 |
| **Full requirement** | Setpoint espresso in W (carica negativa secondo convenzione Afore, da confermare) |
| **Source type** | APPROVED_PLAN, AFORE_DOCUMENTATION |
| **Source reference** | Piano V4 §2.8, doc Afore External EMS |
| **Existing file/function** | `shadow_ems_controller.cpp`: setpoint in W (canonical: positivo=carica); convenzione Afore inversa da confermare |
| **Status** | **PARTIAL** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | CP4 usa W canonici (positivo=carica); convenzione Afore (negativo=carica) da validare empiricamente (M8) e flip da applicare prima dell'invio comando (fase attiva post-G4) |
| **Target module** | `shadow_ems_controller` (già W); `afore_command_sender` (flip sign, fase attiva) |
| **Planned unit test** | Convenzione canonica interna: `positive = charge`; conversione wire isolata e non attiva in Phase 0 |
| **Planned integration test** | **M8** valida solo il segno della telemetria `0x07D8`. **Post-G4**: prova attiva presidiata per il segno del comando External EMS `0x0A09–0x0A0A` |
| **Dependency** | R-NRG-10 (convenzione power), validazione M8 |
| **Milestone** | M4 per modello canonico; M8 per telemetria; post-G4 per comando External EMS |
| **Notes** | Nessuna scrittura External EMS è consentita in M8. La semantica del comando su `0x0A09–0x0A0A` resta non verificata fino al piano attivo post-G4. |

### R-CMD-02
| Campo | Valore |
|---|---|
| **ID** | R-CMD-02 |
| **Full requirement** | Rampa massima ordinaria in salita/discesa (traduzione di 5 A / 8 A per ciclo) ≈ 256 W / 410 W per ciclo |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.8, Blueprint V3.3 HA (conversione A→W a 51.2V) |
| **Existing file/function** | `shadow_ems_controller.cpp::applySmoothing()`: `maxPowerRampWPerS = 500.0` — valore generico per secondo, non per ciclo |
| **Status** | **PARTIAL** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | CP4 ha rampa 500W/s (generico); requisito specifica 256/410 W per ciclo (ciclo durata da definire, presumibilmente 1s → valori simili ma non identici) |
| **Target module** | `setpoint_shaper::applyRampLimits()` (M4) |
| **Planned unit test** | `test_setpoint_shaper::test_ramp_limits_per_cycle` |
| **Planned integration test** | Scenario M4 #1: setpoint da 1000W a 2000W → incremento max 256W/ciclo (salita) |
| **Dependency** | R-CMD-03 (isteresi), durata ciclo controllo |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `max_ramp_up: 5`, `max_ramp_down: 8` [A]; conversione con R-NRG-02 (51.2V); ciclo controllo da definire (presumibilmente 1s) |

### R-CMD-03
| Campo | Valore |
|---|---|
| **ID** | R-CMD-03 |
| **Full requirement** | Isteresi minima per variazione setpoint (traduzione di 2 A) ≈ 100 W |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.8, Blueprint V3.3 HA |
| **Existing file/function** | `shadow_ems_controller.cpp`: hysteresisW = 30W (usato per grid, non per setpoint) |
| **Status** | **PARTIAL** |
| **Compatibility** | **CONFLICTING** — CONF-02: CP4 hysteresis 30W grid-decision vs R-CMD-03 ~100W setpoint hysteresis approvato |
| **Gap** | CP4 ha isteresi 30W per decisione grid-driven; requisito richiede isteresi 100W per variazione setpoint |
| **Target module** | `setpoint_shaper::applyHysteresis()` (M4) |
| **Planned unit test** | `test_setpoint_shaper::test_setpoint_hysteresis` |
| **Planned integration test** | Scenario M4: setpoint richiesto varia di 80W → nessun cambio comando (sotto isteresi 100W) |
| **Dependency** | R-CMD-02 |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `hysteresis_threshold: 2` [A]; conversione ≈ 100W; evita oscillazioni comando |

### R-CMD-04
| Campo | Valore |
|---|---|
| **ID** | R-CMD-04 |
| **Full requirement** | In stato critico nessuna rampa lenta |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.8, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** — CP4 applica sempre smoothing, non ha stato critico |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessuna logica di bypass rampa in criticità |
| **Target module** | `setpoint_shaper::bypassRampIfCritical()` (M4) |
| **Planned unit test** | `test_setpoint_shaper::test_no_ramp_when_critical` |
| **Planned integration test** | Scenario M4 #6: stato critico (R-NRG-09) → setpoint cambia immediatamente senza rampa |
| **Dependency** | R-NRG-09 (stato critico) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `if is_critical: bypass ramp`; urgenza prioritaria su smoothing |

### R-CMD-05
| Campo | Valore |
|---|---|
| **ID** | R-CMD-05 |
| **Full requirement** | Limiti min/max carica configurabili (equivalente 5–50 A) ≈ 256–2560 W, da validare col BMS |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.8, Blueprint V3.3 HA, limiti BMS |
| **Existing file/function** | `shadow_ems_types.h::BatteryConstraints`: maxChargePowerW = 5000W, maxDischargePowerW = 5000W — valori nominali inverter, non limiti utente configurabili |
| **Status** | **PARTIAL** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | CP4 ha limiti fissi 5000W (nominale inverter); requisito richiede limiti configurabili min/max per carica (5–50A → 256–2560W) |
| **Target module** | `BatteryConstraints` esteso con minChargePowerW, maxChargePowerW configurabili (M4) |
| **Planned unit test** | `test_energy_model::test_configurable_charge_limits` |
| **Planned integration test** | Scenario M4: limite max carica configurato 2000W → setpoint clampato a 2000W anche se surplus maggiore |
| **Dependency** | R-CMD-01, limiti BMS validati (M7/M8) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: `min_charge_current: 5`, `max_charge_current: 50` [A]; limiti proteggono BMS; da validare empiricamente (M8) |

### R-CMD-06
| Campo | Valore |
|---|---|
| **ID** | R-CMD-06 |
| **Full requirement** | Nella Phase 0 nessuna scarica comandata nel setpoint primario (`allow_discharge = false`) |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.8 |
| **Existing file/function** | **ABSENT** — CP4 raccomanda scarica (`Discharging` mode, power negativo) ma non invia comandi (shadow) |
| **Status** | **PARTIAL** |
| **Compatibility** | **CONFLICTING** — CONFLICTING con CP4 Discharging mode (Phase 0 no discharge) |
| **Gap** | CP4 in shadow mode non invia comandi quindi conforme de facto; ma logica "allow_discharge" non esplicita → M4 deve implementare flag configurabile per fase attiva |
| **Target module** | `setpoint_shaper::allowDischarge` flag (M4) |
| **Planned unit test** | `test_setpoint_shaper::test_no_discharge_when_disabled` |
| **Planned integration test** | Scenario M4 con `allow_discharge=false`: setpoint scarica clampato a 0W |
| **Dependency** | Nessuna |
| **Milestone** | **M4** |
| **Notes** | Shadow mode CP4 già non invia comandi; M4 deve aggiungere flag esplicito per fase attiva futura; protezione: evita scarica fino a validazione completa |

### R-CMD-07
| Campo | Valore |
|---|---|
| **ID** | R-CMD-07 |
| **Full requirement** | Heartbeat futuro External EMS 30–60 s, timeout Afore 2 min → 0 W (comportamento documentato) |
| **Source type** | APPROVED_PLAN, AFORE_DOCUMENTATION |
| **Source reference** | Piano V4 §2.8, doc Afore External EMS |
| **Existing file/function** | **ABSENT** — solo progettazione, non implementare in Phase 0 |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessun heartbeat (shadow mode); requisito per fase attiva futura |
| **Target module** | `afore_command_sender::heartbeat` (fase attiva post-G4) |
| **Planned unit test** | `test_command_sender::test_heartbeat_timeout` (fase attiva) |
| **Planned integration test** | Fase attiva: heartbeat ogni 30s; se timeout > 2min → Afore power 0W |
| **Dependency** | Fase attiva External EMS (post-G4) |
| **Milestone** | **Fase attiva** (post-G4, non M4) |
| **Notes** | **DOCUMENTED**: Doc Afore specifica timeout 2min → power 0W. **NOT VERIFIED**: Transizione automatica a SelfUse non menzionata in doc Afore (vedi R-CMD-08). Heartbeat raccomandato 30–60s; progettazione M4, implementazione fase attiva |

### R-CMD-08
| Campo | Valore |
|---|---|
| **ID** | R-CMD-08 |
| **Full requirement** | Ritorno controllato a SelfUse dopo timeout >2min produce power/setpoint 0W (comportamento documentato); ritorno automatico SelfUse NON documentato in spec Afore |
| **Source type** | APPROVED_PLAN, AFORE_DOCUMENTATION |
| **Source reference** | Piano V4 §2.8, doc Afore External EMS |
| **Existing file/function** | **ABSENT** — solo progettazione, non implementare in Phase 0 |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessun ritorno controllato (shadow mode); requisito design separato per fase attiva futura |
| **Target module** | `afore_command_sender::returnToSelfUse()` (fase attiva post-G4) |
| **Planned unit test** | `test_command_sender::test_return_to_selfuse` (fase attiva) |
| **Planned integration test** | Fase attiva: perdita MQTT/HA → EMS invia setpoint 0W; M8 NON deve testare write EMS-mode attivo |
| **Dependency** | R-CMD-07, fase attiva External EMS (post-G4) |
| **Milestone** | **Fase attiva** (post-G4, non M4) |
| **Notes** | **DOCUMENTED**: Doc Afore timeout >2min → power 0W. **NOT DOCUMENTED**: Ritorno automatico SelfUse non menzionato in doc Afore (conferma scritta necessaria). **DESIGN**: Inverter deve essere assunto rimanere in External EMS fino a cambio esplicito (salvo conferma scritta Afore diversa). **POST-G4**: Ritorno controllato SelfUse appartiene a piano active-control post-G4. **M8**: NON deve testare write EMS-mode attivo. |

---

## 9. R-DIA — Notifiche e diagnostica

### R-DIA-01
| Campo | Valore |
|---|---|
| **ID** | R-DIA-01 |
| **Full requirement** | Riepilogo serale (20:13): modalità, SOC, previsione domani, stato nebbia |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.9, Blueprint V3.3 HA (automation time trigger) |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessun evento temporizzato; nessun riepilogo |
| **Target module** | `decision_log::publishEveningSummary()` (M4) |
| **Planned unit test** | `test_decision_log::test_evening_summary_content` |
| **Planned integration test** | Scenario M5 replay: alle 20:13 evento summary pubblicato su MQTT/log |
| **Dependency** | Time manager, forecast domani, stato EMS |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: automation trigger `20:13` con template summary; utile per diagnostica giornaliera |

### R-DIA-02
| Campo | Valore |
|---|---|
| **ID** | R-DIA-02 |
| **Full requirement** | Evento all'attivazione della priorità di carica |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.9, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessun evento su transizione priorità |
| **Target module** | `decision_log::logPriorityActivation()` (M4) |
| **Planned unit test** | `test_decision_log::test_priority_activation_event` |
| **Planned integration test** | Scenario M4 #8: priorità attivata alle 08:30 → evento pubblicato |
| **Dependency** | R-SEA-03 (attivazione priorità) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: notifica HA quando `priority_charge` cambia a ON; utile per capire decisioni strategiche |

### R-DIA-03
| Campo | Valore |
|---|---|
| **ID** | R-DIA-03 |
| **Full requirement** | Evento a calibrazione completata |
| **Source type** | APPROVED_PLAN, BLUEPRINT_V3_3 |
| **Source reference** | Piano V4 §2.9, Blueprint V3.3 HA |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessun evento calibrazione |
| **Target module** | `decision_log::logCalibrationComplete()` (M4) |
| **Planned unit test** | `test_decision_log::test_calibration_event` |
| **Planned integration test** | Scenario M4 #12: calibrazione confermata (SOC 100% × 5min) → evento pubblicato |
| **Dependency** | R-CAL-01 (rilevamento calibrazione) |
| **Milestone** | **M4** |
| **Notes** | Blueprint V3.3: notifica HA; calibrazione importante per BMS → evento notevole |

### R-DIA-04
| Campo | Valore |
|---|---|
| **ID** | R-DIA-04 |
| **Full requirement** | Evento previsione Solcast non valida |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.9 |
| **Existing file/function** | **ABSENT** |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Nessun evento scadenza Solcast |
| **Target module** | `decision_log::logForecastInvalid()` (M4) |
| **Planned unit test** | `test_decision_log::test_forecast_invalid_event` |
| **Planned integration test** | Scenario M4 #7: Solcast scaduto > 14h → evento pubblicato |
| **Dependency** | R-PV-02 (età forecast) |
| **Milestone** | **M4** |
| **Notes** | Allerta degrado previsione; utente può verificare integrazione Solcast |

### R-DIA-05
| Campo | Valore |
|---|---|
| **ID** | R-DIA-05 |
| **Full requirement** | Ogni decisione con `decision`, `reason`, `confidence`, input usati e loro qualità |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.9 |
| **Existing file/function** | **PARTIAL** — CP4 ha `ControlRecommendation` con mode, reason, confidenceScore ma non registra input usati e qualità |
| **Status** | **PARTIAL** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | CP4 ha decision/reason/confidence ma non log strutturato con input e qualità |
| **Target module** | `decision_log::logDecisionWithContext()` (M4) |
| **Planned unit test** | `test_decision_log::test_decision_log_structure` |
| **Planned integration test** | Scenario M4/M5: ogni decisione registrata con input (SOC, FV, grid, forecast), qualità (Good/Degraded/Stale), reason, confidence |
| **Dependency** | CP4 `ControlRecommendation`, `TelemetrySample` qualità |
| **Milestone** | **M4** |
| **Notes** | CP4 authored parzialmente; M4 estende con log strutturato JSON completo per replay M5 |

---

## 10. R-SAF — Qualità dati e sicurezza

### R-SAF-01
| Campo | Valore |
|---|---|
| **ID** | R-SAF-01 |
| **Full requirement** | Un dato mancante non è mai zero (`unknown ≠ 0`) |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.10, AGENTS.md:18-20 |
| **Existing file/function** | `telemetry_quality.h::TelemetrySample`: se Invalid, valore non dovrebbe essere usato; ma inizializzato a 0.0 |
| **Status** | **PARTIAL** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | CP4 TelemetrySample ha `value = 0.0` di default e quality Invalid; controller controlla actionable() ma valore 0 tecnicamente presente → ambiguo |
| **Target module** | `telemetry_quality` + controller (M4) |
| **Planned unit test** | `test_telemetry_quality::test_invalid_not_zero_semantic` |
| **Planned integration test** | Scenario M4 #10: telemetria Invalid → controller non usa valore, non assume 0 |
| **Dependency** | R-SAF-02, R-SAF-03 |
| **Milestone** | **M4** |
| **Notes** | AGENTS.md regola fondamentale; CP4 usa actionable() per filtrare ma semantica "valore non esiste" vs "valore 0" da chiarire; std::optional da considerare |

### R-SAF-02
| Campo | Valore |
|---|---|
| **ID** | R-SAF-02 |
| **Full requirement** | Stati qualità: VALID, STALE, INVALID, OUT_OF_RANGE, COMM_ERROR, UNAVAILABLE, UNVERIFIED |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.10 |
| **Existing file/function** | `telemetry_quality.h::Quality`: Good, Degraded, Stale, Invalid (4 stati) — subset del requisito |
| **Status** | **PARTIAL** |
| **Compatibility** | **CONFLICTING** — CONFLICTING (risolto): Degraded actionable vs documentazione |
| **Gap** | CP4 ha 4 stati (Good/Degraded/Stale/Invalid); requisito richiede 7 stati distinti; mappatura: Good→VALID, Stale→STALE, Invalid→{INVALID, OUT_OF_RANGE, COMM_ERROR, UNAVAILABLE}, Degraded→UNVERIFIED? |
| **Target module** | `telemetry_quality::Quality` enum esteso (M4) |
| **Planned unit test** | `test_telemetry_quality::test_all_quality_states` |
| **Planned integration test** | Scenario M4: distinguere COMM_ERROR (timeout Modbus) vs OUT_OF_RANGE (valore fuori limiti) vs UNAVAILABLE (measurement non supportato) |
| **Dependency** | R-SAF-01 |
| **Milestone** | **M4** |
| **Notes** | CP4 authored ha modello semplificato (4 stati); M4 estende per granularità diagnostica; UNVERIFIED già parzialmente coperto da `verified` flag |

### R-SAF-03
| Campo | Valore |
|---|---|
| **ID** | R-SAF-03 |
| **Full requirement** | SOC/potenza batteria/FV non validi → decisione `DATA_INVALID`, setpoint nullo |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.10 |
| **Existing file/function** | `shadow_ems_controller.cpp::checkTelemetryQuality()` + `transitionToFailSafe()`: se critical measurements non actionable → SafeIdle, power 0 |
| **Status** | **IMPLEMENTED_NOT_TESTED** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | CP4 authored implementa fail-safe su Stale/Invalid; reason `TelemetryInvalid` presente; da confermare esecuzione test M3 |
| **Target module** | `shadow_ems_controller::transitionToFailSafe()` (CP4 authored) |
| **Planned unit test** | `test_shadow_ems::test_scenario_d_stale_telemetry_failsafe` (CP4 authored, pending M3) |
| **Planned integration test** | Scenario M4 #10: telemetria Invalid → decisione DATA_INVALID, setpoint 0W |
| **Dependency** | R-SAF-02, CP4 authored |
| **Milestone** | **M3** (verifica test CP4) + **M4** (estensione reason code) |
| **Notes** | CP4 authored copre requisito; da verificare esecuzione test M3; M4 può estendere reason code `DATA_INVALID` vs `TelemetryInvalid` |

### R-SAF-04
| Campo | Valore |
|---|---|
| **ID** | R-SAF-04 |
| **Full requirement** | Override da HA non possono superare limiti di sicurezza |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.10 |
| **Existing file/function** | **ABSENT** — nessun ingresso override da HA in CP4 |
| **Status** | **ABSENT** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | Override non implementati; quando implementati (M10) devono essere clampati |
| **Target module** | `ha_input::loadOverrides()` + `setpoint_shaper::clampToSafety()` (M4/M10) |
| **Planned unit test** | `test_setpoint_shaper::test_override_clamped_to_limits` |
| **Planned integration test** | Scenario M4: override HA target SOC 110% → clampato a maxSOC (95% o 100%); override carica 6000W → clampato a 5000W |
| **Dependency** | R-CMD-05 (limiti configurabili), integrazione HA (M10) |
| **Milestone** | **M4** (logica clamp) + **M10** (ingresso override) |
| **Notes** | Blueprint V3.3 ha override `input_number.*`; EMS deve validare e clamping prima di usare |

### R-SAF-05
| Campo | Valore |
|---|---|
| **ID** | R-SAF-05 |
| **Full requirement** | Gerarchia: sicurezza > validità dati > limiti BMS/inverter > anti-import > override > target > ottimizzazione |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.10 |
| **Existing file/function** | **PARTIAL** — CP4 ha: validità dati (checkTelemetryQuality) → fail-safe; limiti (BatteryConstraints); ma non gerarchia completa override/target/ottimizzazione |
| **Status** | **PARTIAL** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | CP4 ha sicurezza base (fail-safe) e limiti; manca gerarchia esplicita override > target > ottimizzazione |
| **Target module** | `target_planner` + `setpoint_shaper` orchestrazione (M4) |
| **Planned unit test** | `test_target_planner::test_priority_hierarchy` |
| **Planned integration test** | Scenario M4: override attivo → ignora ottimizzazione; target critico → ignora ottimizzazione; sicurezza → sempre prioritaria |
| **Dependency** | Tutti i moduli M4 |
| **Milestone** | **M4** |
| **Notes** | Gerarchia fondamentale per decisioni corrette; M4 implementa orchestrazione esplicita |

### R-SAF-06
| Campo | Valore |
|---|---|
| **ID** | R-SAF-06 |
| **Full requirement** | Il funzionamento senza HA/MQTT deve continuare con dati locali |
| **Source type** | APPROVED_PLAN |
| **Source reference** | Piano V4 §2.10 |
| **Existing file/function** | **PARTIAL** — CP4 usa solo dati locali (battery, grid, pv da Modbus); nessuna dipendenza HA; ma logica target-aware richiede forecast → degradata senza HA |
| **Status** | **PARTIAL** |
| **Compatibility** | **ALIGNED** — Nessun conflitto noto |
| **Gap** | CP4 funziona senza HA (self-consumption generico); M4 target-aware degrada senza Solcast (R-PV-05, R-PV-06) |
| **Target module** | Tutti i moduli M4 con fallback |
| **Planned unit test** | `test_target_planner::test_operation_without_ha` |
| **Planned integration test** | Scenario M4 #7: perdita MQTT/HA → EMS continua con dati locali, modalità conservativa |
| **Dependency** | R-PV-05 (conservativa senza forecast), R-PV-06 |
| **Milestone** | **M4** |
| **Notes** | Autonomia essenziale; HA fornisce Solcast/nebbia/override ma non è obbligatorio; EMS degrada ma funziona |

---

## 11. Gap Summary

### 11.1 Implemented but not tested (authored CP1-CP4, execution pending M3)
| Requisito | Modulo/Funzione authored | Nota |
|---|---|---|
| R-NRG-01 | `BatteryConstraints::ratedCapacityKWh` | Valore presente, uso limitato a constraint SOC; da estendere calcoli energetici M4 |
| R-NRG-10 | `afore_telemetry_engine::decodeBatteryBlock()` scale=-1 | Critico: convenzione sign flip authored, mai validata su bus reale; conferma pendente M8 |
| R-SAF-03 | `shadow_ems_controller::checkTelemetryQuality()` + `transitionToFailSafe()` | Fail-safe Stale/Invalid authored; test authored pending M3 |
| R-DIA-05 (partial) | `ControlRecommendation` con mode/reason/confidence | Decision base presente; log strutturato con input/qualità da completare M4 |

### 11.2 Partially implemented (codice parziale o generico authored CP4)
| Requisito | Gap | Modulo target M4 |
|---|---|---|
| R-CMD-01 | Setpoint già in W ma convenzione canonica interna definita; telemetria validata in M8, segno comando External EMS validato solo post-G4 | `afore_command_sender` (fase attiva) |
| R-CMD-02 | Rampa 500W/s authored generica; requisito specifica 256/410 W/ciclo | `setpoint_shaper::applyRampLimits()` |
| R-CMD-03 | Isteresi 30W per grid-driven; requisito richiede 100W per setpoint | `setpoint_shaper::applyHysteresis()` |
| R-CMD-05 | Limiti fissi 5000W (nominale); requisito richiede configurabili 256–2560W | `BatteryConstraints` esteso |
| R-CMD-06 | Shadow mode non invia comandi (conforme de facto); flag `allow_discharge` esplicito assente | `setpoint_shaper::allowDischarge` |
| R-CAL-03 | maxSOC 95% hard-coded; nessun override temporaneo calibrazione | `calibration_tracker::overrideMaxSOC()` |
| R-PV-06 | CP4 usa solo dati locali (no Solcast) ma è generico (no target); degraded target-aware assente | `target_planner::degradedMode()` |
| R-SAF-01 | TelemetrySample value=0.0 default con Invalid; semantica "non esiste" vs "valore 0" ambigua | `telemetry_quality` + std::optional |
| R-SAF-02 | Quality enum 4 stati (Good/Degraded/Stale/Invalid); requisito richiede 7 (VALID/STALE/INVALID/OUT_OF_RANGE/COMM_ERROR/UNAVAILABLE/UNVERIFIED) | `Quality` esteso |
| R-SAF-05 | Fail-safe e limiti presenti; gerarchia esplicita override > target > ottimizzazione assente | `target_planner` + `setpoint_shaper` |
| R-SAF-06 | CP4 autonomo (no HA) ma generico; target-aware degradato senza Solcast da implementare | Fallback tutti moduli M4 |

### 11.3 Entirely absent (requisiti non coperti da CP4, implementazione completa M4)
| Gruppo | Requisiti ABSENT | Target M4 |
|---|---|---|
| **R-TGT** | R-TGT-01, 02, 03, 04, 05, 06, 07 (tutti) | `target_planner` |
| **R-NRG** | R-NRG-02 (opzionale), 03, 04, 05, 06, 07, 08, 09 | `energy_model` |
| **R-PV** | R-PV-01, 02, 03, 04, 05 | `pv_forecast` |
| **R-FOG** | R-FOG-01, 02, 03, 04, 05 (N/A) | `fog_detector` |
| **R-LOAD** | R-LOAD-01, 02, 03, 04 (post-M6) | `load_profile` |
| **R-SEA** | R-SEA-01, 02, 03, 04, 05 | `seasonal_strategy` |
| **R-CAL** | R-CAL-01, 02 | `calibration_tracker` |
| **R-CMD** | R-CMD-04, 07 (fase attiva), 08 (fase attiva) | `setpoint_shaper`, `afore_command_sender` (fase attiva) |
| **R-DIA** | R-DIA-01, 02, 03, 04 | `decision_log` |
| **R-SAF** | R-SAF-04 (+ M10) | `ha_input` + `setpoint_shaper` |

**Totale requisiti ABSENT**: 43 su 59

### 11.4 Conflicting behavior (authored vs requisito)
| Conflitto | Authored CP4 | Requisito | Risoluzione M4 |
|---|---|---|---|
| Fail-safe su `Degraded` | CP4: `Degraded` resta actionable (confidence 0.7) | Doc CP4 precedente diceva "Degraded → SafeIdle" (conflitto interno doc) | Comportamento CP4 authored corretto: Degraded actionable, solo Stale/Invalid → SafeIdle; doc corretto in M0 |
| Isteresi grid vs setpoint | CP4: 30W per decisione grid-driven | R-CMD-03: 100W per variazione setpoint | M4 separa: isteresi grid 30W (decisione), isteresi setpoint 100W (comando) |
| maxSOC longevità vs calibrazione | CP4: maxSOC 95% hard-coded | R-CAL-03: calibrazione override temporaneo 100% | M4 aggiunge override temporaneo |

### 11.5 No longer needed (requisiti coperti da scelte architetturali)
| Requisito | Motivo non necessario |
|---|---|
| R-NRG-02 (tensione nominale 51.2V) | Protocollo Afore fornisce già potenza W e corrente A separatamente; conversione non necessaria nel codice EMS (opzionale per diagnostica) |
| R-FOG-05 (no orario rigido dissoluzione) | Requisito negativo già soddisfatto: nessuna costante oraria hard-coded |

### 11.6 Blocked by missing data (dipendenze esterne M7/M8/M10)
| Requisito | Dipendenza | Milestone validazione |
|---|---|---|
| R-NRG-10 | Conferma convenzione segno batteria power su bus reale | **M8** |
| R-CMD-01 | Telemetria: validazione segno `0x07D8` in M8; comando External EMS `0x0A09–0x0A0A` solo post-G4 | **M8 / post-G4** |
| R-CMD-05 | Validazione limiti BMS reali (5–50A) | **M7/M8** |
| R-PV-01, 02, 03, 04 | Integrazione Solcast via HA (MQTT) | **M10** |
| R-FOG-01, 02, 03 | Sensori meteo da HA (temperatura, punto rugiada, radiazione, nebbia stabilizzata) | **M10** |
| R-SAF-04 | Override HA (input_number.*) | **M10** |

---

## 12. Mappatura inversa completa delle funzioni CP4

La tabella deriva dalla lettura dei corpi funzione in `src/ems/shadow_ems_controller.cpp` e `src/ems/shadow_ems_types.cpp`; non attribuisce copertura in base ai nomi.

| Funzione CP4 esistente | Comportamento osservato staticamente | Requisiti mappati | Stato evidenza |
|---|---|---|---|
| `ShadowEMSController::ShadowEMSController()` | Copia constraint e inizializza stato | R-NRG-01, R-CMD-05, R-SAF-05 | PARTIAL |
| `ShadowEMSController::update()` | Orchestra quality gate, raccomandazione e divergenza; nessun write path | R-DIA-05, R-SAF-03, R-SAF-06 | PARTIAL |
| `ShadowEMSController::checkTelemetryQuality()` | Richiede SOC, battery/grid/PV power actionable; confidence minima | R-SAF-01, R-SAF-02, R-SAF-03, R-SAF-05 | PARTIAL |
| `ShadowEMSController::transitionToFailSafe()` | Produce `SafeIdle`, zero power, reason e contatore fail-safe | **R-SAF-03, R-SAF-05** | IMPLEMENTED_NOT_TESTED per il fail-safe base |
| `ShadowEMSController::computeRecommendation()` | Legge quattro misure e invoca strategia/constraint/smoothing | **R-CMD-01, R-CMD-02, R-SAF-05** | PARTIAL |
| `ShadowEMSController::applySelfConsumptionStrategy()` | Regola preliminare grid-driven di carica/scarica con SOC min/max | **R-TGT-01 (partial), R-NRG-01, R-CMD-06, R-SAF-05** | PARTIAL; non target-aware |
| `ShadowEMSController::applyConstraints()` | Clamp SOC e potenza ai limiti nominali | **R-CMD-05, R-SAF-04, R-SAF-05** | PARTIAL |
| `ShadowEMSController::applySmoothing()` | Rampa temporale generica 500 W/s | **R-CMD-02 (partial), R-CMD-04** | PARTIAL; valore in conflitto col requisito; manca bypass critico |
| `ShadowEMSController::updateDivergence()` | Calcola divergenza istantanea, cumulativa ed eventi | **IMPLEMENTATION_EXTENSION** (M11 shadow-vs-Afore KPI; no approved requirement coverage) | PARTIAL; non mappa R-DIA-04 (forecast invalid event) |
| `operatingModeToString()` | Serializza modalità CP4 | **R-DIA-05** (confidence/status reporting) | PARTIAL; non R-DIA-03 (calibration event) |
| `decisionReasonToString()` | Serializza reason CP4, set incompleto | **R-DIA-05** (decision log structure) | PARTIAL |
| `TelemetrySample::actionable()` | Accetta Good e Degraded; rifiuta Stale/Invalid | R-SAF-01, R-SAF-02, R-SAF-03 | PARTIAL; conflitto documentale su Degraded |
| `qualityConfidence()` | Associa confidence a quattro qualità CP3 | R-DIA-05, R-SAF-02 | PARTIAL |

## 13. Riepilogo Numerico

| Categoria | Conteggio | % su 59 requisiti |
|---|---|---|
| **ABSENT** | 43 | 73% |
| **PARTIAL** | 12 | 20% |
| **IMPLEMENTED_NOT_TESTED** | 4 | 7% |
| **VERIFIED** | 0 | 0% |
| **Requisiti negativi/opzionali** | 2 (R-NRG-02, R-FOG-05) | Status ABSENT con note |

**Conclusione**: il controller CP4 authored è un **preliminary self-consumption controller**, non un Smart Energy Manager target-aware. Copre 16 requisiti (4 implemented + 12 partial) su 59 (27%), principalmente infrastruttira base (quality, constraints, fail-safe). I **43 requisiti ABSENT** (73%) richiedono implementazione completa in **M4**, inclusi tutti i moduli core: `target_planner`, `energy_model`, `pv_forecast`, `fog_detector`, `load_profile`, `seasonal_strategy`, `calibration_tracker`, `setpoint_shaper` completo, `decision_log`.

---

**Fine documento**  
**Prossimo step**: Aggiornare `avanzamentoPiano.md` con sezione M1, commit `docs(m1): apply requirement traceability corrections`.

---

## 14. Conflitti e Compatibility (sezione aggiunta M1)

Questa sezione documenta i conflitti identificati tra requisiti, comportamenti authored CP4 e vincoli architetturali.

### 14.1 Conflitti documentati

| ID Conflitto | Requisiti coinvolti | Descrizione | Risoluzione |
|---|---|---|---|
| **CONF-01** | R-SAF-02 (Degraded actionable) vs R-SAF-03 (fail-safe) | CP4 authored: Degraded (confidence 0.7) resta actionable. Doc CP4 precedente affermava "Degraded → SafeIdle" (conflitto documentale interno). | **RISOLTO**: Comportamento CP4 authored corretto; Degraded actionable, solo Stale/Invalid → SafeIdle. Documentazione corretta in M0. |
| **CONF-02** | R-CMD-03 (isteresi 100W setpoint) vs CP4 hysteresis 30W grid-decision | CP4 authored usa isteresi 30W per decisione grid-driven. R-CMD-03 richiede isteresi ~100W approvato per variazione setpoint. | **RISOLUZIONE M4**: Separare isteresi grid-decision 30W (CP4) da isteresi setpoint 100W (R-CMD-03). Due soglie diverse per scopi diversi. NON riferire R-CMD-06 per hysteresis. |
| **CONF-03** | R-CAL-01 e R-CAL-03 (calibrazione 100%) vs CP4 BatteryConstraints::maxSOC 95% | CP4 authored: `BatteryConstraints::maxSOC = 95%` (longevità). R-CAL-01 e R-CAL-03 richiedono calibrazione 100% per 5 min. | **RISOLUZIONE M4**: Implementare override temporaneo `calibration_tracker::overrideMaxSOC()`. Calibrazione prioritaria su longevità. NON riferire R-CMD-03 per maxSOC. |
| **CONF-04** | R-CMD-06 (Phase 0 no discharge) vs CP4 `Discharging` mode | CP4 raccomanda mode `Discharging` con power negativo ma shadow mode non invia comandi (conforme de facto). Flag `allow_discharge` esplicito assente. | **RISOLUZIONE M4**: Aggiungere flag configurabile `setpoint_shaper::allowDischarge`. Phase 0: false. Fase attiva: configurabile. |
| **LIMIT-01** | R-PV-03 (surplus = FV − carico) estimated-load accuracy | Surplus calculation usa carico stimato 0.6/0.9 kW (R-LOAD-01/02). Carico reale misurato non disponibile fino a power balance validato (M8). | **LIMITATION**: Accuracy limitata fino a bilancio validato M8. Post-M8: sostituire stima con misurato da bilancio (grid + pv − battery). R-PV-03 ALIGNED (estimated-load = limitazione, non conflitto con R-LOAD-04). |
| **ASSUMPTION-01** | Segno telemetria batteria `0x07D8` | La convenzione raw non è ancora verificata empiricamente sul bus. Un segno errato invertirebbe i calcoli EMS. | **M8 read-only**: correlazione con stato batteria, corrente e variazione SOC. |

### 14.2 Aggiunta colonna Compatibility ai requisiti

La colonna **Compatibility** è stata aggiunta dopo "Status" in tutte le 59 tabelle requisiti con i seguenti valori:

- **ALIGNED**: Nessun conflitto noto con altri requisiti o vincoli
- **CONFLICTING**: Conflitto documentato (vedi CONF-XX sopra)
- **NOT_APPLICABLE**: Requisito non applicabile in questo contesto

**Requisiti marcati CONFLICTING**:
- R-CMD-03 (isteresi setpoint 100W) → CONF-02
- R-CMD-06 (Phase 0 no discharge) → CONF-04
- R-CAL-01 (calibrazione 100%) → CONF-03
- R-CAL-03 (calibrazione override maxSOC) → CONF-03
- R-SAF-02 (Degraded actionable) → CONF-01 (risolto)


---

## Rettifica finale M1 a cura della revisione tecnica

- `R-NRG-10` non è un conflitto: è `IMPLEMENTED_NOT_TESTED`, compatibile ma con rischio `CRITICAL_UNVERIFIED_SIGN_CONVENTION`.
- `CONF-06` è sostituito da `ASSUMPTION-01`.
- M8 valida esclusivamente il segno della telemetria batteria `0x07D8` in sola lettura.
- Il segno dei comandi External EMS `0x0A09–0x0A0A` sarà verificato soltanto post-G4 con un piano attivo dedicato.
- Le fonti sono state normalizzate in `Source type` e `Source reference`.
- Nessuna modifica riguarda codice C++, build, seriale o inverter.
