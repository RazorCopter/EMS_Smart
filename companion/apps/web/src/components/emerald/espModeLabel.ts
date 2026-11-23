export function espModeLabel(snapshot?: { recorded_at: string | null; detail: Record<string, unknown> }): string {
  const detail = snapshot?.detail;
  const timestamp = detail?.timestamp ?? snapshot?.recorded_at;
  const ageMs = typeof timestamp === "string" ? Date.now() - Date.parse(timestamp) : NaN;
  if (!Number.isFinite(ageMs) || ageMs < -5000 || ageMs > 30000) return "ESP · stato sconosciuto";
  if (detail?.operating_mode === "SHADOW") return "ESP · SHADOW";
  if (detail?.operating_mode !== "LIVE") return "ESP · stato sconosciuto";
  if (detail.live_delegated_native === true) return "ESP · LIVE · Self-Use nativo";
  if (detail.live_trainer === true) return "ESP · LIVE · Trainer";
  return "ESP · LIVE · ExtEMS";
}
