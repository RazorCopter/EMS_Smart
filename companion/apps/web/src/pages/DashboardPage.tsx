import { useMemo, useState } from "react";
import { useQuery } from "@tanstack/react-query";
import { Link } from "react-router-dom";
import type { EChartsOption } from "echarts";
import { Info } from "lucide-react";
import { apiFetch } from "../lib/api";
import {
  fmtAge,
  fmtDurationCompact,
  fmtKwh,
  fmtNum,
  fmtPct,
  fmtShortTime,
  fmtTime,
  fmtWatts,
  humanDecisionDetail,
  strategyLabel,
} from "../lib/format";
import { EChart } from "../components/EChart";
import { PowerFlow } from "../components/PowerFlow";
import { ShadowBanner } from "../components/ShadowBanner";

type LiveChartRange = "1h" | "3h" | "6h" | "12h" | "today";

interface LiveResponse {
  generated_at: string;
  timezone: string;
  telemetry: any;
  decision: any;
  kpi_today: any;
  kpi_yesterday: any;
  comparison_yesterday?: { available: boolean; reason: string };
  coverage?: {
    first_sample_at: string | null;
    effective_monitor_start_at: string | null;
    monitoring_started_today: boolean;
    unmonitored_pre_install_seconds: number;
    monitored_window_seconds: number;
    valid_seconds: number;
    gap_seconds: number;
    invalid_samples: number;
    unavailable_seconds: number;
    coverage_since_first_sample_pct: number | null;
    legacy_stride_gaps_excluded?: number;
  };
  sparklines: Array<{ hour: string; pv_kwh: number | null; load_kwh: number | null; chg_kwh: number | null; imp_kwh: number | null }>;
  system: { components: Record<string, { status: string; recorded_at: string | null; detail: Record<string, unknown> }> };
  grouped_events?: Array<{
    key: string;
    id: number;
    occurred_at: string;
    event_type: string;
    severity: string;
    title: string;
    detail: string | null;
    count: number;
  }>;
  recent_events: Array<{ id: number; occurred_at: string; event_type: string; severity: string; title: string; detail: string | null }>;
}

const CHART_RANGE_TITLES: Record<LiveChartRange, string> = {
  "1h": "Andamento ultima ora",
  "3h": "Andamento ultime 3 ore",
  "6h": "Andamento ultime 6 ore",
  "12h": "Andamento ultime 12 ore",
  today: "Andamento di oggi",
};

const SYSTEM_LABELS: Array<[string, string]> = [
  ["esp32", "ESP32"],
  ["mqtt", "MQTT"],
  ["home_assistant", "Home Assistant"],
  ["modbus", "Modbus"],
  ["ntp", "NTP"],
  ["collector", "Collector"],
  ["database", "Database"],
  ["aux1", "AUX1"],
];

function SemiGauge({ value, color, label }: { value: number | null | undefined; color: string; label: string }) {
  const clamped = value != null && Number.isFinite(value) ? Math.max(0, Math.min(100, value)) : 0;
  const r = 36;
  const halfCirc = Math.PI * r;
  const dash = (clamped / 100) * halfCirc;
  return (
    <svg width="96" height="58" viewBox="0 0 96 58" role="img" aria-label={`${label}: ${value != null ? `${clamped.toFixed(1)}%` : "Non calcolabile"}`}>
      <path
        d="M 12 48 A 36 36 0 0 1 84 48"
        fill="none"
        stroke="var(--border-strong)"
        strokeWidth="9"
        strokeLinecap="round"
        opacity="0.45"
      />
      {value != null && Number.isFinite(value) && (
        <path
          d="M 12 48 A 36 36 0 0 1 84 48"
          fill="none"
          stroke={color}
          strokeWidth="9"
          strokeLinecap="round"
          strokeDasharray={`${dash.toFixed(1)} ${halfCirc.toFixed(1)}`}
        />
      )}
    </svg>
  );
}

