import { FormEvent, useEffect, useState } from "react";
import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { apiFetch, type AuthUser } from "../lib/api";
import { fmtTime } from "../lib/format";
import { HelpTip } from "../components/HelpTip";
import { ShadowBanner } from "../components/ShadowBanner";
import { PageHeader } from "../components/shared/PageHeader";
import { ThemeSettingsCard } from "../components/shared/ThemeControls";
import { useTheme } from "../theme/useTheme";

interface Props {
  user: AuthUser;
}

export function SettingsPage({ user }: Props) {
  const { resolvedScheme } = useTheme();
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
      theme: resolvedScheme,
    });
  }

  const isAdmin = user.role === "admin";
  const integ = data?.integrations ?? {};
  const ret = data?.retention ?? {};

  return (
    <>
      <PageHeader
        title="Impostazioni Companion"
        subtitle={`Ruolo attivo: ${user.role} · Nessun segreto o password viene esposto dalle API`}
        badges={<span className="chip shadow">Controllo inverter disabilitato per design</span>}
      />

      <ShadowBanner compact extra="Nessun ruolo può abilitare la modalità LIVE sull'ESP32" />

      {isLoading || !data ? (
        <div className="card">Caricamento configurazione...</div>
      ) : (
        <section className="grid-2">
          <ThemeSettingsCard />
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
                Raw {ret.raw_mqtt_days}g · 5s {ret.telemetry_days}g · Decisioni {ret.decisions_days ?? 180}g · 1m {ret.agg_1m_days}g · Backup{" "}
                {ret.backup_retention_days}g
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

              <p className="card-sub" style={{ margin: 0 }}>
                Lo schema colore, lo stile (Classic / Emerald Control) e la densità si gestiscono nella sezione{" "}
                <strong>Aspetto &amp; Tema Multi-Style</strong> sopra, con salvataggio per singolo utente.
              </p>

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

      {!isLoading && data?.resources && (() => {
        const esp = data.resources.esp32 ?? {};
        const api = data.resources.containers?.api ?? {};
        const coll = data.resources.containers?.collector ?? {};
        const db = data.resources.database ?? {};
        const fmtKb = (b: unknown) => (typeof b === "number" ? `${Math.round(b / 1024)} KB` : "N/D");
        const fmtMb = (b: unknown) => (typeof b === "number" ? `${(b / (1024 * 1024)).toFixed(2)} MB` : "N/D");
        return (
          <section className="grid-2" style={{ marginTop: 16 }}>
            <article className="card">
              <div className="card-header">
                <div>
                  <h2 className="card-title">Diagnostica Risorse ESP32 ({esp.ota_slot || "app"})</h2>
                  <p className="card-sub">CPU FreeRTOS, SRAM interna, PSRAM ring buffer e Flash NVS</p>
                </div>
                <span className="badge measured">{esp.firmware || "v5.1"}</span>
              </div>
              <div className="kv-row">
                <span className="kv-label">
                  Utilizzo CPU media (Core 0 / Core 1)
                  <HelpTip id="tip-set-esp-cpu" title="CPU ESP32" text="Calcolata via FreeRTOS uxTaskGetSystemState (timer hardware microsecondi)." align="left" />
                </span>
                <span className="kv-val">
                  {typeof esp.cpu_pct === "number"
                    ? `${esp.cpu_pct.toFixed(1)}% (C0 ${esp.cpu0_pct?.toFixed(1) ?? "?"}% · C1 ${esp.cpu1_pct?.toFixed(1) ?? "?"}%) @ ${esp.cpu_mhz ?? 240} MHz`
                    : "In attesa campione"}
                </span>
              </div>
              <div className="kv-row">
                <span className="kv-label">SRAM interna (occupata / libera)</span>
                <span className="kv-val">
                  {typeof esp.heap_used_pct === "number" ? `${esp.heap_used_pct.toFixed(1)}%` : "N/D"} · libera {fmtKb(esp.heap_free)} / {fmtKb(esp.heap_total)}
                </span>
              </div>
              <div className="kv-row">
                <span className="kv-label">Minimo storico SRAM / Blocco contiguo max</span>
                <span className="kv-val">
                  min {fmtKb(esp.heap_min)} · max block {fmtKb(esp.heap_max_alloc)}
                </span>
              </div>
              <div className="kv-row">
                <span className="kv-label">PSRAM (8 MB, incl. ring 256 decisioni)</span>
                <span className="kv-val">
                  {typeof esp.psram_used_pct === "number" ? `${esp.psram_used_pct.toFixed(1)}% usata` : "N/D"} · libera {fmtMb(esp.psram_free)} / {fmtMb(esp.psram_total)}
                </span>
              </div>
              <div className="kv-row">
                <span className="kv-label">Flash NVS &amp; Sketch</span>
                <span className="kv-val">
                  NVS {esp.nvs_used_entries ?? "?"}/{esp.nvs_total_entries ?? "?"} ({esp.nvs_used_pct ?? "?"}%) · Sketch {fmtMb(esp.sketch_size)} / {fmtMb(esp.sketch_free)}
                </span>
              </div>
              <div className="kv-row">
                <span className="kv-label">Task attivi / Stack app min / Wi-Fi RSSI</span>
                <span className="kv-val">
                  {esp.task_count ?? "?"} task · min stack app {esp.min_application_stack_free_b ?? esp.min_task_stack_free_b ?? "?"} B ({esp.min_application_stack_name ?? "IDLE0"}) · {esp.rssi_dbm ?? "?"} dBm
                </span>
              </div>
            </article>

            <article className="card">
              <div className="card-header">
                <div>
                  <h2 className="card-title">Risorse Container, TimescaleDB &amp; Disco</h2>
                  <p className="card-sub">Protezione anti-saturazione disco e consumo container LXC</p>
                </div>
                <span className="badge measured">{typeof db.db_size_mb === "number" ? `${db.db_size_mb.toFixed(1)} MB DB` : "TimescaleDB"}</span>
              </div>
              <div className="kv-row">
                <span className="kv-label">Container API (CPU / RAM RSS / cgroup)</span>
                <span className="kv-val">
                  CPU {api.cpu_pct ?? "?"}% · RSS {api.process_rss_mb ?? "?"} MB · cgroup {api.cgroup_mem_mb ?? "?"} MB
                </span>
              </div>
              <div className="kv-row">
                <span className="kv-label">Container Collector (CPU / RAM RSS / cgroup)</span>
                <span className="kv-val">
                  CPU {coll.cpu_pct ?? "?"}% · RSS {coll.process_rss_mb ?? "?"} MB · cgroup {coll.cgroup_mem_mb ?? "?"} MB
                </span>
              </div>
              <div className="kv-row">
                <span className="kv-label">Filesystem LXC (/ host volume)</span>
                <span className="kv-val">
                  {api.disk_used_gb ?? "?"} / {api.disk_total_gb ?? "?"} GB ({api.disk_used_pct ?? "?"}% · {api.disk_free_gb ?? "?"} GB liberi)
                </span>
              </div>
              {Array.isArray(db.hypertables) && db.hypertables.length > 0 && (
                <table className="diag-table">
                  <thead>
                    <tr>
                      <th>Hypertable</th>
                      <th>Dimensione</th>
                      <th>Chunk (compressi)</th>
                    </tr>
                  </thead>
                  <tbody>
                    {db.hypertables.map((ht: any) => (
                      <tr key={ht.name}>
                        <td><code>{ht.name}</code></td>
                        <td>{ht.size_mb.toFixed(2)} MB</td>
                        <td>{ht.chunks} ({ht.compressed_chunks})</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              )}
            </article>
          </section>
        );
      })()}
    </>
  );
}
