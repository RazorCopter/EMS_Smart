import { useMemo, useState } from "react";
import { useSearchParams } from "react-router-dom";
import { useQuery } from "@tanstack/react-query";
import type { EChartsOption } from "echarts";
import { X } from "lucide-react";
import { apiFetch } from "../lib/api";
import { fmtTime, fmtWatts, reasonLabel, strategyLabel, type TimeRange } from "../lib/format";
import { EChart } from "../components/EChart";
import { ShadowBanner } from "../components/ShadowBanner";
import { useTheme } from "../theme/useTheme";

const STRATEGY_COLORS: Record<string, { bg: string; fg: string }> = {
  SELF_USE: { bg: "rgba(45, 212, 191, 0.18)", fg: "#5eead4" },
  CHARGE_FIRST: { bg: "rgba(245, 185, 66, 0.2)", fg: "#fcd34d" },
  TARGET_SOC: { bg: "rgba(167, 139, 250, 0.2)", fg: "#c4b5fd" },
  MAINTENANCE: { bg: "rgba(96, 165, 250, 0.2)", fg: "#93c5fd" },
  NATIVE_INVERTER: { bg: "rgba(100, 116, 139, 0.22)", fg: "#cbd5e1" },
  SAFE_MODE: { bg: "rgba(245, 158, 11, 0.22)", fg: "#fde68a" },
};

