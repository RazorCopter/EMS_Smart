import { useMemo } from "react";
import { useQuery } from "@tanstack/react-query";
import { Link } from "react-router-dom";
import type { EChartsOption } from "echarts";
import { apiFetch } from "../lib/api";
import {
  fmtAge,
  fmtKwh,
  fmtPct,
  fmtTime,
  fmtWatts,
  reasonLabel,
  strategyLabel,
} from "../lib/format";
import { EChart } from "../components/EChart";
import { PowerFlow } from "../components/PowerFlow";
import { ShadowBanner } from "../components/ShadowBanner";

interface LiveResponse {
  generated_at: string;
  timezone: string;
  telemetry: any;
  decision: any;
  kpi_today: any;
  kpi_yesterday: any;
  sparklines: Array<{ hour: string; pv_kwh: number | null; load_kwh: number | null; chg_kwh: number | null; imp_kwh: number | null }>;
  system: { components: Record<string, { status: string; recorded_at: string | null; detail: Record<string, unknown> }> };
  recent_events: Array<{ id: number; occurred_at: string; event_type: string; severity: string; title: string; detail: string | null }>;
}

function SparkBars({ values, color }: { values: Array<number | null>; color: string }) {
  const clean = values.slice(-12);
  const max = Math.max(0.1, ...clean.map((v) => v ?? 0));
  return (
    <svg viewBox="0 0 180 28" height="26" width="100%" aria-hidden="true">
      {clean.map((v, i) => {
        const h = v == null ? 2 : Math.max(3, Math.round((v / max) * 24));
        return (
          <rect
            key={i}
            x={i * 14 + 2}
            y={26 - h}
            width="9"
            height={h}
            rx="2"
            fill={v == null ? "var(--missing-grey)" : color}
            opacity={v == null ? 0.35 : 0.85}
          />
        );
      })}
    </svg>
  );
}

function deltaLabel(today: number | null | undefined, yesterday: number | null | undefined): string {
  if (today == null || yesterday == null || yesterday <= 0.05) return "Confronto n.d.";
  const pct = ((today - yesterday) / yesterday) * 100;
  const sign = pct >= 0 ? "+" : "";
  return `${sign}${pct.toFixed(0)}% vs ieri`;
}

const SYSTEM_LABELS: Array<[string, string]> = [
  ["esp32", "ESP32"],
  ["mqtt", "MQTT"],
  ["home_assistant", "Home Assistant"],
  ["modbus", "Modbus FC04"],
  ["ntp", "NTP"],
  ["collector", "Collector"],
  ["database", "TimescaleDB"],
  ["aux1", "AUX1"],
];

