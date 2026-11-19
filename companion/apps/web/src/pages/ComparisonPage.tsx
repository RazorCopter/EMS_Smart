import { useMemo, useState } from "react";
import { useQuery } from "@tanstack/react-query";
import type { EChartsOption } from "echarts";
import { apiFetch } from "../lib/api";
import { fmtPct, fmtWatts, strategyLabel, type TimeRange } from "../lib/format";
import { EChart } from "../components/EChart";

export function ComparisonPage() {
  const [range, setRange] = useState<TimeRange>("today");
  const [thresholdW, setThresholdW] = useState<number>(100);

  const { data, isLoading } = useQuery({
    queryKey: ["comparison", range, thresholdW],
    queryFn: () => apiFetch<any>(`/api/v1/comparison?range=${range}&watt_threshold=${thresholdW}`),
    refetchInterval: 15000,
  });

  const kpi = data?.kpi ?? {};
  const series: any[] = data?.series ?? [];

  const chartOption: EChartsOption = useMemo(() => {
    return {
      tooltip: { trigger: "axis", confine: true },
      legend: { top: 0, textStyle: { color: "#98b0ad", fontSize: 11 } },
      grid: { top: 52, right: 24, bottom: 32, left: 52 },
      xAxis: {
        type: "category",
        data: series.map((p) => new Date(p.timestamp).toLocaleTimeString("it-IT", { timeZone: "Europe/Rome" })),
        axisLabel: { color: "#6e8683", fontSize: 10 },
      },
      yAxis: {
        type: "value",
        name: "W",
        axisLabel: { color: "#6e8683", fontSize: 10 },
        splitLine: { lineStyle: { color: "#1f3134" } },
      },
      series: [
        { name: "SEM (reale / target)", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: "#c084fc", width: 2.2 }, data: series.map((p) => p.sem_target_w) },
        { name: "ESP (SHADOW simulato)", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: "#38bdf8", width: 2.2, type: "dashed" }, data: series.map((p) => p.esp_simulated_w) },
        { name: "Batteria misurata", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: "#34d399", width: 1.8 }, data: series.map((p) => p.actual_battery_w) },
        { name: "Rete misurata", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: "#60a5fa", width: 1.6 }, data: series.map((p) => p.actual_grid_w) },
      ],
    };
  }, [series]);

  return (
    <>
      <header className="topbar">
        <div>
          <h1 className="page-title">Confronto SEM / ESP32 SHADOW</h1>
          <p className="page-sub">Qualificazione comportamento simulato ESP32 rispetto al controllore reale Home Assistant SEM</p>
        </div>
        <div className="topbar-right">
          <label className="chip" style={{ gap: 8 }}>
            Soglia (W)
            <input
              type="number"
              min={10}
              max={2000}
              step={10}
              value={thresholdW}
              onChange={(e) => setThresholdW(Number(e.target.value) || 100)}
              style={{ width: 68, background: "transparent", border: 0, textAlign: "right", fontWeight: 700 }}
            />
          </label>
          <div className="seg-group">
            {(["today", "yesterday", "7d", "30d"] as TimeRange[]).map((r) => (
              <button key={r} type="button" className={`seg-btn ${range === r ? "active" : ""}`} onClick={() => setRange(r)}>
                {r === "today" ? "Oggi" : r === "yesterday" ? "Ieri" : r}
              </button>
            ))}
          </div>
        </div>
      </header>

      <div
        className="shadow-banner"
        style={{
          borderColor: "rgba(245, 158, 11, 0.45)",
          background: "linear-gradient(90deg, rgba(245, 158, 11, 0.14), rgba(56, 189, 248, 0.1))",
        }}
        role="note"
      >
        <div>
          <strong style={{ color: "var(--safety-amber)" }}>SEM è il controllore reale attuale.</strong>{" "}
          <span>L&apos;ESP produce decisioni simulate (SHADOW) per analisi e validazione. Nessuna simulazione controfattuale è un comando reale.</span>
        </div>
        <span className="badge simulated">0 scritture ESP</span>
      </div>

      <section className="grid-5">
        <article className="kpi-card">
          <span className="kv-label">Concordanza</span>
          <div className="kpi-val" style={{ color: "var(--battery-color)" }}>{fmtPct(kpi.concordance_pct)}</div>
          <span className="card-sub">{kpi.concordant_points ?? 0} / {kpi.comparable_points ?? 0} confrontabili</span>
        </article>
        <article className="kpi-card">
          <span className="kv-label">Differenza media</span>
          <div className="kpi-val">{kpi.mean_watt_diff != null ? `${kpi.mean_watt_diff} W` : "—"}</div>
          <span className="card-sub">P95: {kpi.p95_watt_diff != null ? `${kpi.p95_watt_diff} W` : "—"}</span>
        </article>
        <article className="kpi-card">
          <span className="kv-label">Decisioni divergenti</span>
          <div className="kpi-val" style={{ color: "var(--safety-amber)" }}>{kpi.divergent_points ?? 0}</div>
          <span className="card-sub">Falsi pos: {kpi.false_positives ?? 0} · Perse: {kpi.missed_opportunities ?? 0}</span>
        </article>
        <article className="kpi-card">
          <span className="kv-label">Interventi Safety</span>
          <div className="kpi-val" style={{ color: "var(--safety-amber)" }}>{kpi.safety_interventions ?? 0}</div>
          <span className="card-sub">Vincoli applicati in SHADOW</span>
        </article>
        <article className="kpi-card">
          <span className="kv-label">Oscillazioni setpoint</span>
          <div className="kpi-val">{kpi.setpoint_oscillations_per_hour != null ? `${kpi.setpoint_oscillations_per_hour}/h` : "—"}</div>
          <span className="card-sub">Variazioni &gt; {thresholdW} W/ora</span>
        </article>
      </section>

      <section className="grid-2">
        <article className="card">
          <div className="card-header">
            <div>
              <h2 className="card-title">SEM Reale vs ESP Simulato vs Rete Misurata</h2>
              <p className="card-sub">Confronto temporale fra controllore attivo e simulazione</p>
            </div>
          </div>
          {isLoading ? (
            <p className="card-sub">Caricamento confronto...</p>
          ) : (
            <EChart option={chartOption} height={290} ariaLabel="Grafico confronto SEM reale ed ESP SHADOW simulato" />
          )}
        </article>

        <article className="card">
          <div className="card-header">
            <h2 className="card-title">Criterio di concordanza &amp; Divergenze recenti</h2>
            <span className="badge configured">Soglia ±{thresholdW} W</span>
          </div>
          {(data?.thresholds?.criteria ?? []).map((c: string) => (
            <div key={c} className="kv-row">
              <span className="kv-label">{c}</span>
              <span className="badge measured">Attivo</span>
            </div>
          ))}
          <div className="table-wrap" style={{ marginTop: 10, maxHeight: 220 }}>
            <table className="data-table">
              <thead>
                <tr>
                  <th>Ora</th>
                  <th>SEM (Reale)</th>
                  <th>ESP (Sim.)</th>
                  <th>Δ</th>
                  <th>Esito</th>
                </tr>
              </thead>
              <tbody>
                {[...series].reverse().slice(0, 30).map((pt, idx) => (
                  <tr key={idx}>
                    <td>{new Date(pt.timestamp).toLocaleTimeString("it-IT", { timeZone: "Europe/Rome" })}</td>
                    <td>{pt.sem_decision ?? "—"}</td>
                    <td>{strategyLabel(pt.esp_strategy)} ({fmtWatts(pt.esp_simulated_w, "W")})</td>
                    <td>{pt.watt_diff != null ? `${pt.watt_diff} W` : "—"}</td>
                    <td>
                      <span className={`badge ${!pt.comparable ? "unavailable" : pt.concordant ? "measured" : "safety"}`}>
                        {!pt.comparable ? "N.C." : pt.concordant ? "Concorde" : pt.reason}
                      </span>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </article>
      </section>
    </>
  );
}
