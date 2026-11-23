import { fmtAbsKw, fmtAge, fmtNum, fmtPct, fmtWatts } from "../../lib/format";
import type { TelemetryLive } from "../PowerFlow";

export type NodeId = "pv" | "grid" | "home" | "battery" | "inverter";
export type QualityKind = "measured" | "derived" | "stale" | "unavailable";

export interface DerivedEnergyFlow {
  isStale: boolean;
  pv: number | null;
  load: number | null;
  grid: number | null;
  batt: number | null;
  soc: number | null;
  pvActive: boolean;
  loadActive: boolean;
  gridImport: boolean;
  gridExport: boolean;
  gridActive: boolean;
  battCharge: boolean;
  battDischarge: boolean;
  battActive: boolean;
  pvColor: string;
  loadColor: string;
  gridColor: string;
  battColor: string;
  inactiveColor: string;
  pvQualityKind: QualityKind;
  gridQualityKind: QualityKind;
  loadQualityKind: QualityKind;
  battQualityKind: QualityKind;
  pvStateLabel: string;
  gridDirLabel: string;
  loadDirLabel: string;
  battDirLabel: string;
  invStatusLabel: string;
  accessibleSummary: string;
  specs: Record<NodeId, { title: string; accent: string; rows: Array<[string, string]> }>;
}

