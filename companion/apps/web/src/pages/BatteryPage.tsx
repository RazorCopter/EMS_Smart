import { useMemo, useState } from "react";
import { useQuery } from "@tanstack/react-query";
import type { EChartsOption } from "echarts";
import { apiFetch } from "../lib/api";
import { fmtKwh, fmtNum, fmtPct, fmtTime, fmtWatts, type TimeRange } from "../lib/format";
import { EChart } from "../components/EChart";

function ProvBadge({ provenance }: { provenance: string }) {
  const labels: Record<string, string> = {
    measured: "Misurato",
    derived: "Derivato",
    configured: "Configurato",
    provisional: "Provvisorio",
    unavailable: "Non disponibile",
  };
  return <span className={`badge ${provenance}`}>{labels[provenance] ?? provenance}</span>;
}

export function BatteryPage() {
  const [range, setRange] = useState<TimeRange>("today");
  const { data, isLoading } = useQuery({
    queryKey: ["battery", range],
    queryFn: () => apiFetch<any>(`/api/v1/battery?range=${range}`),
    refetchInterval: 10000,
  });

  const hist: any[] = data?.history ?? [];

  const chartOption: EChartsOption = useMemo(() => {
    return {
      tooltip: { trigger: "axis", confine: true },
      legend: { top: 0, textStyle: { color: "#98b0ad", fontSize: 11 } },
      grid: { top: 38, right: 48, bottom: 28, left: 48 },
      xAxis: {
        type: "category",
        data: hist.map((p) => new Date(p.timestamp).toLocaleTimeString("it-IT", { timeZone: "Europe/Rome", hour: "2-digit", minute: "2-digit" })),
        axisLabel: { color: "#6e8683", fontSize: 10 },
      },
      yAxis: [
        { type: "value", name: "SOC %", min: 0, max: 100, axisLabel: { color: "#6e8683", formatter: "{value}%" }, splitLine: { lineStyle: { color: "#1f3134" } } },
        { type: "value", name: "W", axisLabel: { color: "#6e8683" }, splitLine: { show: false } },
      ],
      series: [
        { name: "SOC %", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: "#34d399", width: 2.2 }, areaStyle: { color: "rgba(52,211,153,0.14)" }, data: hist.map((p) => p.soc) },
        { name: "Potenza (W)", type: "line", yAxisIndex: 1, showSymbol: false, connectNulls: false, lineStyle: { color: "#60a5fa", width: 1.8 }, data: hist.map((p) => p.battery_w) },
      ],
    };
  }, [hist]);

  if (isLoading || !data) {
    return <div className="card">Caricamento dati batteria...</div>;
  }

  const live = data.live ?? {};
  const specs = data.specifications ?? {};
  const limits = data.limits?.configured ?? {};
  const stats = data.stats ?? {};

  return (
    <>
      <header className="topbar">
        <div>
          <h1 className="page-title">Batteria &amp; Stato Accumulo</h1>
          <p className="page-sub">
            Capacità {specs.capacity_wh?.value ? `${(specs.capacity_wh.value / 1000).toFixed(2)} kWh` : "10,24 kWh"} ·{" "}
            Tensione nominale {specs.nominal_v?.value ?? 51.2} V · Provenienza esplicita su ogni grandezza
          </p>
        </div>
        <div className="topbar-right">
          <div className="seg-group">
            {(["today", "yesterday", "7d", "30d"] as TimeRange[]).map((r) => (
              <button key={r} type="button" className={`seg-btn ${range === r ? "active" : ""}`} onClick={() => setRange(r)}>
                {r === "today" ? "Oggi" : r === "yesterday" ? "Ieri" : r}
              </button>
            ))}
          </div>
        </div>
      </header>

      <section className="grid-4">
        <article className="kpi-card">
          <div style={{ display: "flex", justifyContent: "space-between" }}><span className="kv-label">SOC Live</span><ProvBadge provenance={live.soc?.provenance ?? "unavailable"} /></div>
          <div className="kpi-val" style={{ color: "var(--battery-color)" }}>{fmtPct(live.soc?.value)}</div>
          <span className="card-sub">Min {fmtPct(stats.soc_min)} · Max {fmtPct(stats.soc_max)} · Media {fmtPct(stats.soc_avg)}</span>
        </article>
        <article className="kpi-card">
          <div style={{ display: "flex", justifyContent: "space-between" }}><span className="kv-label">Potenza / V / A</span><ProvBadge provenance={live.power_w?.provenance ?? "unavailable"} /></div>
          <div className="kpi-val">{fmtWatts(live.power_w?.value, "W")}</div>
          <span className="card-sub">{fmtNum(live.voltage_v?.value, "V", 1)} · {fmtNum(live.current_a?.value, "A", 1)}</span>
        </article>
        <article className="kpi-card">
          <div style={{ display: "flex", justifyContent: "space-between" }}><span className="kv-label">Throughput &amp; Cicli</span><ProvBadge provenance="derived" /></div>
          <div className="kpi-val">{fmtKwh(stats.throughput_kwh)}</div>
          <span className="card-sub">Cicli equivalenti: {fmtNum(stats.equivalent_cycles, "", 3)}</span>
        </article>
        <article className="kpi-card">
          <div style={{ display: "flex", justifyContent: "space-between" }}><span className="kv-label">SOH &amp; Temperatura</span><ProvBadge provenance="unavailable" /></div>
          <div className="kpi-val" style={{ color: "var(--missing-grey)", fontSize: 18 }}>Non disponibile</div>
          <span className="card-sub">Registri SOH/temp batteria non ancora qualificati su FC04</span>
        </article>
      </section>

      <section className="card">
        <div className="card-header"><h2 className="card-title">Andamento Storico SOC &amp; Potenza Batteria</h2></div>
        <EChart option={chartOption} height={280} ariaLabel="Grafico storico SOC e potenza batteria" />
      </section>

      <section className="grid-3">
        <article className="card">
          <div className="card-header"><h2 className="card-title">Specifiche &amp; Limiti</h2><ProvBadge provenance="configured" /></div>
          <div className="kv-row"><span className="kv-label">Profilo</span><span className="kv-val">{specs.model?.value}</span></div>
          <div className="kv-row"><span className="kv-label">Capacità utile</span><span className="kv-val">{specs.capacity_wh?.value} Wh</span></div>
          <div className="kv-row"><span className="kv-label">Tensione nominale</span><span className="kv-val">{specs.nominal_v?.value} V</span></div>
          <div className="kv-row"><span className="kv-label">Efficienza modello</span><span className="kv-val">{specs.efficiency_pct?.value}% <ProvBadge provenance="provisional" /></span></div>
          <div className="kv-row"><span className="kv-label">Limiti SOC (min / max)</span><span className="kv-val">{limits.soc_min}% / {limits.soc_max}%</span></div>
          <div className="kv-row"><span className="kv-label">Limiti W (carica / scarica)</span><span className="kv-val">{limits.charge_w} W / {limits.discharge_w} W</span></div>
          <div className="kv-row"><span className="kv-label">Limiti A (carica / scarica)</span><span className="kv-val">{limits.charge_a} A / {limits.discharge_a} A</span></div>
        </article>

        <article className="card">
          <div className="card-header"><h2 className="card-title">Sessioni Target SOC &amp; Maintenance</h2></div>
          {(data.sessions ?? []).length === 0 ? (
            <p className="card-sub">Nessuna sessione Target SOC o Maintenance nell&apos;intervallo.</p>
          ) : (
            (data.sessions ?? []).map((s: any) => (
              <div key={s.id} className="kv-row">
                <span className="kv-label">{s.strategy} ({s.state})</span>
                <span className="kv-val">{fmtTime(s.started_at)} · {Math.round(s.duration_s / 60)} min</span>
              </div>
            ))
          )}
        </article>

        <article className="card">
          <div className="card-header"><h2 className="card-title">Eventi Riserva SOC</h2></div>
          {(data.reserve_events ?? []).length === 0 ? (
            <p className="card-sub">Nessun evento riserva SOC registrato nell&apos;intervallo.</p>
          ) : (
            (data.reserve_events ?? []).map((ev: any) => (
              <div key={ev.id} className="kv-row">
                <span className="kv-label">{fmtTime(ev.occurred_at)}</span>
                <span className="kv-val">{ev.title}</span>
              </div>
            ))
          )}
        </article>
      </section>
    </>
  );
}
