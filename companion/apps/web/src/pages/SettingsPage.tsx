import { FormEvent, useEffect, useState } from "react";
import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { apiFetch, type AuthUser } from "../lib/api";
import { fmtTime } from "../lib/format";
import { ShadowBanner } from "../components/ShadowBanner";

interface Props {
  user: AuthUser;
  theme: "dark" | "light";
  onChangeTheme: (next: "dark" | "light") => void;
}

export function SettingsPage({ user, theme, onChangeTheme }: Props) {
  const qc = useQueryClient();
  const { data, isLoading } = useQuery({
    queryKey: ["settings"],
    queryFn: () => apiFetch<any>("/api/v1/settings"),
  });

  const [thresholdW, setThresholdW] = useState(100);
  const [timezone, setTimezone] = useState("Europe/Rome");
  const [unitsPower, setUnitsPower] = useState("kW");
  const [feedback, setFeedback] = useState<string | null>(null);
  const [testResult, setTestResult] = useState<string | null>(null);

  useEffect(() => {
    if (data?.companion) {
      setThresholdW(Number(data.companion.sem_concordance_watt_threshold ?? 100));
      setTimezone(String(data.companion.timezone ?? "Europe/Rome"));
      setUnitsPower(String(data.companion.units_power ?? "kW"));
    }
  }, [data]);

  const saveMutation = useMutation({
    mutationFn: (payload: Record<string, unknown>) =>
      apiFetch("/api/v1/settings", { method: "PATCH", body: JSON.stringify(payload) }),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["settings"] });
      setFeedback("Impostazioni Companion salvate.");
    },
    onError: (err) => {
      setFeedback(err instanceof Error ? err.message : "Errore salvataggio");
    },
  });

  async function runConnectionTest(target: "esp32" | "database") {
    setTestResult("Verifica connessione in corso...");
    try {
      const res = await apiFetch<any>(`/api/v1/settings/test-connection?target=${target}`, { method: "POST" });
      setTestResult(
        res.ok
          ? `${target.toUpperCase()}: Connessione riuscita (${res.firmware || res.detail || "OK"})`
          : `${target.toUpperCase()}: Errore - ${res.detail}`
      );
    } catch (err) {
      setTestResult(err instanceof Error ? err.message : "Test fallito");
    }
  }

  function onSubmit(e: FormEvent) {
    e.preventDefault();
    setFeedback(null);
    saveMutation.mutate({
      sem_concordance_watt_threshold: thresholdW,
      timezone,
      units_power: unitsPower,
      theme,
    });
  }

  const isAdmin = user.role === "admin";
  const integ = data?.integrations ?? {};
  const ret = data?.retention ?? {};

  return (
    <>
      <header className="topbar">
        <div>
          <h1 className="page-title">Impostazioni Companion</h1>
          <p className="page-sub">
            Ruolo attivo: <strong>{user.role}</strong> · Nessun segreto o password viene esposto dalle API
          </p>
        </div>
        <span className="chip shadow">Controllo inverter disabilitato per design</span>
      </header>

      <ShadowBanner compact extra="Nessun ruolo può abilitare la modalità LIVE sull'ESP32" />

      {isLoading || !data ? (
        <div className="card">Caricamento configurazione...</div>
      ) : (
        <section className="grid-2">
          <article className="card">
            <div className="card-header">
              <h2 className="card-title">Stato Integrazioni (Sola Lettura)</h2>
              <span className="badge measured">Segreti redatti</span>
            </div>
            <div className="kv-row">
              <span className="kv-label">MQTT Broker</span>
              <span className="kv-val">
                {integ.mqtt?.host}:{integ.mqtt?.port} · pwd configured={String(Boolean(integ.mqtt?.password_configured))} ·{" "}
                <strong>{integ.mqtt?.status}</strong>
              </span>
            </div>
            <div className="kv-row">
              <span className="kv-label">ESP32 REST (Bootstrap/Backfill)</span>
              <span className="kv-val">
                {integ.esp32?.url} · pwd configured={String(Boolean(integ.esp32?.password_configured))} ·{" "}
                <strong>{integ.esp32?.status}</strong>
              </span>
            </div>
            <div className="kv-row">
              <span className="kv-label">Home Assistant</span>
              <span className="kv-val">
                MQTT push: {integ.home_assistant?.status} · REST fallback: {String(Boolean(integ.home_assistant?.rest_fallback_enabled))}
              </span>
            </div>
            <div className="kv-row">
              <span className="kv-label">Ultimo collegamento MQTT / ESP32</span>
              <span className="kv-val">{fmtTime(integ.mqtt?.last_connected_at || integ.esp32?.last_connected_at)}</span>
            </div>
            <div className="kv-row">
              <span className="kv-label">Retention TimescaleDB / Backup</span>
              <span className="kv-val">
                Raw {ret.raw_mqtt_days}g · 5s {ret.telemetry_days}g · 1m {ret.agg_1m_days}g · Backup {ret.backup_retention_days}g
              </span>
            </div>

            {isAdmin && (
              <div style={{ display: "flex", gap: 8, marginTop: 14, flexWrap: "wrap" }}>
                <button type="button" className="btn" onClick={() => runConnectionTest("database")}>
                  Test Database
                </button>
                <button type="button" className="btn" onClick={() => runConnectionTest("esp32")}>
                  Test ESP32 Read-Only
                </button>
              </div>
            )}
            {testResult && (
              <p role="status" style={{ marginTop: 10, fontSize: 12.5, color: "var(--accent-teal)" }}>
                {testResult}
              </p>
            )}
          </article>

          <article className="card">
            <div className="card-header">
              <h2 className="card-title">Preferenze UI &amp; Soglie Confronto</h2>
              {!isAdmin && <span className="badge unavailable">Solo Admin modificabile</span>}
            </div>
            <form onSubmit={onSubmit} style={{ display: "flex", flexDirection: "column", gap: 12 }}>
              <label style={{ display: "grid", gap: 5, fontSize: 12.5, color: "var(--text-secondary)" }}>
                Soglia concordanza SEM / ESP (W)
                <input
                  type="number"
                  className="input-field"
                  min={10}
                  max={2000}
                  value={thresholdW}
                  onChange={(e) => setThresholdW(Number(e.target.value))}
                  disabled={!isAdmin}
                />
              </label>

              <label style={{ display: "grid", gap: 5, fontSize: 12.5, color: "var(--text-secondary)" }}>
                Timezone
                <input
                  type="text"
                  className="input-field"
                  value={timezone}
                  onChange={(e) => setTimezone(e.target.value)}
                  disabled={!isAdmin}
                />
              </label>

              <label style={{ display: "grid", gap: 5, fontSize: 12.5, color: "var(--text-secondary)" }}>
                Unità potenza predefinita
                <select
                  className="input-field"
                  value={unitsPower}
                  onChange={(e) => setUnitsPower(e.target.value)}
                  disabled={!isAdmin}
                >
                  <option value="kW">kW (kilowatt)</option>
                  <option value="W">W (watt)</option>
                </select>
              </label>

              <label style={{ display: "grid", gap: 5, fontSize: 12.5, color: "var(--text-secondary)" }}>
                Tema interfaccia
                <select
                  className="input-field"
                  value={theme}
                  onChange={(e) => onChangeTheme(e.target.value === "light" ? "light" : "dark")}
                >
                  <option value="dark">Scuro (predefinito)</option>
                  <option value="light">Chiaro</option>
                </select>
              </label>

              {isAdmin && (
                <button type="submit" className="btn primary" disabled={saveMutation.isPending}>
                  {saveMutation.isPending ? "Salvataggio..." : "Salva impostazioni"}
                </button>
              )}
              {feedback && (
                <p role="status" style={{ fontSize: 12.5, color: "var(--accent-teal)" }}>
                  {feedback}
                </p>
              )}
            </form>
          </article>
        </section>
      )}
    </>
  );
}