export function DashboardPage() {
  const { data: live, isLoading, error } = useQuery({
    queryKey: ["live"],
    queryFn: () => apiFetch<LiveResponse>("/api/v1/live"),
    refetchInterval: 5000,
  });

  const { data: hist } = useQuery({
    queryKey: ["history-preview"],
    queryFn: () => apiFetch<{ series: any[] }>("/api/v1/history?range=today&bucket=15m"),
    refetchInterval: 30000,
  });

  const { data: comp } = useQuery({
    queryKey: ["comparison-preview"],
    queryFn: () => apiFetch<any>("/api/v1/comparison?range=today"),
    refetchInterval: 30000,
  });

  const previewChartOption: EChartsOption = useMemo(() => {
    const s = hist?.series ?? [];
    return {
      grid: { top: 14, right: 36, bottom: 24, left: 36 },
      tooltip: { trigger: "axis", confine: true },
      xAxis: {
        type: "category",
        data: s.map((p) => p.timestamp.slice(11, 16)),
        axisLabel: { color: "#6e8683", fontSize: 10 },
      },
      yAxis: [
        { type: "value", axisLabel: { color: "#6e8683", fontSize: 10, formatter: (v: number) => `${(v / 1000).toFixed(1)}k` }, splitLine: { lineStyle: { color: "#1f3134" } } },
        { type: "value", min: 0, max: 100, axisLabel: { color: "#6e8683", fontSize: 10, formatter: "{value}%" }, splitLine: { show: false } },
      ],
      series: [
        { name: "FV", type: "line", showSymbol: false, smooth: 0.25, connectNulls: false, lineStyle: { color: "#f5b942", width: 2 }, areaStyle: { color: "rgba(245,185,66,0.16)" }, data: s.map((p) => p.pv_w) },
        { name: "Casa", type: "line", showSymbol: false, smooth: 0.25, connectNulls: false, lineStyle: { color: "#a78bfa", width: 2 }, data: s.map((p) => p.load_w) },
        { name: "Batteria", type: "line", showSymbol: false, smooth: 0.25, connectNulls: false, lineStyle: { color: "#34d399", width: 2 }, data: s.map((p) => p.battery_w) },
        { name: "SOC", type: "line", yAxisIndex: 1, showSymbol: false, connectNulls: false, lineStyle: { color: "#eef5f3", width: 1.5, type: "dashed" }, data: s.map((p) => p.soc) },
      ],
    };
  }, [hist]);

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
  const sparks = live.sparklines ?? [];
  const comps = live.system?.components ?? {};
  const telStateClass = !tel ? "warn" : tel.stale ? "warn" : tel.fault ? "err" : "ok";

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
              ? `● Telemetria stale (${fmtAge(tel.age_s)})`
              : `● Telemetria valida · ${fmtAge(tel.age_s)}`}
          </span>
          <span className="chip shadow">Modalità SHADOW</span>
        </div>
      </header>

      <section className="grid-hero">
        <article className="card">
          <PowerFlow
            telemetry={tel}
            safetyMinW={dec?.safety_min_w}
            safetyMaxW={dec?.safety_max_w}
            selfConsumptionPct={kpi.self_consumption_pct}
          />
        </article>

        <article className="card" style={{ display: "flex", flexDirection: "column", justifyContent: "space-between", gap: 12 }}>
          <div>
            <div className="card-header">
              <h2 className="card-title">Stato Impianto &amp; EMS</h2>
              <span className={`chip ${dec?.strategy === "SAFE_MODE" ? "warn" : "ok"}`}>
                {dec?.strategy === "SAFE_MODE" ? "Safe Mode" : "Normale"}
              </span>
            </div>
            <div className="kv-row">
              <span className="kv-label">Modalità operativa</span>
              <span className="kv-val" style={{ color: "var(--shadow-blue)" }}>SHADOW</span>
            </div>
            <div className="kv-row">
              <span className="kv-label">Strategia locale / effettiva</span>
              <span className="kv-val">
                {strategyLabel(dec?.configured_strategy)} → {strategyLabel(dec?.strategy)}
              </span>
            </div>
            <div className="kv-row">
              <span className="kv-label">Candidato strategia</span>
              <span className="kv-val">{fmtWatts(dec?.candidate_w, "W")}</span>
            </div>
            <div className="kv-row">
              <span className="kv-label">Limiti Safety</span>
              <span className="kv-val">
                {dec?.safety_min_w != null && dec?.safety_max_w != null
                  ? `${Math.round(dec.safety_min_w)} / ${Math.round(dec.safety_max_w)} W`
                  : "Non disponibile"}
              </span>
            </div>
            <div className="kv-row">
              <span className="kv-label">Setpoint simulato</span>
              <span className="kv-val" style={{ color: "var(--accent-teal)" }}>
                {fmtWatts(dec?.simulated_w, "W")} <span className="badge simulated">Simulato</span>
              </span>
            </div>
            <div className="kv-row">
              <span className="kv-label">Motivo / Safety</span>
              <span className="kv-val">{reasonLabel(dec?.safety_reason !== "NONE" ? dec?.safety_reason : dec?.reason)}</span>
            </div>
            <div className="kv-row">
              <span className="kv-label">Ultima decisione {dec ? `(#${dec.seq})` : ""}</span>
              <span className="kv-val">{dec ? fmtTime(dec.source_timestamp, live.timezone) : "Non disponibile"}</span>
            </div>
          </div>
          <ShadowBanner compact />
        </article>
      </section>

      <section className="grid-4" aria-label="KPI energetici giornalieri">
        <article className="kpi-card">
          <div style={{ display: "flex", justifyContent: "space-between", fontSize: 12, color: "var(--text-secondary)" }}>
            <span>Produzione FV oggi</span>
            <span className="badge measured">Misurato</span>
          </div>
          <div className="kpi-val" style={{ color: "var(--pv-color)" }}>{fmtKwh(kpi.pv_kwh)}</div>
          <SparkBars values={sparks.map((s) => s.pv_kwh)} color="var(--pv-color)" />
          <div style={{ display: "flex", justifyContent: "space-between", fontSize: 12, color: "var(--text-secondary)" }}>
            <span>{deltaLabel(kpi.pv_kwh, yest.pv_kwh)}</span>
            <span>Autocons. {fmtPct(kpi.self_consumption_pct)}</span>
          </div>
        </article>

        <article className="kpi-card">
          <div style={{ display: "flex", justifyContent: "space-between", fontSize: 12, color: "var(--text-secondary)" }}>
            <span>Consumo oggi</span>
            <span className="badge derived">Derivato</span>
          </div>
          <div className="kpi-val" style={{ color: "var(--home-color)" }}>{fmtKwh(kpi.load_kwh)}</div>
          <SparkBars values={sparks.map((s) => s.load_kwh)} color="var(--home-color)" />
          <div style={{ display: "flex", justifyContent: "space-between", fontSize: 12, color: "var(--text-secondary)" }}>
            <span>{deltaLabel(kpi.load_kwh, yest.load_kwh)}</span>
            <span>Autosuff. {fmtPct(kpi.self_sufficiency_pct)}</span>
          </div>
        </article>

        <article className="kpi-card">
          <div style={{ display: "flex", justifyContent: "space-between", fontSize: 12, color: "var(--text-secondary)" }}>
            <span>Batteria oggi (C / D)</span>
            <span className="badge measured">Misurato</span>
          </div>
          <div className="kpi-val" style={{ color: "var(--battery-color)" }}>
            {kpi.battery_charge_kwh != null ? `+${kpi.battery_charge_kwh.toFixed(2)}` : "—"} /{" "}
            {kpi.battery_discharge_kwh != null ? `-${kpi.battery_discharge_kwh.toFixed(2)} kWh` : "—"}
          </div>
          <SparkBars values={sparks.map((s) => s.chg_kwh)} color="var(--battery-color)" />
          <div style={{ display: "flex", justifyContent: "space-between", fontSize: 12, color: "var(--text-secondary)" }}>
            <span>Cicli eq: {kpi.equivalent_cycles ?? "—"}</span>
            <span>Throughput {fmtKwh(kpi.battery_throughput_kwh)}</span>
          </div>
        </article>

        <article className="kpi-card">
          <div style={{ display: "flex", justifyContent: "space-between", fontSize: 12, color: "var(--text-secondary)" }}>
            <span>Scambio rete oggi</span>
            <span className="badge measured">Misurato</span>
          </div>
          <div className="kpi-val" style={{ color: "var(--grid-color)" }}>
            {kpi.grid_import_kwh != null ? `${kpi.grid_import_kwh.toFixed(2)}` : "—"} imp /{" "}
            {kpi.grid_export_kwh != null ? `${kpi.grid_export_kwh.toFixed(2)} esp` : "—"}
          </div>
          <SparkBars values={sparks.map((s) => s.imp_kwh)} color="var(--grid-color)" />
          <div style={{ display: "flex", justifyContent: "space-between", fontSize: 12, color: "var(--text-secondary)" }}>
            <span>{deltaLabel(kpi.grid_import_kwh, yest.grid_import_kwh)}</span>
            <span>Disp. dati {fmtPct(kpi.data_availability_pct)}</span>
          </div>
        </article>
      </section>

      <section className="grid-3">
        <article className="card">
          <div className="card-header">
            <div>
              <h3 className="card-title">Storico giornaliero</h3>
              <p className="card-sub">Potenza (W) e SOC (%) · Gap espliciti</p>
            </div>
            <Link to="/storico" style={{ color: "var(--accent-teal)", fontSize: 12, textDecoration: "none" }}>
              Apri Storico →
            </Link>
          </div>
          <EChart option={previewChartOption} height={190} ariaLabel="Anteprima grafico storico giornaliero" />
        </article>

        <article className="card">
          <div className="card-header">
            <div>
              <h3 className="card-title">Eventi &amp; Decisioni EMS</h3>
              <p className="card-sub">Ultime transizioni registrate</p>
            </div>
            <Link to="/decisioni" style={{ color: "var(--accent-teal)", fontSize: 12, textDecoration: "none" }}>
              Apri Decisioni →
            </Link>
          </div>
          {live.recent_events.length === 0 ? (
            <p className="card-sub">Nessun evento rilevato nella finestra recente.</p>
          ) : (
            <div style={{ display: "flex", flexDirection: "column", gap: 6 }}>
              {live.recent_events.map((ev) => (
                <div key={ev.id} className="kv-row" style={{ padding: "6px 0" }}>
                  <div>
                    <div style={{ fontWeight: 600, fontSize: 12.5 }}>{ev.title}</div>
                    <div style={{ fontSize: 11, color: "var(--text-muted)" }}>{ev.detail}</div>
                  </div>
                  <span className={`badge ${ev.severity === "warning" ? "safety" : "simulated"}`}>
                    {new Date(ev.occurred_at).toLocaleTimeString("it-IT", { timeZone: live.timezone })}
                  </span>
                </div>
              ))}
            </div>
          )}
        </article>

        <article className="card">
          <div className="card-header">
            <div>
              <h3 className="card-title">Confronto SEM / ESP &amp; Sistema</h3>
              <p className="card-sub">SEM reale vs ESP SHADOW simulato</p>
            </div>
            <Link to="/confronto-sem" style={{ color: "var(--accent-teal)", fontSize: 12, textDecoration: "none" }}>
              Dettagli →
            </Link>
          </div>
          <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: 8, marginBottom: 12 }}>
            <div style={{ background: "var(--bg-inset)", padding: 8, borderRadius: 8, border: "1px solid var(--border-subtle)" }}>
              <div style={{ fontSize: 11, color: "var(--text-muted)" }}>Concordanza</div>
              <div style={{ fontSize: 17, fontWeight: 700, color: "var(--battery-color)" }}>
                {fmtPct(comp?.kpi?.concordance_pct)}
              </div>
            </div>
            <div style={{ background: "var(--bg-inset)", padding: 8, borderRadius: 8, border: "1px solid var(--border-subtle)" }}>
              <div style={{ fontSize: 11, color: "var(--text-muted)" }}>Diff. media</div>
              <div style={{ fontSize: 17, fontWeight: 700 }}>
                {comp?.kpi?.mean_watt_diff != null ? `${Math.round(comp.kpi.mean_watt_diff)} W` : "—"}
              </div>
            </div>
            <div style={{ background: "var(--bg-inset)", padding: 8, borderRadius: 8, border: "1px solid var(--border-subtle)" }}>
              <div style={{ fontSize: 11, color: "var(--text-muted)" }}>Divergenze</div>
              <div style={{ fontSize: 17, fontWeight: 700, color: "var(--safety-amber)" }}>
                {comp?.kpi?.comparable_points
                  ? `${comp.kpi.divergent_points} / ${comp.kpi.comparable_points}`
                  : "—"}
              </div>
            </div>
          </div>

          <div style={{ display: "grid", gridTemplateColumns: "repeat(2, 1fr)", gap: 6 }}>
            {SYSTEM_LABELS.map(([key, label]) => {
              const st = comps[key]?.status ?? "unknown";
              const auxDetail = key === "aux1" ? String(comps[key]?.detail?.aux1 ?? "OFF") : null;
              return (
                <div
                  key={key}
                  style={{
                    background: "var(--bg-inset)",
                    border: "1px solid var(--border-subtle)",
                    borderRadius: 8,
                    padding: "6px 9px",
                    display: "flex",
                    alignItems: "center",
                    justifyContent: "space-between",
                    fontSize: 12,
                  }}
                >
                  <span>{label}</span>
                  <span style={{ display: "inline-flex", alignItems: "center", gap: 5, fontWeight: 600 }}>
                    <span className={`dot ${st}`} aria-hidden="true" />
                    {auxDetail ?? (st === "online" ? "OK" : st === "degraded" ? "Avviso" : st === "offline" ? "Offline" : "N/D")}
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
