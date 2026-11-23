import { useState } from "react";
import { useQuery } from "@tanstack/react-query";
import { apiFetch } from "../lib/api";
import { fmtKwh, fmtNum, fmtPct, strategyLabel, type TimeRange } from "../lib/format";
import { ShadowBanner } from "../components/ShadowBanner";
import { PageHeader } from "../components/shared/PageHeader";

export function AnalysisPage() {
  const [range, setRange] = useState<TimeRange>("today");

  const { data, isLoading } = useQuery({
    queryKey: ["kpi-analysis", range],
    queryFn: () => apiFetch<any>(`/api/v1/kpi?range=${range}`),
  });

  const en = data?.energy ?? {};
  const an = data?.analytics ?? {};
  const dq = an.data_quality ?? {};
  const ems = an.ems ?? {};

  return (
    <>
      <PageHeader
        title="Analisi & KPI Energetici"
        subtitle="Autoconsumo, autosufficienza, prestazioni EMS SHADOW e qualità dei dati"
        actions={
          <div className="seg-group">
            {(["today", "yesterday", "7d", "30d"] as TimeRange[]).map((r) => (
              <button key={r} type="button" className={`seg-btn ${range === r ? "active" : ""}`} onClick={() => setRange(r)}>
                {r === "today" ? "Giornaliero" : r === "yesterday" ? "Ieri" : r === "7d" ? "Settimanale" : "Mensile"}
              </button>
            ))}
          </div>
        }
      />

      <ShadowBanner compact />

      {isLoading ? (
        <div className="card">Calcolo KPI in corso...</div>
      ) : (
        <>
          <section className="grid-4">
            <article className="kpi-card">
              <div style={{ display: "flex", justifyContent: "space-between" }}>
                <span className="kv-label">Autoconsumo</span>
                <span className="badge derived">Formula</span>
              </div>
              <div className="kpi-val" style={{ color: "var(--battery-color)" }}>{fmtPct(en.self_consumption_pct)}</div>
              <span className="card-sub">{en.formulas?.self_consumption}</span>
            </article>
            <article className="kpi-card">
              <div style={{ display: "flex", justifyContent: "space-between" }}>
                <span className="kv-label">Autosufficienza</span>
                <span className="badge derived">Formula</span>
              </div>
              <div className="kpi-val" style={{ color: "var(--accent-teal)" }}>{fmtPct(en.self_sufficiency_pct)}</div>
              <span className="card-sub">{en.formulas?.self_sufficiency}</span>
            </article>
            <article className="kpi-card">
              <span className="kv-label">Perdite stimate bilancio</span>
              <div className="kpi-val">{fmtKwh(en.estimated_losses_kwh)}</div>
              <span className="card-sub">Residuo (FV+Imp+Scarica) - (Casa+Esp+Carica)</span>
            </article>
            <article className="kpi-card">
              <span className="kv-label">Disponibilità telemetria</span>
              <div className="kpi-val">{fmtPct(dq.availability_pct)}</div>
              <span className="card-sub">Gap: {Math.round((dq.gap_seconds ?? 0) / 60)} min · Campioni: {dq.telemetry_samples ?? 0}</span>
            </article>
          </section>

          <section className="grid-3">
            <article className="card">
              <div className="card-header"><h2 className="card-title">Bilancio Energetico</h2><span className="badge measured">Wh integrati</span></div>
              <div className="kv-row"><span className="kv-label">Produzione FV</span><span className="kv-val">{fmtKwh(en.pv_kwh)}</span></div>
              <div className="kv-row"><span className="kv-label">Consumo casa (derivato)</span><span className="kv-val">{fmtKwh(en.load_kwh)}</span></div>
              <div className="kv-row"><span className="kv-label">Importazione rete</span><span className="kv-val">{fmtKwh(en.grid_import_kwh)}</span></div>
              <div className="kv-row"><span className="kv-label">Esportazione rete</span><span className="kv-val">{fmtKwh(en.grid_export_kwh)}</span></div>
              <div className="kv-row"><span className="kv-label">Carica / Scarica batteria</span><span className="kv-val">{fmtKwh(en.battery_charge_kwh)} / {fmtKwh(en.battery_discharge_kwh)}</span></div>
              <div className="kv-row"><span className="kv-label">Cicli equivalenti / Throughput</span><span className="kv-val">{fmtNum(en.equivalent_cycles, "", 2)} · {fmtKwh(en.battery_throughput_kwh)}</span></div>
            </article>

            <article className="card">
              <div className="card-header"><h2 className="card-title">Statistiche EMS SHADOW</h2><span className="badge simulated">Simulato</span></div>
              {(ems.strategies ?? []).map((st: any) => (
                <div key={st.strategy} className="kv-row">
                  <span className="kv-label">{strategyLabel(st.strategy)}</span>
                  <span className="kv-val">{st.decisions} dec. · media {st.avg_simulated_w != null ? `${st.avg_simulated_w} W` : "—"}</span>
                </div>
              ))}
              <div className="kv-row"><span className="kv-label">Interventi Safety</span><span className="kv-val">{ems.safety_interventions ?? 0}</span></div>
              <div className="kv-row"><span className="kv-label">Target raggiunti / scaduti / irraggiungibili</span><span className="kv-val">{ems.targets_reached ?? 0} / {ems.targets_expired ?? 0} / {ems.targets_unreachable ?? 0}</span></div>
              <div className="kv-row"><span className="kv-label">Variazioni setpoint / ora</span><span className="kv-val">{ems.setpoint_oscillations_per_hour ?? "—"}</span></div>
            </article>

            <article className="card">
              <div className="card-header"><h2 className="card-title">Qualità dei Dati &amp; Diagnostica</h2><span className="badge measured">Audit</span></div>
              <div className="kv-row"><span className="kv-label">Disponibilità telemetria</span><span className="kv-val">{fmtPct(dq.availability_pct)}</span></div>
              <div className="kv-row"><span className="kv-label">Gap telemetria</span><span className="kv-val">{Math.round(dq.gap_seconds ?? 0)} s</span></div>
              <div className="kv-row"><span className="kv-label">Errori lettura Modbus (max)</span><span className="kv-val">{dq.max_modbus_errors ?? "—"}</span></div>
              <div className="kv-row"><span className="kv-label">Campioni con fault bus</span><span className="kv-val">{dq.fault_samples ?? 0}</span></div>
              <div className="kv-row"><span className="kv-label">Input HA validi / scaduti</span><span className="kv-val">{dq.ha_valid_inputs ?? 0} / {dq.ha_stale_inputs ?? 0}</span></div>
              <div className="kv-row"><span className="kv-label">Riavvii ESP / Gap sequenza</span><span className="kv-val">{dq.esp_reboots ?? 0} / {dq.decision_gaps ?? 0}</span></div>
            </article>
          </section>
        </>
      )}
    </>
  );
}