function FormulaTip({ id, title, formula }: { id: string; title: string; formula: string }) {
  const [open, setOpen] = useState(false);
  return (
    <span className="tip-wrap">
      <button
        type="button"
        className="tip-btn"
        aria-label={`Mostra formula ${title}`}
        aria-expanded={open}
        aria-controls={id}
        onClick={() => setOpen((v) => !v)}
        onBlur={() => setOpen(false)}
      >
        <Info size={14} aria-hidden="true" />
      </button>
      {open && (
        <span id={id} role="tooltip" className="tip-bubble">
          <strong>Formula {title}:</strong> {formula}
        </span>
      )}
    </span>
  );
}

function RealSparkline({
  points,
  color,
}: {
  points: Array<{ hour: string; value: number | null }>;
  color: string;
}) {
  if (!points.length) {
    return <div className="spark-empty">In attesa dei primi bucket orari</div>;
  }
  const clean = points.slice(-12);
  const max = Math.max(0.05, ...clean.map((p) => p.value ?? 0));
  const width = 180;
  const barSlot = Math.floor(width / Math.max(clean.length, 6));
  const barW = Math.max(8, Math.min(18, barSlot - 4));
  const offsetX = width - clean.length * barSlot;

  return (
    <svg viewBox={`0 0 ${width} 26`} height="24" width="100%" aria-hidden="true">
      {clean.map((p, idx) => {
        const h = p.value == null ? 2 : Math.max(3, Math.round((p.value / max) * 22));
        return (
          <rect
            key={`${p.hour}-${idx}`}
            x={offsetX + idx * barSlot + 2}
            y={24 - h}
            width={barW}
            height={h}
            rx="2.5"
            fill={p.value == null ? "var(--missing-grey)" : color}
            opacity={p.value == null ? 0.3 : 0.9}
          >
            <title>{`${p.hour}: ${p.value != null ? `${p.value.toFixed(2)} kWh` : "Non disponibile"}`}</title>
          </rect>
        );
      })}
    </svg>
  );
}

function comparisonCopy(
  today: number | null | undefined,
  yesterday: number | null | undefined,
  yesterdayAvailable: boolean,
): string {
  if (!yesterdayAvailable) return "Confronto disponibile da domani";
  if (today == null || yesterday == null || yesterday <= 0.05) {
    return "Nessun periodo precedente confrontabile";
  }
  const pct = ((today - yesterday) / yesterday) * 100;
  const sign = pct >= 0 ? "+" : "";
  return `${sign}${pct.toFixed(0)}% rispetto a ieri`;
}

function formatStartedAtLabel(iso: string | null | undefined, startedToday: boolean, tz: string): string {
  if (!iso) return "In attesa del primo campione";
  const hm = fmtShortTime(iso, tz);
  if (startedToday) return `Monitoraggio iniziato oggi alle ${hm}`;
  const d = new Date(iso).toLocaleDateString("it-IT", { timeZone: tz, day: "2-digit", month: "2-digit" });
  return `Monitoraggio attivo dal ${d} alle ${hm}`;
}

