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
    GRID_BALANCE: "Bilanciamento della rete",
    CHARGE_REQUEST: "Richiesta carica batteria attiva",
    SAFETY_LIMIT: "Limite Safety applicato",
    SAFETY_RECLAMP: "Setpoint limitato dai vincoli Safety",
    DEADBAND_OR_RAMP: "Bilanciamento stabile (deadband/rampa)",
    CAPTURE_CHARGE_FIRST: "Cattura solare massima (Charge First fino al 100%)",
    GENTLE_TARGET_CHARGE: "Carica dolce modulata verso orario obiettivo",
    FULL_SOC_HOLD_5M: "Mantenimento al 100% per bilanciamento celle (5 min)",
    FULL_SOC_5MIN_COMPLETE: "100% completato (5 min): passaggio a Self Use",
    MAINTENANCE_HOLD: "Mantenimento carica batteria",
    UNREACHABLE_AT_CURRENT_LIMIT: "Target SOC non raggiungibile entro i limiti correnti",
    NATIVE_OBSERVATION: "Gestione nativa inverter",
    SAFE_REQUESTED: "Modalità sicura attiva",
    SOC_RESERVE: "Riserva SOC raggiunta: scarica bloccata",
    SOC_CEILING: "Limite superiore SOC raggiunto: carica ridotta",
    POWER_LIMIT: "Limite massimo di potenza applicato",
    VOLTAGE_LIMIT: "Limite tensione batteria applicato",
    STALE_TELEMETRY: "Telemetria non aggiornata",
    ESSENTIAL_DATA_UNAVAILABLE: "Dati essenziali non disponibili",
    NONE: "Nessun vincolo Safety intervenuto",
  };
  return r ? map[r] ?? r : "Non disponibile";
}

export function humanDecisionDetail(
  safetyReason: string | null | undefined,
  reason: string | null | undefined,
  soc: number | null | undefined,
  safetyMinW: number | null | undefined,
  safetyMaxW: number | null | undefined,
): { title: string; subtitle: string; tone: "normal" | "safety" | "warn" } {
  const primaryCode = safetyReason && safetyReason !== "NONE" ? safetyReason : reason;
  const title = reasonLabel(primaryCode);
  const socTxt = soc != null ? `SOC ${Math.round(soc)}%` : "SOC n.d.";

  if (primaryCode === "SOC_RESERVE") {
    return {
      title,
      subtitle: `${socTxt} al limite minimo di riserva: scarica sospesa, consentita solo eventuale carica da surplus`,
      tone: "safety",
    };
  }
  if (primaryCode === "SOC_CEILING") {
    return {
      title,
      subtitle: `${socTxt} prossimo al limite massimo configurato: carica ridotta per protezione batteria`,
      tone: "safety",
    };
  }
  if (primaryCode === "POWER_LIMIT" || primaryCode === "SAFETY_LIMIT" || primaryCode === "SAFETY_RECLAMP") {
    const bounds =
      safetyMinW != null && safetyMaxW != null
        ? ` (${Math.round(safetyMinW)} / ${Math.round(safetyMaxW)} W)`
        : "";
    return {
      title,
      subtitle: `${socTxt} · Setpoint adattato all'inviluppo di sicurezza${bounds}`,
      tone: "safety",
    };
  }
  if (primaryCode === "STALE_TELEMETRY" || primaryCode === "ESSENTIAL_DATA_UNAVAILABLE") {
    return {
      title,
      subtitle: "In attesa di campioni freschi e completi dall'inverter prima di simulare nuovi setpoint",
      tone: "warn",
    };
  }
  if (primaryCode === "CAPTURE_CHARGE_FIRST") {
    return {
      title,
      subtitle: `${socTxt} · Giornata a bassa produzione o ciclo invernale: accumulo prioritario fino al 100%, scarica bloccata`,
      tone: "normal",
    };
  }
  if (primaryCode === "GENTLE_TARGET_CHARGE") {
    return {
      title,
      subtitle: `${socTxt} · Carica modulata agli ampere minimi per completare il 100% entro l'orario obiettivo`,
      tone: "normal",
    };
  }
  if (primaryCode === "FULL_SOC_HOLD_5M" || primaryCode === "FULL_SOC_5MIN_COMPLETE") {
    return {
      title,
      subtitle: `${socTxt} · Bilanciamento finale 5 min al 100% prima del ritorno in Self Use`,
      tone: "normal",
    };
  }
  if (primaryCode === "NATIVE_OBSERVATION") {
    return {
      title,
      subtitle: "Inverter osservato in gestione nativa; simulazione SHADOW attiva senza interventi",
      tone: "normal",
    };
  }
  return {
    title,
    subtitle: `${socTxt} · Nessun vincolo restrittivo attivo nella valutazione corrente`,
    tone: "normal",
  };
}

export function fmtShortTime(iso: string | null | undefined, tz = "Europe/Rome"): string {
  if (!iso) return "Non disponibile";
  try {
    return new Date(iso).toLocaleTimeString("it-IT", {
      timeZone: tz,
      hour: "2-digit",
      minute: "2-digit",
    });
  } catch {
    return iso;
  }
}

export function fmtDurationCompact(seconds: number | null | undefined): string {
  if (seconds == null || !Number.isFinite(seconds)) return "Non disponibile";
  const s = Math.max(0, Math.round(seconds));
  if (s < 60) return `${s} s`;
  const mins = Math.floor(s / 60);
  if (mins < 60) return `${mins} min`;
  const hrs = Math.floor(mins / 60);
  const rem = mins % 60;
  return rem > 0 ? `${hrs} h ${rem} min` : `${hrs} h`;
}