export function buildEnergyFlowModel(
  telemetry: TelemetryLive | null,
  deadbandW: number,
  safetyMinW?: number | null,
  safetyMaxW?: number | null,
  selfConsumptionPct?: number | null,
): DerivedEnergyFlow {
  const isStale = Boolean(telemetry?.stale);
  const pv = telemetry?.pv_w ?? null;
  const load = telemetry?.load_w ?? null;
  const grid = telemetry?.grid_w ?? null;
  const batt = telemetry?.battery_w ?? null;
  const soc = telemetry?.soc ?? null;

  const pvActive = !isStale && pv != null && pv > deadbandW;
  const loadActive = !isStale && load != null && load > deadbandW;
  const gridImport = !isStale && grid != null && grid > deadbandW;
  const gridExport = !isStale && grid != null && grid < -deadbandW;
  const gridActive = gridImport || gridExport;
  const battCharge = !isStale && batt != null && batt > deadbandW;
  const battDischarge = !isStale && batt != null && batt < -deadbandW;
  const battActive = battCharge || battDischarge;

  const inactiveColor = "var(--missing-grey)";
  const pvColor = pv == null || isStale ? inactiveColor : pvActive ? "var(--pv-color)" : inactiveColor;
  const loadColor = load == null || isStale ? inactiveColor : loadActive ? "var(--home-color)" : inactiveColor;
  const gridColor = grid == null || isStale ? inactiveColor : gridActive ? "var(--grid-color)" : inactiveColor;
  const battColor = batt == null || isStale ? inactiveColor : battActive ? "var(--battery-color)" : inactiveColor;

  const pvQualityKind: QualityKind = pv == null ? "unavailable" : isStale ? "stale" : "measured";
  const gridQualityKind: QualityKind = grid == null ? "unavailable" : isStale ? "stale" : "measured";
  const loadQualityKind: QualityKind =
    load == null
      ? "unavailable"
      : isStale
      ? "stale"
      : (telemetry?.quality?.load_w?.source ?? "derived") === "derived"
      ? "derived"
      : "measured";
  const battQualityKind: QualityKind = batt == null && soc == null ? "unavailable" : isStale ? "stale" : "measured";

  const pvStateLabel = pv == null ? "Non disp." : pvActive ? "↓ Produzione" : "Inattivo";
  const gridDirLabel =
    grid == null ? "Non disp." : gridImport ? "→ Prelievo" : gridExport ? "← Immissione" : "In equilibrio";
  const loadDirLabel = load == null ? "Non disp." : loadActive ? "← Consumo" : "Standby";
  const battDirLabel =
    batt == null ? "Non disp." : battCharge ? "↑ In carica" : battDischarge ? "↓ In scarica" : "Inattiva";

  const invStatusLabel = !telemetry
    ? "In attesa"
    : telemetry.fault
    ? "Anomalia"
    : isStale
    ? "Non agg."
    : "Operativo";

  const age = telemetry ? fmtAge(telemetry.age_s) : "Non disponibile";
  const qualityStatus = !telemetry ? "Non disponibile" : isStale ? "Non aggiornato (stale)" : "Valido";

  const specs: DerivedEnergyFlow["specs"] = {
    pv: {
      title: "Dettaglio nodo · Fotovoltaico",
      accent: "var(--pv-color)",
      rows: [
        ["Potenza totale", fmtAbsKw(pv)],
        ["Stringa PV1", `${telemetry?.pv1_w != null ? `${Math.round(telemetry.pv1_w)} W` : "—"} · ${fmtNum(telemetry?.pv1_v, "V", 1)}`],
        ["Stringa PV2", `${telemetry?.pv2_w != null ? `${Math.round(telemetry.pv2_w)} W` : "—"} · ${fmtNum(telemetry?.pv2_v, "V", 1)}`],
        ["Provenienza", "Misurato FC04"],
        ["Età campione", age],
        ["Qualità dato", qualityStatus],
      ],
    },
    grid: {
      title: "Dettaglio nodo · Rete elettrica",
      accent: "var(--grid-color)",
      rows: [
        ["Scambio netto", fmtWatts(grid)],
        ["Tensione / Frequenza", `${fmtNum(telemetry?.grid_v, "V", 1)} · ${fmtNum(telemetry?.hz, "Hz", 2)}`],
        ["Provenienza", "Misurato FC04"],
        ["Età campione", age],
        ["Qualità dato", qualityStatus],
      ],
    },
    home: {
      title: "Dettaglio nodo · Utenze Casa",
      accent: "var(--home-color)",
      rows: [
        ["Consumo istantaneo", fmtAbsKw(load)],
        ["Autoconsumo oggi", fmtPct(selfConsumptionPct)],
        ["Tensione / Frequenza", `${fmtNum(telemetry?.grid_v, "V", 1)} · ${fmtNum(telemetry?.hz, "Hz", 2)}`],
        [
          "Provenienza",
          telemetry?.quality?.load_w?.source === "derived" || !telemetry?.quality?.load_w
            ? "Derivato AC (bilancio inverter)"
            : String(telemetry.quality.load_w.source),
        ],
        ["Età campione", age],
        ["Qualità dato", qualityStatus],
      ],
    },
    battery: {
      title: "Dettaglio nodo · Batteria",
      accent: "var(--battery-color)",
      rows: [
        ["SOC attuale", fmtPct(soc)],
        ["Potenza istantanea", `${fmtWatts(batt)} (${battDirLabel})`],
        ["Tensione / Corrente", `${fmtNum(telemetry?.battery_v, "V", 1)} · ${fmtNum(telemetry?.battery_a, "A", 1)}`],
        [
          "Limiti Safety",
          safetyMinW != null && safetyMaxW != null ? `${Math.round(safetyMinW)} / ${Math.round(safetyMaxW)} W` : "Non disponibile",
        ],
        ["Provenienza", "Misurato FC04"],
        ["Età campione", age],
      ],
    },
    inverter: {
      title: "Dettaglio nodo · Inverter Afore",
      accent: "var(--accent-teal)",
      rows: [
        ["Stato sistema", invStatusLabel],
        ["Temperatura interna", fmtNum(telemetry?.temp_c, "°C", 1)],
        ["Letture / Errori RS485", `${telemetry?.reads ?? "—"} / ${telemetry?.errors ?? "—"}`],
        ["Deadband flussi", `±${deadbandW} W`],
        ["Età campione", age],
      ],
    },
  };

  const accessibleSummary = [
    `Fotovoltaico ${pv == null ? "Non disponibile" : fmtAbsKw(pv)} (${pvStateLabel})`,
    `Rete ${grid == null ? "Non disponibile" : fmtWatts(grid)} (${gridDirLabel})`,
    `Casa ${load == null ? "Non disponibile" : fmtAbsKw(load)} (${loadDirLabel})`,
    `Batteria ${batt == null ? "Non disponibile" : fmtWatts(batt)} (${battDirLabel}, SOC ${fmtPct(soc)})`,
    `Inverter ${invStatusLabel}`,
  ].join("; ");

  return {
    isStale,
    pv,
    load,
    grid,
    batt,
    soc,
    pvActive,
    loadActive,
    gridImport,
    gridExport,
    gridActive,
    battCharge,
    battDischarge,
    battActive,
    pvColor,
    loadColor,
    gridColor,
    battColor,
    inactiveColor,
    pvQualityKind,
    gridQualityKind,
    loadQualityKind,
    battQualityKind,
    pvStateLabel,
    gridDirLabel,
    loadDirLabel,
    battDirLabel,
    invStatusLabel,
    accessibleSummary,
    specs,
  };
}
