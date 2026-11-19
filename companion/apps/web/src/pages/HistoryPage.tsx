import { useMemo, useRef, useState } from "react";
import { useQuery } from "@tanstack/react-query";
import type { EChartsOption } from "echarts";
import type * as echarts from "echarts";
import { Download, ImageDown } from "lucide-react";
import { apiFetch, rangeQuery } from "../lib/api";
import { fmtNum, fmtWatts, strategyLabel, type TimeRange } from "../lib/format";
import { EChart } from "../components/EChart";
import { ShadowBanner } from "../components/ShadowBanner";
import { PageHeader } from "../components/shared/PageHeader";
import { useTheme } from "../theme/useTheme";

const RANGES: Array<{ id: TimeRange; label: string }> = [
  { id: "today", label: "Oggi" },
  { id: "yesterday", label: "Ieri" },
  { id: "7d", label: "7 giorni" },
  { id: "30d", label: "30 giorni" },
  { id: "12m", label: "12 mesi" },
  { id: "custom", label: "Personalizzato" },
];

export function HistoryPage() {
  const { chartPalette } = useTheme();
  const [range, setRange] = useState<TimeRange>("today");
  const [customStart, setCustomStart] = useState("");
  const [customEnd, setCustomEnd] = useState("");
  const chartInstanceRef = useRef<echarts.ECharts | null>(null);

  const qs = rangeQuery(range, customStart, customEnd);
  const { data, isLoading, error } = useQuery({
    queryKey: ["history", qs],
    queryFn: () => apiFetch<any>(`/api/v1/history?${qs}`),
    enabled: range !== "custom" || (Boolean(customStart) && Boolean(customEnd)),
  });

  const { data: kpiData } = useQuery({
    queryKey: ["kpi", qs],
    queryFn: () => apiFetch<any>(`/api/v1/kpi?${qs}`),
    enabled: range !== "custom" || (Boolean(customStart) && Boolean(customEnd)),
  });

  const series: any[] = data?.series ?? [];

  const option: EChartsOption = useMemo(() => {
    const times = series.map((p) => p.timestamp);
    return {
      tooltip: {
        trigger: "axis",
        confine: true,
        backgroundColor: chartPalette.tooltipBg,
        borderColor: chartPalette.tooltipBorder,
        textStyle: { color: chartPalette.textPrimary, fontSize: 12 },
        formatter: (params: any) => {
          const idx = params?.[0]?.dataIndex;
          const pt = series[idx];
          if (!pt) return "";
          const dt = new Date(pt.timestamp).toLocaleString("it-IT", { timeZone: "Europe/Rome" });
          const battDir =
            pt.battery_w == null ? "" : pt.battery_w > 20 ? ", carica" : pt.battery_w < -20 ? ", scarica" : "";
          const gridDir =
            pt.grid_w == null ? "" : pt.grid_w > 20 ? ", importazione" : pt.grid_w < -20 ? ", esportazione" : "";
          return [
            `<strong>${dt}</strong>`,
            `SOC: <b>${pt.soc != null ? `${pt.soc}%` : "Dato mancante"}</b>`,
            `<span style="color:${chartPalette.pv}">Produzione:</span> <b>${fmtWatts(pt.pv_w)}</b>`,
            `<span style="color:${chartPalette.home}">Consumo (der.):</span> <b>${fmtWatts(pt.load_w)}</b>`,
            `<span style="color:${chartPalette.battery}">Batteria:</span> <b>${fmtWatts(pt.battery_w)}${battDir}</b>`,
            `<span style="color:${chartPalette.grid}">Rete:</span> <b>${fmtWatts(pt.grid_w)}${gridDir}</b>`,
            `Strategia: <b>${strategyLabel(pt.strategy)}</b>`,
            `Setpoint ESP: <b>${fmtWatts(pt.simulated_w)} (simulato)</b>`,
            `Decisione SEM: <b>${pt.sem_decision ?? "Non disponibile"}</b>`,
            `Safety: <b>${pt.safety_reason && pt.safety_reason !== "NONE" ? pt.safety_reason : "Nessuna limitazione"}</b>`,
            `Modalità: <b>SHADOW</b> · Qualità: <b>${pt.quality === "valid" ? "valida" : "dato mancante"}</b>`,
          ].join("<br/>");
        },
      },
      legend: {
        top: 0,
        textStyle: { color: chartPalette.textSecondary, fontSize: 11 },
      },
      dataZoom: [
        { type: "inside", start: 0, end: 100 },
        { type: "slider", bottom: 4, height: 20, borderColor: chartPalette.sliderBorder },
      ],
      grid: { top: 54, right: 52, bottom: 52, left: 56 },
      xAxis: {
        type: "category",
        data: times.map((t) =>
          new Date(t).toLocaleTimeString("it-IT", {
            timeZone: "Europe/Rome",
            hour: "2-digit",
            minute: "2-digit",
            ...(range === "7d" || range === "30d" || range === "12m" ? { day: "2-digit", month: "2-digit" } : {}),
          })
        ),
        axisLabel: { color: chartPalette.axisLabel, fontSize: 11 },
      },
      yAxis: [
        {
          type: "value",
          name: "kW",
          nameTextStyle: { color: chartPalette.axisLabel },
          axisLabel: { color: chartPalette.axisLabel, formatter: (v: number) => `${(v / 1000).toFixed(1)}` },
          splitLine: { lineStyle: { color: chartPalette.splitLine } },
        },
        {
          type: "value",
          name: "SOC %",
          min: 0,
          max: 100,
          nameTextStyle: { color: chartPalette.axisLabel },
          axisLabel: { color: chartPalette.axisLabel, formatter: "{value}%" },
          splitLine: { show: false },
        },
      ],
      series: [
        { name: "FV", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.pv, width: 2 }, itemStyle: { color: chartPalette.pv }, areaStyle: { color: chartPalette.pvArea }, data: series.map((p) => p.pv_w) },
        { name: "Consumo", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.home, width: 2 }, itemStyle: { color: chartPalette.home }, data: series.map((p) => p.load_w) },
        { name: "Batteria", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.battery, width: 2 }, itemStyle: { color: chartPalette.battery }, data: series.map((p) => p.battery_w) },
        { name: "Rete", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.grid, width: 1.8 }, itemStyle: { color: chartPalette.grid }, data: series.map((p) => p.grid_w) },
        { name: "Setpoint ESP (sim.)", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.shadow, width: 1.6, type: "dashed" }, itemStyle: { color: chartPalette.shadow }, data: series.map((p) => p.simulated_w) },
        { name: "Limite Safety Max", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.safety, width: 1, type: "dotted" }, itemStyle: { color: chartPalette.safety }, data: series.map((p) => p.safety_max_w) },
        { name: "Limite Safety Min", type: "line", showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.safety, width: 1, type: "dotted" }, itemStyle: { color: chartPalette.safety }, data: series.map((p) => p.safety_min_w) },
        { name: "SOC", type: "line", yAxisIndex: 1, showSymbol: false, connectNulls: false, lineStyle: { color: chartPalette.soc, width: 2, type: "dashed" }, itemStyle: { color: chartPalette.soc }, data: series.map((p) => p.soc) },
      ],
    };
  }, [series, range, chartPalette]);

  function saveImage() {
    const url = chartInstanceRef.current?.getDataURL({ type: "png", backgroundColor: chartPalette.canvasBg });
    if (!url) return;
    const a = document.createElement("a");
    a.href = url;
    a.download = `afore-storico-${range}.png`;
    a.click();
  }

  const en = kpiData?.energy ?? {};

  return (
    <>
      <PageHeader
        title="Storico Energetico"
        subtitle={`Aggregazione: ${data?.bucket ?? "auto"} · Bucket mancanti: ${data?.missing_buckets ?? 0} (mai sostituiti con 0)`}
        actions={
          <div className="seg-group" role="group" aria-label="Intervallo temporale">
            {RANGES.map((r) => (
              <button
                key={r.id}
                type="button"
                className={`seg-btn ${range === r.id ? "active" : ""}`}
                onClick={() => setRange(r.id)}
              >
                {r.label}
              </button>
            ))}
          </div>
        }
      />

      {range === "custom" && (
        <section className="card" style={{ display: "flex", gap: 12, flexWrap: "wrap", alignItems: "end" }}>
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

      <section className="card">
        <div className="card-header">
          <div>
            <h2 className="card-title">Potenza, SOC &amp; Setpoint Simulato</h2>
            <p className="card-sub">Zoom con rotellina o slider · Click legenda per filtrare le serie</p>
          </div>
          <div style={{ display: "flex", gap: 8, flexWrap: "wrap" }}>
            <a className="btn" href={`/api/v1/export?format=csv&scope=history&${qs}`}>
              <Download size={14} /> CSV
            </a>
            <a className="btn" href={`/api/v1/export?format=json&scope=history&${qs}`}>
              <Download size={14} /> JSON
            </a>
            <button type="button" className="btn" onClick={saveImage}>
              <ImageDown size={14} /> PNG
            </button>
          </div>
        </div>

        {isLoading ? (
          <p className="card-sub">Caricamento serie storica...</p>
        ) : error ? (
          <p role="alert" style={{ color: "var(--fault-red)" }}>Errore caricamento storico.</p>
        ) : series.length === 0 ? (
          <p className="card-sub">Nessun campione registrato nell&apos;intervallo selezionato.</p>
        ) : (
          <EChart
            option={option}
            height={380}
            ariaLabel="Grafico storico produzione, consumo, batteria, rete, setpoint simulato e SOC"
            onChartReady={(inst) => {
              chartInstanceRef.current = inst;
            }}
          />
        )}
      </section>

      <section className="grid-4">
        <article className="kpi-card">
          <span className="kv-label">Produzione FV</span>
          <div className="kpi-val" style={{ color: "var(--pv-color)" }}>{fmtNum(en.pv_kwh, "kWh", 2)}</div>
        </article>
        <article className="kpi-card">
          <span className="kv-label">Consumo casa (derivato)</span>
          <div className="kpi-val" style={{ color: "var(--home-color)" }}>{fmtNum(en.load_kwh, "kWh", 2)}</div>
        </article>
        <article className="kpi-card">
          <span className="kv-label">Autoconsumo / Autosufficienza</span>
          <div className="kpi-val" style={{ color: "var(--battery-color)" }}>
            {en.self_consumption_pct != null ? `${en.self_consumption_pct}%` : "—"} /{" "}
            {en.self_sufficiency_pct != null ? `${en.self_sufficiency_pct}%` : "—"}
          </div>
        </article>
        <article className="kpi-card">
          <span className="kv-label">Disponibilità telemetria</span>
          <div className="kpi-val">{en.data_availability_pct != null ? `${en.data_availability_pct}%` : "—"}</div>
        </article>
      </section>
    </>
  );
}
