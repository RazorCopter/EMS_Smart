import { useState } from "react";
import { useQuery } from "@tanstack/react-query";
import { Download } from "lucide-react";
import { apiFetch, rangeQuery } from "../lib/api";
import { fmtKwh, fmtPct, strategyLabel, type TimeRange } from "../lib/format";
import { ShadowBanner } from "../components/ShadowBanner";
import { PageHeader } from "../components/shared/PageHeader";

export function ReportPage() {
  const [range, setRange] = useState<TimeRange>("today");
  const [customStart, setCustomStart] = useState("");
  const [customEnd, setCustomEnd] = useState("");

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
    </>
  );
}
