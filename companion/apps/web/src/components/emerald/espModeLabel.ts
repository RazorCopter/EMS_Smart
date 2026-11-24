export type EspTone = "ok" | "warn" | "neutral";

export interface EspControlStatus {
  modeLabel: string;
  modeTone: EspTone;
  realControlValue: "Abilitato" | "Disabilitato" | "N/D";
  realControlTone: EspTone;
  pillTone: "ok" | "warn" | "info";
}

export function espControlStatus(snapshot?: {
  recorded_at: string | null;
  detail: Record<string, unknown>;
}): EspControlStatus {
  const detail = snapshot?.detail;
  const timestamp = detail?.timestamp ?? snapshot?.recorded_at;
  const ageMs = typeof timestamp === "string" ? Date.now() - Date.parse(timestamp) : NaN;
  if (!Number.isFinite(ageMs) || ageMs < -5000 || ageMs > 30000) {
    return {
      modeLabel: "ESP · stato sconosciuto",
      modeTone: "neutral",
      realControlValue: "N/D",
      realControlTone: "neutral",
      pillTone: "info",
    };
  }
  if (detail?.operating_mode === "SHADOW") {
    return {
      modeLabel: "ESP · SHADOW",
      modeTone: "warn",
      realControlValue: "Disabilitato",
      realControlTone: "warn",
      pillTone: "warn",
    };
  }
  if (detail?.operating_mode !== "LIVE") {
    return {
      modeLabel: "ESP · stato sconosciuto",
      modeTone: "neutral",
      realControlValue: "N/D",
      realControlTone: "neutral",
      pillTone: "info",
    };
  }
  const modeLabel =
    detail.live_delegated_native === true
      ? "ESP · LIVE · Self-Use nativo"
      : detail.live_trainer === true
        ? "ESP · LIVE · Trainer"
        : "ESP · LIVE · ExtEMS";
  return {
    modeLabel,
    modeTone: "ok",
    realControlValue: "Abilitato",
    realControlTone: "ok",
    pillTone: "ok",
  };
}

export function espModeLabel(snapshot?: { recorded_at: string | null; detail: Record<string, unknown> }): string {
  return espControlStatus(snapshot).modeLabel;
}
