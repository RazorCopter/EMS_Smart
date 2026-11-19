export type TimeRange = "today" | "yesterday" | "7d" | "30d" | "12m" | "custom";

export function fmtWatts(w: number | null | undefined, unit: "W" | "kW" = "kW"): string {
  if (w == null || !Number.isFinite(w)) return "Non disponibile";
  if (unit === "W") {
    return `${new Intl.NumberFormat("it-IT", { maximumFractionDigits: 0 }).format(w)} W`;
  }
  const kw = w / 1000;
  const sign = w > 0 ? "+" : "";
  return `${sign}${new Intl.NumberFormat("it-IT", {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2,
  }).format(kw)} kW`;
}

export function fmtAbsKw(w: number | null | undefined): string {
  if (w == null || !Number.isFinite(w)) return "Non disponibile";
  return `${new Intl.NumberFormat("it-IT", {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2,
  }).format(Math.abs(w) / 1000)} kW`;
}

export function fmtKwh(kwh: number | null | undefined): string {
  if (kwh == null || !Number.isFinite(kwh)) return "Non disponibile";
  return `${new Intl.NumberFormat("it-IT", {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2,
  }).format(kwh)} kWh`;
}

export function fmtPct(val: number | null | undefined): string {
  if (val == null || !Number.isFinite(val)) return "Non disponibile";
  return `${new Intl.NumberFormat("it-IT", { maximumFractionDigits: 1 }).format(val)}%`;
}

export function fmtNum(val: number | null | undefined, unit = "", digits = 1): string {
  if (val == null || !Number.isFinite(val)) return "Non disponibile";
  const formatted = new Intl.NumberFormat("it-IT", { maximumFractionDigits: digits }).format(val);
  return unit ? `${formatted} ${unit}` : formatted;
}

export function fmtAge(seconds: number | null | undefined): string {
  if (seconds == null || !Number.isFinite(seconds)) return "Sconosciuto";
  if (seconds < 60) return `${Math.round(seconds)} s fa`;
  if (seconds < 3600) return `${Math.floor(seconds / 60)} min fa`;
  return `${Math.floor(seconds / 3600)} h fa`;
}

export function fmtTime(iso: string | null | undefined, tz = "Europe/Rome"): string {
  if (!iso) return "Non disponibile";
  try {
    return new Date(iso).toLocaleString("it-IT", {
      timeZone: tz,
      day: "2-digit",
      month: "2-digit",
      year: "numeric",
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
    });
  } catch {
    return iso;
  }
}

export function strategyLabel(s: string | null | undefined): string {
  const map: Record<string, string> = {
    SELF_USE: "Self Use",
    CHARGE_FIRST: "Charge First",
    TARGET_SOC: "Target SOC",
    MAINTENANCE: "Maintenance",
    NATIVE_INVERTER: "Native Inverter",
    SAFE_MODE: "Safe Mode",
  };
  return s ? map[s] ?? s : "Non disponibile";
}

export function reasonLabel(r: string | null | undefined): string {
  const map: Record<string, string> = {
    GRID_BALANCE: "Bilanciamento rete",
    CHARGE_REQUEST: "Richiesta carica",
    SAFETY_LIMIT: "Limite Safety applicato",
    SAFETY_RECLAMP: "Riadattamento Safety",
    DEADBAND_OR_RAMP: "Filtro deadband / rampa",
    MAINTENANCE_HOLD: "Mantenimento SOC",
    UNREACHABLE_AT_CURRENT_LIMIT: "Target non raggiungibile",
    NATIVE_OBSERVATION: "Osservazione inverter nativo",
    SAFE_REQUESTED: "Modalità sicura attiva",
    SOC_RESERVE: "Riserva SOC raggiunta",
    SOC_CEILING: "Limite superiore SOC",
    POWER_LIMIT: "Limite potenza",
    VOLTAGE_LIMIT: "Limite tensione batteria",
    STALE_TELEMETRY: "Telemetria scaduta",
    ESSENTIAL_DATA_UNAVAILABLE: "Dati essenziali non disponibili",
    NONE: "Nessuna limitazione",
  };
  return r ? map[r] ?? r : "Non disponibile";
}
