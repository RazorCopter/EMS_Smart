import { useState } from "react";
import { useQuery } from "@tanstack/react-query";
import { Download } from "lucide-react";
import { apiFetch, rangeQuery } from "../lib/api";
import { fmtKwh, fmtPct, fmtTime, strategyLabel, type TimeRange } from "../lib/format";
import { ShadowBanner } from "../components/ShadowBanner";
import { PageHeader } from "../components/shared/PageHeader";

export function ReportPage() {
  const [range, setRange] = useState<TimeRange>("today");
  const [customStart, setCustomStart] = useState("");
  const [customEnd, setCustomEnd] = useState("");
  const [balance, setBalance] = useState<Array<Record<string, number | string | null>>>([]);
  const [balanceStatus, setBalanceStatus] = useState("");

  const watts = (v: unknown) => (typeof v === "number" ? `${Math.round(v)} W` : "N/D");
  async function loadBalance() {
    setBalanceStatus("Caricamento…");
    try {
      const response = await apiFetch<{ rows: typeof balance }>("/api/v1/diagnostics/power-balance");
      setBalance(response.rows.slice().reverse());
      setBalanceStatus(response.rows.length ? `${response.rows.length} campioni` : "Nessun campione nell’ultima ora");
    } catch {
      setBalanceStatus("Impossibile caricare lo storico.");
    }
  }

  const qs = rangeQuery(range, customStart, customEnd);
  const { data, isLoading } = useQuery({
    queryKey: ["reports", qs],
    queryFn: () => apiFetch<any>(`/api/v1/reports?${qs}`),
    enabled: range !== "custom" || (Boolean(customStart) && Boolean(customEnd)),
  });

  const en = data?.energy ?? {};
  const cmp = data?.comparison ?? {};
  const dq = data?.data_quality ?? {};
  const ems = data?.ems ?? {};

  return (
    <>
      <PageHeader
        title="Report & Esportazioni"
        subtitle="Riepilogo energetico, KPI, strategie, confronto SEM/ESP e qualità sistema"
        actions={
          <>
            <div className="seg-group">
              {(["today", "7d", "30d", "custom"] as TimeRange[]).map((r) => (
                <button key={r} type="button" className={`seg-btn ${range === r ? "active" : ""}`} onClick={() => setRange(r)}>
                  {r === "today" ? "Giornaliero" : r === "7d" ? "Settimanale" : r === "30d" ? "Mensile" : "Personalizzato"}
                </button>
              ))}
            </div>
            <a className="btn primary" href={`/api/v1/export?format=pdf&scope=report&${qs}`}>
              <Download size={14} /> PDF
            </a>
            <a className="btn" href={`/api/v1/export?format=csv&scope=report&${qs}`}>
              <Download size={14} /> CSV
            </a>
            <a className="btn" href={`/api/v1/export?format=json&scope=report&${qs}`}>
              <Download size={14} /> JSON
            </a>
          </>
        }
      />

      {range === "custom" && (
        <section className="card" style={{ display: "flex", gap: 12, flexWrap: "wrap" }}>
          <label style={{ display: "grid", gap: 4, fontSize: 12 }}>
            Inizio
            <input type="datetime-local" className="input-field" value={customStart} onChange={(e) => setCustomStart(e.target.value)} />
          </label>
          <label style={{ display: "grid", gap: 4, fontSize: 12 }}>
            Fine
            <input type="datetime-local" className="input-field" value={customEnd} onChange={(e) => setCustomEnd(e.target.value)} />
          </label>
        </section>
      )}

      <ShadowBanner compact />

      {isLoading || !data ? (
        <div className="card">Generazione report in corso...</div>
      ) : (
        <section className="grid-2">
          <article className="card">
            <div className="card-header"><h2 className="card-title">Riepilogo Energetico &amp; KPI</h2></div>
            <div className="kv-row"><span className="kv-label">Produzione FV</span><span className="kv-val">{fmtKwh(en.pv_kwh)}</span></div>
            <div className="kv-row"><span className="kv-label">Consumo casa (derivato)</span><span className="kv-val">{fmtKwh(en.load_kwh)}</span></div>
            <div className="kv-row"><span className="kv-label">Importazione / Esportazione rete</span><span className="kv-val">{fmtKwh(en.grid_import_kwh)} / {fmtKwh(en.grid_export_kwh)}</span></div>
            <div className="kv-row"><span className="kv-label">Carica / Scarica batteria</span><span className="kv-val">{fmtKwh(en.battery_charge_kwh)} / {fmtKwh(en.battery_discharge_kwh)}</span></div>
            <div className="kv-row"><span className="kv-label">Autoconsumo / Autosufficienza</span><span className="kv-val">{fmtPct(en.self_consumption_pct)} / {fmtPct(en.self_sufficiency_pct)}</span></div>
          </article>

          <article className="card">
            <div className="card-header"><h2 className="card-title">Confronto SEM/ESP, Safety &amp; Qualità</h2></div>
            <div className="kv-row"><span className="kv-label">Concordanza SEM / ESP</span><span className="kv-val">{fmtPct(cmp.concordance_pct)}</span></div>
            <div className="kv-row"><span className="kv-label">Decisioni divergenti</span><span className="kv-val">{cmp.divergent_points ?? 0} / {cmp.comparable_points ?? 0}</span></div>
            <div className="kv-row"><span className="kv-label">Interventi Safety</span><span className="kv-val">{ems.safety_interventions ?? 0}</span></div>
            <div className="kv-row"><span className="kv-label">Disponibilità telemetria</span><span className="kv-val">{fmtPct(dq.availability_pct)}</span></div>
            <div className="kv-row"><span className="kv-label">Gap rilevati / Riavvii ESP</span><span className="kv-val">{dq.decision_gaps ?? 0} / {dq.esp_reboots ?? 0}</span></div>
            <div style={{ marginTop: 10 }}>
              <div style={{ fontSize: 12, color: "var(--text-secondary)", marginBottom: 6 }}>Distribuzione strategie nel periodo:</div>
              {(ems.strategies ?? []).map((s: any) => (
                <span key={s.strategy} className="badge simulated" style={{ marginRight: 6, marginBottom: 4 }}>
                  {strategyLabel(s.strategy)}: {s.decisions}
                </span>
              ))}
            </div>
          </article>
        </section>
      )}

      <section className="cyber-stack">
        <details className="em-card em-balance">
          <summary>Log ESP32</summary>
          <div className="em-balance-actions">
            <a className="inline-action-link" href="/api/v1/diagnostics/esp-logs" download>Scarica log ESP32</a>
          </div>
          <p>Archivio del Companion con telemetria, eventi del Trainer e interruzioni della raccolta.</p>
        </details>

        <details
          className="em-card em-balance"
          onToggle={(event) => {
            if (event.currentTarget.open) void loadBalance();
          }}
        >
          <summary>Casa misurata (POW-Generale) · Bilancio ultima ora</summary>
          <div className="em-balance-actions">
            <button className="btn" type="button" onClick={() => void loadBalance()}>Aggiorna</button>
            <a className="inline-action-link" href="/api/v1/diagnostics/power-balance?format=csv" download>Scarica CSV</a>
            <span role="status">{balanceStatus}</span>
          </div>
          {balance.length > 0 && (
            <div className="em-balance-scroll" tabIndex={0} aria-label="Misure dell’ultima ora">
              <table>
                <thead>
                  <tr>
                    {["Ora", "Casa Tongou", "Casa Afore", "Δ casa", "FV", "AC inverter", "Rete", "Batteria", "Residuo Afore", "Residuo Tongou"].map((label) => (
                      <th key={label} scope="col">{label}</th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {balance.map((row, index) => (
                    <tr key={`${row.sample_utc}-${index}`}>
                      <td>{fmtTime(String(row.sample_utc), "Europe/Rome")}</td>
                      {["house_measured_w", "house_derived_w", "house_delta_w", "pv_w", "inverter_w", "grid_w", "battery_w", "residual_afore_w", "residual_tongou_w"].map((key) => (
                        <td key={key}>{watts(row[key])}</td>
                      ))}
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </details>
      </section>
    </>
  );
}