export function DashboardPage() {
  const [chartRange, setChartRange] = useState<LiveChartRange>("6h");

  const { data: live, isLoading, error } = useQuery({
    queryKey: ["live"],
    queryFn: () => apiFetch<LiveResponse>("/api/v1/live"),
    refetchInterval: 5000,
  });

  const { data: hist } = useQuery({
    queryKey: ["history-live-compact", chartRange],
    queryFn: () => apiFetch<{ series: any[] }>(`/api/v1/history?range=${chartRange}&bucket=auto`),
    refetchInterval: 30000,
  });

  const isLight = typeof document !== "undefined" && document.documentElement.dataset.theme === "light";

  const compactChartOption: EChartsOption = useMemo(() => {
    const s = hist?.series ?? [];
    const axisColor = isLight ? "#5a7471" : "#7c9491";
    const splitColor = isLight ? "#d5e1df" : "#1f3134";
    const socLineColor = isLight ? "#0f201e" : "#eef5f3";
    return {
      grid: { top: 34, right: 44, bottom: 26, left: 46 },
      tooltip: { trigger: "axis", confine: true },
      legend: {
        top: 0,
        left: 0,
        itemWidth: 12,
        itemHeight: 8,
        textStyle: { color: axisColor, fontSize: 11.5 },
      },
      xAxis: {
        type: "category",
        data: s.map((p) => {
          const d = new Date(p.timestamp);
          return d.toLocaleTimeString("it-IT", { timeZone: live?.timezone ?? "Europe/Rome", hour: "2-digit", minute: "2-digit" });
        }),
        axisLabel: { color: axisColor, fontSize: 10.5 },
      },
      yAxis: [
        {
          type: "value",
          axisLabel: {
            color: axisColor,
            fontSize: 10.5,
            formatter: (v: number) => (Math.abs(v) >= 1000 ? `${(v / 1000).toFixed(1)}k` : `${Math.round(v)} W`),
          },
          splitLine: { lineStyle: { color: splitColor } },
        },
        {
          type: "value",
          min: 0,
          max: 100,
          axisLabel: { color: axisColor, fontSize: 10.5, formatter: "{value}%" },
          splitLine: { show: false },
        },
      ],
      series: [
        {
          name: "FV",
          type: "line",
          showSymbol: false,
          smooth: 0.2,
          connectNulls: false,
          lineStyle: { color: "#f5b942", width: 2.2 },
          itemStyle: { color: "#f5b942" },
          areaStyle: { color: "rgba(245,185,66,0.12)" },
          data: s.map((p) => p.pv_w),
        },
        {
          name: "Casa",
          type: "line",
          showSymbol: false,
          smooth: 0.2,
          connectNulls: false,
          lineStyle: { color: "#a78bfa", width: 2 },
          itemStyle: { color: "#a78bfa" },
          data: s.map((p) => p.load_w),
        },
        {
          name: "Batteria",
          type: "line",
          showSymbol: false,
          smooth: 0.2,
          connectNulls: false,
          lineStyle: { color: "#34d399", width: 2 },
          itemStyle: { color: "#34d399" },
          data: s.map((p) => p.battery_w),
        },
        {
          name: "Rete",
          type: "line",
          showSymbol: false,
          smooth: 0.2,
          connectNulls: false,
          lineStyle: { color: "#60a5fa", width: 1.8 },
          itemStyle: { color: "#60a5fa" },
          data: s.map((p) => p.grid_w),
        },
        {
          name: "SOC",
          type: "line",
          yAxisIndex: 1,
          showSymbol: false,
          connectNulls: false,
          lineStyle: { color: socLineColor, width: 1.6, type: "dashed" },
          itemStyle: { color: socLineColor },
          data: s.map((p) => p.soc),
        },
      ],
    };
  }, [hist, isLight, live?.timezone]);

  if (isLoading) {
    return <div className="card">Caricamento telemetria live...</div>;
  }

  if (error || !live) {
    return (
      <div className="card" role="alert">
        Impossibile caricare la Dashboard Live: {error instanceof Error ? error.message : "Errore sconosciuto"}
      </div>
    );
  }

  const tel = live.telemetry;
  const dec = live.decision;
  const kpi = live.kpi_today ?? {};
  const yest = live.kpi_yesterday ?? {};
  const yestAvail = Boolean(live.comparison_yesterday?.available);
  const cov = live.coverage;
  const sparks = live.sparklines ?? [];
  const comps = live.system?.components ?? {};
  const eventsTop3 = (live.grouped_events ?? live.recent_events ?? []).slice(0, 3);

  const telStateClass = !tel ? "warn" : tel.stale ? "warn" : tel.fault ? "err" : "ok";
  const covPct = cov?.coverage_since_first_sample_pct ?? kpi.data_availability_pct;
  const decisionReason = humanDecisionDetail(
    dec?.safety_reason,
    dec?.reason,
    dec?.soc ?? tel?.soc,
    dec?.safety_min_w,
    dec?.safety_max_w,
  );

  const pvNotCalculable = kpi.pv_kwh == null || kpi.pv_kwh <= 0.001;
  const loadNotCalculable = kpi.load_kwh == null || kpi.load_kwh <= 0.001;
  const socPct = tel?.soc ?? dec?.soc ?? null;
  const decDetailUrl = dec?.id ? `/decisioni?id=${encodeURIComponent(dec.id)}` : "/decisioni";

  return (
    <>
      <header className="topbar">
        <div>
          <h1 className="page-title">Dashboard Live</h1>
          <p className="page-sub">
            Monitoraggio in tempo reale · {fmtTime(live.generated_at, live.timezone)} · {live.timezone}
          </p>
        </div>
        <div className="topbar-right">
          <span className={`chip ${telStateClass}`}>
            {!tel
              ? "● In attesa di telemetria"
              : tel.stale
              ? `● Dati non aggiornati (${fmtAge(tel.age_s)})`
              : `● Telemetria live · ${fmtAge(tel.age_s)}`}
          </span>
          {covPct != null && (
            <span className="chip ok">
              ● Copertura dal primo campione: {fmtPct(covPct)}
            </span>
          )}
          <span className="chip shadow">Modalità SHADOW</span>
        </div>
      </header>

      {/* HERO: 65% Energy Monitor + 35% Decisione EMS */}
      <section className="grid-hero-v2">
        <article className="card">
          <PowerFlow
            telemetry={tel}
            safetyMinW={dec?.safety_min_w}
            safetyMaxW={dec?.safety_max_w}
            selfConsumptionPct={kpi.self_consumption_pct}
          />
        </article>

        <article className="card decision-hero-card">
          <div>
            <div className="card-header">
              <div>
                <h2 className="card-title">Decisione EMS</h2>
                <p className="card-sub">Valutazione autonoma locale</p>
              </div>
              <span className="chip shadow">SHADOW</span>
            </div>

            <ShadowBanner compact />

            <div className="kv-row" style={{ marginTop: 10 }}>
              <span className="kv-label">Strategia locale</span>
              <span className="kv-val">{strategyLabel(dec?.configured_strategy)}</span>
            </div>
            <div className="kv-row">
              <span className="kv-label">Strategia effettiva</span>
              <span className="kv-val">{strategyLabel(dec?.strategy)}</span>
            </div>

            <div className={`reason-callout ${decisionReason.tone}`}>
              <div className="reason-callout-kicker">Motivo corrente</div>
              <div className="reason-callout-title">{decisionReason.title}</div>
              <div className="reason-callout-sub">{decisionReason.subtitle}</div>
            </div>

            <div className="pipeline-3" aria-label="Pipeline setpoint SHADOW">
              <div className="pipe-box">
                <span className="pipe-lbl">Candidato</span>
                <strong className="pipe-val">{fmtWatts(dec?.candidate_w, "W")}</strong>
              </div>
              <div className="pipe-box">
                <span className="pipe-lbl">Safety-limited</span>
                <strong className="pipe-val" style={{ color: "var(--safety-amber)" }}>
                  {fmtWatts(dec?.limited_w, "W")}
                </strong>
              </div>
              <div className="pipe-box">
                <span className="pipe-lbl">Simulato</span>
                <strong className="pipe-val" style={{ color: "var(--accent-teal)" }}>
                  {fmtWatts(dec?.simulated_w, "W")}
                </strong>
              </div>
            </div>

            <div className="kv-row">
              <span className="kv-label">Limiti Safety attivi</span>
              <span className="kv-val">
                {dec?.safety_min_w != null && dec?.safety_max_w != null
                  ? `${Math.round(dec.safety_min_w)} / ${Math.round(dec.safety_max_w)} W`
                  : "Non disponibile"}
              </span>
            </div>
            <div className="kv-row">
              <span className="kv-label">Ultima decisione {dec ? `(#${dec.seq})` : ""}</span>
              <span className="kv-val">{dec ? fmtTime(dec.source_timestamp, live.timezone) : "Non disponibile"}</span>
            </div>
          </div>

          <Link to={decDetailUrl} className="decision-cta-link">
            Apri dettaglio decisione →
          </Link>
        </article>
      </section>

      {/* PRIMARY KPI ROW: Autoconsumo, Autosufficienza, Batteria */}
      <section className="kpi-primary-grid" aria-label="Indicatori chiave giornalieri">
        <article className="kpi-hero-card">
          <div className="kpi-hero-copy">
            <div className="kpi-head-row">
              <span className="kpi-title-lbl">Autoconsumo</span>
              <FormulaTip
                id="tip-autoconsumo"
                title="Autoconsumo"
                formula={kpi.formulas?.self_consumption ?? "FV consumato localmente / Produzione FV"}
              />
            </div>
            <div className="kpi-hero-val" style={{ color: pvNotCalculable ? "var(--missing-grey)" : "var(--pv-color)" }}>
              {pvNotCalculable ? "Non calcolabile" : fmtPct(kpi.self_consumption_pct)}
            </div>
            <div className="kpi-hero-sub">
              {pvNotCalculable
                ? "Produzione fotovoltaica ancora nulla oggi"
                : `${fmtKwh(kpi.self_consumed_pv_kwh)} su ${fmtKwh(kpi.pv_kwh)} prodotti`}
            </div>
          </div>
          <SemiGauge
            value={pvNotCalculable ? null : kpi.self_consumption_pct}
            color="var(--pv-color)"
            label="Autoconsumo"
          />
        </article>

        <article className="kpi-hero-card">
          <div className="kpi-hero-copy">
            <div className="kpi-head-row">
              <span className="kpi-title-lbl">Autosufficienza</span>
              <FormulaTip
                id="tip-autosuff"
                title="Autosufficienza"
                formula={kpi.formulas?.self_sufficiency ?? "(Consumo totale - Importazione rete) / Consumo totale"}
              />
            </div>
            <div className="kpi-hero-val" style={{ color: loadNotCalculable ? "var(--missing-grey)" : "var(--accent-teal)" }}>
              {loadNotCalculable ? "Non calcolabile" : fmtPct(kpi.self_sufficiency_pct)}
            </div>
            <div className="kpi-hero-sub">
              {kpi.grid_import_kwh != null && kpi.load_kwh != null
                ? `Rete: ${kpi.grid_import_kwh.toFixed(2)} kWh prelevati su ${kpi.load_kwh.toFixed(2)} kWh`
                : "Contributo rete non disponibile"}
            </div>
          </div>
          <SemiGauge
            value={loadNotCalculable ? null : kpi.self_sufficiency_pct}
            color="var(--accent-teal)"
            label="Autosufficienza"
          />
        </article>

        <article className="kpi-hero-card batt-summary">
          <div style={{ width: "100%" }}>
            <div className="kpi-head-row">
              <span className="kpi-title-lbl">Batteria</span>
              <span className="batt-soc-tag">
                SOC {socPct != null ? fmtPct(socPct) : "Non disponibile"}
              </span>
            </div>
            <div className="kpi-hero-val" style={{ color: "var(--battery-color)", fontSize: 23, marginTop: 4 }}>
              {kpi.battery_charge_kwh != null ? `+${kpi.battery_charge_kwh.toFixed(2)}` : "—"} /{" "}
              {kpi.battery_discharge_kwh != null ? `-${kpi.battery_discharge_kwh.toFixed(2)}` : "—"}{" "}
              <span className="kpi-unit-sm">kWh</span>
            </div>
            <div className="batt-meter-track" aria-hidden="true">
              <div
                className="batt-meter-fill"
                style={{ width: `${socPct != null ? Math.max(2, Math.min(100, socPct)) : 0}%` }}
              />
            </div>
            <div className="kpi-hero-sub" style={{ display: "flex", justifyContent: "space-between", gap: 8 }}>
              <span>Throughput {fmtKwh(kpi.battery_throughput_kwh)} · {kpi.equivalent_cycles ?? "—"} cicli</span>
              <span>{fmtNum(tel?.battery_v, "V", 1)}</span>
            </div>
          </div>
        </article>
      </section>

      {/* SECONDARY COMPACT DAILY KPI GRID */}
      <section className="kpi-compact-grid" aria-label="Totali energetici giornalieri">
        <article className="kpi-card">
          <div className="kpi-compact-head">
            <span>Produzione FV oggi</span>
            <span className="badge measured">Misurato</span>
          </div>
          <div className="kpi-val-lg" style={{ color: "var(--pv-color)" }}>
            {kpi.pv_kwh != null ? kpi.pv_kwh.toFixed(2) : "Non disp."}
            {kpi.pv_kwh != null && <span className="kpi-unit-sm"> kWh</span>}
          </div>
          <RealSparkline points={sparks.map((s) => ({ hour: s.hour, value: s.pv_kwh }))} color="var(--pv-color)" />
          <div className="kpi-compact-foot">{comparisonCopy(kpi.pv_kwh, yest.pv_kwh, yestAvail)}</div>
        </article>

        <article className="kpi-card">
          <div className="kpi-compact-head">
            <span>Consumo casa oggi</span>
            <span className="badge derived">Derivato</span>
          </div>
          <div className="kpi-val-lg" style={{ color: "var(--home-color)" }}>
            {kpi.load_kwh != null ? kpi.load_kwh.toFixed(2) : "Non disp."}
            {kpi.load_kwh != null && <span className="kpi-unit-sm"> kWh</span>}
          </div>
          <RealSparkline points={sparks.map((s) => ({ hour: s.hour, value: s.load_kwh }))} color="var(--home-color)" />
          <div className="kpi-compact-foot">{comparisonCopy(kpi.load_kwh, yest.load_kwh, yestAvail)}</div>
        </article>

        <article className="kpi-card">
          <div className="kpi-compact-head">
            <span>Importazione / Esportazione</span>
            <span className="badge measured">Misurato</span>
          </div>
          <div className="kpi-val-lg" style={{ color: "var(--grid-color)" }}>
            {kpi.grid_import_kwh != null ? kpi.grid_import_kwh.toFixed(2) : "—"}
            <span className="kpi-unit-sm"> imp / </span>
            {kpi.grid_export_kwh != null ? kpi.grid_export_kwh.toFixed(2) : "—"}
            <span className="kpi-unit-sm"> esp</span>
          </div>
          <RealSparkline points={sparks.map((s) => ({ hour: s.hour, value: s.imp_kwh }))} color="var(--grid-color)" />
          <div className="kpi-compact-foot">{comparisonCopy(kpi.grid_import_kwh, yest.grid_import_kwh, yestAvail)}</div>
        </article>

        <article className="kpi-card">
          <div className="kpi-compact-head">
            <span>Carica / Scarica batteria</span>
            <span className="badge measured">Misurato</span>
          </div>
          <div className="kpi-val-lg" style={{ color: "var(--battery-color)" }}>
            {kpi.battery_charge_kwh != null ? `+${kpi.battery_charge_kwh.toFixed(2)}` : "—"}
            <span className="kpi-unit-sm"> / </span>
            {kpi.battery_discharge_kwh != null ? `-${kpi.battery_discharge_kwh.toFixed(2)}` : "—"}
            <span className="kpi-unit-sm"> kWh</span>
          </div>
          <RealSparkline points={sparks.map((s) => ({ hour: s.hour, value: s.chg_kwh }))} color="var(--battery-color)" />
          <div className="kpi-compact-foot">
            {comparisonCopy(kpi.battery_charge_kwh, yest.battery_charge_kwh, yestAvail)}
          </div>
        </article>
      </section>

      {/* EXPLICIT COVERAGE & GAP STRIP */}
      <section className="coverage-strip" aria-label="Copertura e qualità acquisizione dati">
        <div className="coverage-main">
          <span className={`dot ${covPct != null && covPct >= 95 ? "online" : covPct == null ? "" : "degraded"}`} aria-hidden="true" />
          <strong>Copertura dal primo campione: {covPct != null ? fmtPct(covPct) : "Non disponibile"}</strong>
          <span className="coverage-sep">·</span>
          <span>{formatStartedAtLabel(cov?.first_sample_at, Boolean(cov?.monitoring_started_today), live.timezone)}</span>
        </div>
        <div className="coverage-badges">
          {cov?.monitoring_started_today && (
            <span className="chip" title="Periodo di oggi precedente all'installazione del Companion">
              Periodo precedente non monitorato ({fmtDurationCompact(cov.unmonitored_pre_install_seconds)})
            </span>
          )}
          <span className={`chip ${(cov?.gap_seconds ?? 0) > 0 ? "warn" : ""}`}>
            Gap dopo avvio: {fmtDurationCompact(cov?.gap_seconds ?? 0)}
          </span>
          <span className={`chip ${(cov?.invalid_samples ?? 0) > 0 ? "warn" : ""}`}>
            Invalidi: {cov?.invalid_samples ?? 0}
          </span>
          {(cov?.unavailable_seconds ?? 0) > 0 && (
            <span className="chip err">Unavailable: {fmtDurationCompact(cov?.unavailable_seconds)}</span>
          )}
          {(cov?.legacy_stride_gaps_excluded ?? 0) > 0 && (
            <span className="chip" title="Eventi DECISION_GAP precedenti al fix dello stride MQTT (esclusi dai KPI correnti senza eliminare lo storico)">
              {cov?.legacy_stride_gaps_excluded} gap legacy pre-fix esclusi
            </span>
          )}
        </div>
      </section>

      {/* COMPACT LIVE TREND CHART */}
      <section className="card">
        <div className="card-header">
          <div>
            <h3 className="card-title">{CHART_RANGE_TITLES[chartRange]}</h3>
            <p className="card-sub">FV, Casa, Batteria, Rete (W) e SOC (%) · Gap espliciti (nessun zero sintetico)</p>
          </div>
          <div className="chart-controls-wrap">
            <div className="seg-group" role="group" aria-label="Seleziona finestra grafico live">
              {(
                [
                  ["1h", "1h"],
                  ["3h", "3h"],
                  ["6h", "6h"],
                  ["12h", "12h"],
                  ["today", "Oggi"],
                ] as Array<[LiveChartRange, string]>
              ).map(([key, label]) => (
                <button
                  key={key}
                  type="button"
                  className={`seg-btn ${chartRange === key ? "active" : ""}`}
                  onClick={() => setChartRange(key)}
                >
                  {label}
                </button>
              ))}
            </div>
            <Link to="/storico" className="inline-action-link">
              Apri Storico →
            </Link>
          </div>
        </div>
        <EChart option={compactChartOption} height={280} ariaLabel="Grafico andamento ultime ore FV, Casa, Batteria, Rete e SOC" />
      </section>

      {/* BOTTOM ROW: TOP 3 GROUPED EVENTS + COMPACT SYSTEM STATUS PILLS */}
      <section className="grid-2">
        <article className="card">
          <div className="card-header">
            <div>
              <h3 className="card-title">Ultimi eventi significativi</h3>
              <p className="card-sub">Raggruppati automaticamente (massimo 3 voci recenti)</p>
            </div>
            <Link to="/decisioni" className="inline-action-link">
              Apri Decisioni →
            </Link>
          </div>
          {eventsTop3.length === 0 ? (
            <p className="card-sub">Nessun evento operativo significativo nelle ultime ore.</p>
          ) : (
            <div style={{ display: "flex", flexDirection: "column", gap: 8 }}>
              {eventsTop3.map((ev: any) => (
                <div key={ev.key ?? ev.id} className="event-summary-row">
                  <div>
                    <div style={{ fontWeight: 650, fontSize: 13 }}>{ev.title}</div>
                    {ev.detail && <div style={{ fontSize: 11.5, color: "var(--text-muted)", marginTop: 2 }}>{ev.detail}</div>}
                  </div>
                  <span className={`badge ${ev.severity === "warning" ? "safety" : "simulated"}`}>
                    {fmtShortTime(ev.occurred_at, live.timezone)}
                  </span>
                </div>
              ))}
            </div>
          )}
        </article>

        <article className="card">
          <div className="card-header">
            <div>
              <h3 className="card-title">Stato sistema</h3>
              <p className="card-sub">Monitoraggio continuo sottosistemi</p>
            </div>
            <span className={`badge ${Object.values(comps).some((c) => c?.status === "offline") ? "safety" : "measured"}`}>
              {Object.values(comps).some((c) => c?.status === "offline") ? "Attenzione" : "Operativo"}
            </span>
          </div>
          <div className="sys-pill-grid">
            {SYSTEM_LABELS.map(([key, label]) => {
              const st = comps[key]?.status ?? "unknown";
              const auxDetail = key === "aux1" ? String(comps[key]?.detail?.aux1 ?? "OFF") : null;
              const statusTxt = auxDetail ?? (st === "online" ? "OK" : st === "degraded" ? "Avviso" : st === "offline" ? "Offline" : "N/D");
              return (
                <div key={key} className="sys-pill-item">
                  <span>{label}</span>
                  <span className="sys-pill-status">
                    <span className={`dot ${st}`} aria-hidden="true" />
                    {statusTxt}
                  </span>
                </div>
              );
            })}
          </div>
        </article>
      </section>
    </>
  );
}