export function DecisionsPage() {
  const { chartPalette } = useTheme();
  const [searchParams] = useSearchParams();
  const [range, setRange] = useState<TimeRange>("today");
  const [strategyFilter, setStrategyFilter] = useState<string>("");
  const [safetyOnly, setSafetyOnly] = useState(false);
  const [selectedId, setSelectedId] = useState<string | null>(() => searchParams.get("id"));

  const decQuery = useQuery({
    queryKey: ["decisions", range, strategyFilter, safetyOnly],
    queryFn: () => {
      const p = new URLSearchParams({ range, limit: "250" });
      if (strategyFilter) p.set("strategy", strategyFilter);
      if (safetyOnly) p.set("safety_only", "true");
      return apiFetch<any>(`/api/v1/decisions?${p.toString()}`);
    },
    refetchInterval: 10000,
  });

  const evQuery = useQuery({
    queryKey: ["events", range],
    queryFn: () => apiFetch<any>(`/api/v1/events?range=${range}&limit=100`),
    refetchInterval: 10000,
  });

  const detailQuery = useQuery({
    queryKey: ["decision-detail", selectedId],
    queryFn: () => apiFetch<any>(`/api/v1/decisions/${encodeURIComponent(selectedId!)}`),
    enabled: Boolean(selectedId),
  });

  const decisions: any[] = [...(decQuery.data?.decisions ?? [])].reverse();
  const intervals: any[] = decQuery.data?.intervals ?? [];
  const events: any[] = evQuery.data?.events ?? [];

  const chartOption: EChartsOption = useMemo(() => {
    return {
      tooltip: {
        trigger: "axis",
        confine: true,
        backgroundColor: chartPalette.tooltipBg,
        borderColor: chartPalette.tooltipBorder,
        textStyle: { color: chartPalette.textPrimary, fontSize: 12 },
      },
      legend: { top: 0, textStyle: { color: chartPalette.textSecondary, fontSize: 11 } },
      grid: { top: 52, right: 20, bottom: 32, left: 52 },
      xAxis: {
        type: "category",
        data: decisions.map((d) => new Date(d.source_timestamp).toLocaleTimeString("it-IT", { timeZone: "Europe/Rome" })),
        axisLabel: { color: chartPalette.axisLabel, fontSize: 10 },
      },
      yAxis: {
        type: "value",
        name: "W",
        axisLabel: { color: chartPalette.axisLabel, fontSize: 10 },
        splitLine: { lineStyle: { color: chartPalette.splitLine } },
      },
      series: [
        { name: "Candidato", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.candidate, width: 1.5, type: "dotted" }, itemStyle: { color: chartPalette.candidate }, data: decisions.map((d) => d.candidate_w) },
        { name: "Safety-limited", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.safety, width: 2 }, itemStyle: { color: chartPalette.safety }, data: decisions.map((d) => d.limited_w) },
        { name: "Simulato ESP", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.shadow, width: 2.4 }, itemStyle: { color: chartPalette.shadow }, data: decisions.map((d) => d.simulated_w) },
        { name: "Batteria misurata", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.battery, width: 2 }, itemStyle: { color: chartPalette.battery }, data: decisions.map((d) => d.battery_w) },
        { name: "Rete misurata", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.grid, width: 1.5 }, itemStyle: { color: chartPalette.grid }, data: decisions.map((d) => d.grid_w) },
        { name: "Envelope Max", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.safety, width: 1, type: "dashed" }, itemStyle: { color: chartPalette.safety }, data: decisions.map((d) => d.safety_max_w) },
        { name: "Envelope Min", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.safety, width: 1, type: "dashed" }, itemStyle: { color: chartPalette.safety }, data: decisions.map((d) => d.safety_min_w) },
      ],
    };
  }, [decisions, chartPalette]);

  const activeDetail = detailQuery.data;

  return (
    <>
      <header className="topbar">
        <div>
          <h1 className="page-title">Decisioni EMS &amp; Audit SHADOW</h1>
          <p className="page-sub">Timeline strategie, vincoli Safety, eventi e ispezione per sequence ID</p>
        </div>
        <div className="topbar-right">
          <select
            aria-label="Filtra strategia"
            className="input-field"
            style={{ width: "auto" }}
            value={strategyFilter}
            onChange={(e) => setStrategyFilter(e.target.value)}
          >
            <option value="">Tutte le strategie</option>
            <option value="SELF_USE">Self Use</option>
            <option value="CHARGE_FIRST">Charge First</option>
            <option value="TARGET_SOC">Target SOC</option>
            <option value="MAINTENANCE">Maintenance</option>
            <option value="NATIVE_INVERTER">Native Inverter</option>
            <option value="SAFE_MODE">Safe Mode</option>
          </select>
          <button
            type="button"
            className={`btn ${safetyOnly ? "primary" : ""}`}
            onClick={() => setSafetyOnly((v) => !v)}
          >
            Solo Safety
          </button>
          <div className="seg-group">
            {(["today", "yesterday", "7d"] as TimeRange[]).map((r) => (
              <button key={r} type="button" className={`seg-btn ${range === r ? "active" : ""}`} onClick={() => setRange(r)}>
                {r === "today" ? "Oggi" : r === "yesterday" ? "Ieri" : "7g"}
              </button>
            ))}
          </div>
        </div>
      </header>

      <ShadowBanner compact />

      <section className="card">
        <div className="card-header">
          <h2 className="card-title">Timeline Strategie</h2>
          <span className="card-sub">{intervals.length} intervalli · {decQuery.data?.total ?? 0} decisioni</span>
        </div>
        {intervals.length === 0 ? (
          <p className="card-sub">Nessun intervallo registrato nella finestra selezionata.</p>
        ) : (
          <div style={{ display: "flex", gap: 4, flexWrap: "wrap" }}>
            {intervals.map((iv) => {
              const col = STRATEGY_COLORS[iv.strategy] ?? STRATEGY_COLORS.NATIVE_INVERTER;
              return (
                <div
                  key={iv.id}
                  style={{
                    background: col.bg,
                    color: col.fg,
                    padding: "8px 12px",
                    borderRadius: 8,
                    fontSize: 12,
                    fontWeight: 650,
                    flexGrow: Math.max(1, iv.decision_count),
                  }}
                >
                  {strategyLabel(iv.strategy)} · {Math.max(1, Math.round(iv.duration_s / 60))} min
                </div>
              );
            })}
          </div>
        )}
      </section>

      <section className="card">
        <div className="card-header">
          <h2 className="card-title">Candidato vs Safety-Limited vs Setpoint Simulato vs Reale</h2>
        </div>
        <EChart option={chartOption} height={260} ariaLabel="Grafico decisioni EMS e envelope Safety" />
      </section>

      <section className="grid-2">
        <article className="card">
          <div className="card-header">
            <h2 className="card-title">Registro Decisioni</h2>
            <span className="badge simulated">Seleziona riga per dettagli</span>
          </div>
          <div className="table-wrap" style={{ maxHeight: 380 }}>
            <table className="data-table resp-cards">
              <thead>
                <tr>
                  <th>Ora / Seq</th>
                  <th>Strategia</th>
                  <th>Candidato</th>
                  <th>Simulato</th>
                  <th>Safety / Motivo</th>
                </tr>
              </thead>
              <tbody>
                {(decQuery.data?.decisions ?? []).slice(0, 80).map((d: any) => (
                  <tr
                    key={d.id}
                    className={selectedId === d.id ? "selected" : ""}
                    onClick={() => setSelectedId(d.id)}
                  >
                    <td data-label="Ora / Seq">
                      {new Date(d.source_timestamp).toLocaleTimeString("it-IT", { timeZone: "Europe/Rome" })} · #{d.seq}
                    </td>
                    <td data-label="Strategia">{strategyLabel(d.strategy)}</td>
                    <td data-label="Candidato">{fmtWatts(d.candidate_w, "W")}</td>
                    <td data-label="Simulato" style={{ color: "var(--shadow-blue)", fontWeight: 600 }}>{fmtWatts(d.simulated_w, "W")}</td>
                    <td data-label="Safety / Motivo">
                      <span className={`badge ${d.safety_reason !== "NONE" ? "safety" : "measured"}`}>
                        {d.safety_reason !== "NONE" ? reasonLabel(d.safety_reason) : reasonLabel(d.reason)}
                      </span>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </article>

        <article className="card">
          <div className="card-header">
            <h2 className="card-title">Timeline Eventi</h2>
            <span className="card-sub">{events.length} eventi</span>
          </div>
          <div className="table-wrap" style={{ maxHeight: 380 }}>
            <table className="data-table">
              <thead>
                <tr>
                  <th>Ora</th>
                  <th>Tipo</th>
                  <th>Dettaglio</th>
                </tr>
              </thead>
              <tbody>
                {events.map((ev) => (
                  <tr key={ev.id}>
                    <td>{new Date(ev.occurred_at).toLocaleTimeString("it-IT", { timeZone: "Europe/Rome" })}</td>
                    <td>
                      <span className={`badge ${ev.severity === "warning" ? "safety" : "simulated"}`}>
                        {ev.event_type}
                      </span>
                    </td>
                    <td>
                      <strong>{ev.title}</strong>
                      {ev.detail && <div style={{ fontSize: 11, color: "var(--text-muted)" }}>{ev.detail}</div>}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </article>
      </section>

      {selectedId && activeDetail && (
        <div className="decision-drawer-overlay" onClick={() => setSelectedId(null)}>
          <aside className="decision-drawer" onClick={(e) => e.stopPropagation()} role="dialog" aria-label="Dettaglio decisione">
            <div className="card-header">
              <div>
                <h2 className="card-title">Dettaglio Decisione · #{activeDetail.seq}</h2>
                <p className="card-sub">{fmtTime(activeDetail.source_timestamp)} · {activeDetail.boot_id}</p>
              </div>
              <button type="button" className="btn" onClick={() => setSelectedId(null)} aria-label="Chiudi dettaglio">
                <X size={16} />
              </button>
            </div>
            <div className="kv-row"><span className="kv-label">Strategia / Configurata</span><span className="kv-val">{activeDetail.strategy} ({activeDetail.configured_strategy})</span></div>
            <div className="kv-row"><span className="kv-label">Stato / Transizione</span><span className="kv-val">{activeDetail.state} · {activeDetail.transition}</span></div>
            <div className="kv-row"><span className="kv-label">SOC / PV / Casa / Rete</span><span className="kv-val">{activeDetail.telemetry?.soc ?? "—"}% · {fmtWatts(activeDetail.telemetry?.pv_w, "W")} · {fmtWatts(activeDetail.telemetry?.load_w, "W")} · {fmtWatts(activeDetail.telemetry?.grid_w, "W")}</span></div>
            <div className="kv-row"><span className="kv-label">Qualità input</span><span className="kv-val">{JSON.stringify(activeDetail.telemetry?.quality ?? {})}</span></div>
            <div className="kv-row"><span className="kv-label">Candidato</span><span className="kv-val">{fmtWatts(activeDetail.candidate_w, "W")}</span></div>
            <div className="kv-row"><span className="kv-label">Safety-limited</span><span className="kv-val">{fmtWatts(activeDetail.limited_w, "W")}</span></div>
            <div className="kv-row"><span className="kv-label">Limiti Safety [min / max]</span><span className="kv-val">{activeDetail.safety_min_w ?? "—"} / {activeDetail.safety_max_w ?? "—"} W</span></div>
            <div className="kv-row"><span className="kv-label">Setpoint simulato</span><span className="kv-val" style={{ color: "var(--accent-teal)" }}>{fmtWatts(activeDetail.simulated_w, "W")}</span></div>
            <div className="kv-row"><span className="kv-label">Motivo / Safety</span><span className="kv-val">{activeDetail.reason} · {activeDetail.safety_reason}</span></div>
            <div className="kv-row"><span className="kv-label">executed / applied</span><span className="kv-val" style={{ color: "var(--shadow-blue)" }}>executed=false · applied=null</span></div>
            <div className="kv-row"><span className="kv-label">modbus_write_count / AUX1</span><span className="kv-val">0 · {activeDetail.aux1 ?? "OFF"}</span></div>
            <div style={{ marginTop: 14 }}>
              <div style={{ fontSize: 12, color: "var(--text-secondary)", marginBottom: 6 }}>Input Home Assistant nel pacchetto:</div>
              <pre style={{ background: "var(--bg-inset)", padding: 10, borderRadius: 8, fontSize: 11, overflowX: "auto" }}>
                {JSON.stringify(activeDetail.ha_inputs ?? {}, null, 2)}
              </pre>
            </div>
          </aside>
        </div>
      )}
    </>
  );
}
