import { BatteryCharging, Home, PlugZap, Sun } from "lucide-react";
import { fmtKwh, fmtNum, fmtPct } from "../../lib/format";
import {
  formatDeltaYesterdayPct,
  type DailyKpiViewModel,
  type SparkPoint,
} from "../../viewmodels/dashboardViewModel";
import { HelpTip } from "../HelpTip";

interface Props {
  kpis: DailyKpiViewModel;
  battPowerW: number | null | undefined;
  formulas?: Record<string, string>;
}

function ContinuousSparkline({
  id,
  points,
  color,
}: {
  id: string;
  points: SparkPoint[];
  color: string;
}) {
  const valid = points
    .slice(-14)
    .map((p, i) => ({ ...p, idx: i }))
    .filter((p): p is SparkPoint & { value: number; idx: number } => p.value != null && Number.isFinite(p.value));

  if (valid.length < 2) {
    return <div className="em-spark-empty">Serie oraria in formazione</div>;
  }

  const W = 200;
  const H = 36;
  const padX = 4;
  const padY = 5;
  const minVal = Math.min(0, ...valid.map((p) => p.value));
  const maxVal = Math.max(0.05, ...valid.map((p) => p.value));
  const span = Math.max(0.05, maxVal - minVal);
  const count = Math.max(2, valid.length);

  const coords = valid.map((p, i) => {
    const x = padX + (i / (count - 1)) * (W - padX * 2);
    const y = H - padY - ((p.value - minVal) / span) * (H - padY * 2);
    return { x: Number(x.toFixed(1)), y: Number(y.toFixed(1)), hour: p.hour, value: p.value };
  });

  const linePoints = coords.map((c) => `${c.x},${c.y}`).join(" ");
  const first = coords[0];
  const last = coords[coords.length - 1];
  const areaPoints = `${first.x},${H - 2} ${linePoints} ${last.x},${H - 2}`;

  return (
    <svg className="em-spark-svg" viewBox={`0 0 ${W} ${H}`} height="36" width="100%" aria-hidden="true">
      <defs>
        <linearGradient id={`grad-${id}`} x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%" stopColor={color} stopOpacity="0.32" />
          <stop offset="100%" stopColor={color} stopOpacity="0.02" />
        </linearGradient>
      </defs>
      <polygon points={areaPoints} fill={`url(#grad-${id})`} />
      <polyline
        fill="none"
        stroke={color}
        strokeWidth="2.1"
        strokeLinecap="round"
        strokeLinejoin="round"
        points={linePoints}
      />
      <circle cx={last.x} cy={last.y} r="3" fill={color} />
    </svg>
  );
}

function SegmentedBatteryGraphic({ socPct }: { socPct: number | null }) {
  const totalCells = 10;
  const clamped = socPct != null && Number.isFinite(socPct) ? Math.max(0, Math.min(100, socPct)) : 0;
  const filledCells = Math.round((clamped / 100) * totalCells);

  return (
    <div
      className="em-seg-battery"
      role="img"
      aria-label={`Livello batteria ${socPct != null ? fmtPct(socPct) : "non disponibile"}`}
    >
      <div className="em-seg-battery-body">
        {Array.from({ length: totalCells }).map((_, idx) => {
          const active = socPct != null && idx < filledCells;
          return (
            <span
              key={idx}
              className={`em-seg-cell ${active ? "active" : ""}`}
            />
          );
        })}
      </div>
      <span className="em-seg-battery-cap" aria-hidden="true" />
    </div>
  );
}

function kwhYesterdayCopy(today: number | null, yesterday: number | null, available: boolean): string {
  if (!available) return "Confronto disponibile da domani";
  if (today == null || yesterday == null || yesterday <= 0.05) {
    return "Nessun periodo precedente confrontabile";
  }
  const pct = ((today - yesterday) / yesterday) * 100;
  const sign = pct >= 0 ? "+" : "";
  return `${sign}${pct.toFixed(0)}% rispetto a ieri`;
}

export function EmeraldKpiSection({ kpis, battPowerW, formulas }: Props) {
  const scClamped = kpis.selfConsumptionPct != null && !kpis.pvNotCalculable ? Math.max(0, Math.min(100, kpis.selfConsumptionPct)) : 0;
  const ssClamped = kpis.selfSufficiencyPct != null && !kpis.loadNotCalculable ? Math.max(0, Math.min(100, kpis.selfSufficiencyPct)) : 0;
  const battState =
    battPowerW == null
      ? "N/D"
      : battPowerW > 20
      ? "In carica"
      : battPowerW < -20
      ? "In scarica"
      : "Inattiva";

  const selfProducedKwh =
    kpis.loadKwh != null && kpis.gridImportKwh != null
      ? Math.max(0, kpis.loadKwh - kpis.gridImportKwh)
      : null;

  return (
    <div className="em-kpi-stack">
      {/* ROW 2: 3 HIGH-IMPACT HERO PANELS (Autoconsumo, Autosufficienza, Accumulo) */}
      <section className="em-kpi-row-hero" aria-label="Indicatori chiave ad alto impatto">
        {/* Autoconsumo */}
        <article className="em-card em-kpi-panel">
          <div className="em-kpi-panel-top">
            <span className="em-kpi-title label-with-tip">
              Autoconsumo
              <HelpTip
                id="tip-em-sc"
                title="Formula Autoconsumo"
                text={`${formulas?.self_consumption ?? "FV autoconsumato / Produzione FV"} · Quota dell'energia solare usata subito in casa o accumulata in batteria.`}
                align="left"
              />
            </span>
            <span className="em-kpi-delta">
              {formatDeltaYesterdayPct(
                kpis.pvNotCalculable ? null : kpis.selfConsumptionPct,
                kpis.yesterday.selfConsumptionPct,
                kpis.yesterdayAvailable,
              )}
            </span>
          </div>

          <div className="em-kpi-huge mono" style={{ color: kpis.pvNotCalculable ? "var(--missing-grey)" : "var(--pv-color)" }}>
            {kpis.pvNotCalculable ? "Non calc." : fmtPct(kpis.selfConsumptionPct)}
          </div>

          <div className="em-thick-bar" aria-hidden="true">
            <div className="em-thick-bar-fill" style={{ width: `${scClamped}%`, background: "var(--pv-color)" }} />
          </div>

          <p className="em-kpi-explain">
            {kpis.pvNotCalculable
              ? "Produzione fotovoltaica ancora nulla oggi"
              : `${fmtKwh(kpis.selfConsumedKwh)} autoconsumati su ${fmtKwh(kpis.pvKwh)} prodotti`}
          </p>
        </article>

        {/* Autosufficienza */}
        <article className="em-card em-kpi-panel">
          <div className="em-kpi-panel-top">
            <span className="em-kpi-title label-with-tip">
              Autosufficienza
              <HelpTip
                id="tip-em-ss"
                title="Formula Autosufficienza"
                text={`${formulas?.self_sufficiency ?? "(Consumo totale - Importazione rete) / Consumo totale"} · Quota del fabbisogno domestico coperta senza acquistare dalla rete.`}
                align="left"
              />
            </span>
            <span className="em-kpi-delta">
              {formatDeltaYesterdayPct(
                kpis.loadNotCalculable ? null : kpis.selfSufficiencyPct,
                kpis.yesterday.selfSufficiencyPct,
                kpis.yesterdayAvailable,
              )}
            </span>
          </div>

          <div className="em-kpi-huge mono" style={{ color: kpis.loadNotCalculable ? "var(--missing-grey)" : "var(--accent-teal)" }}>
            {kpis.loadNotCalculable ? "Non calc." : fmtPct(kpis.selfSufficiencyPct)}
          </div>

          <div className="em-thick-bar" aria-hidden="true">
            <div className="em-thick-bar-fill" style={{ width: `${ssClamped}%`, background: "var(--accent-teal)" }} />
          </div>

          <p className="em-kpi-explain">
            {selfProducedKwh != null && kpis.loadKwh != null
              ? `${selfProducedKwh.toFixed(2)} kWh coperti da sole/batteria su ${kpis.loadKwh.toFixed(2)} kWh`
              : "Contributo autonomo non disponibile"}
          </p>
        </article>

        {/* Accumulo */}
        <article className="em-card em-kpi-panel em-kpi-battery">
          <div className="em-kpi-panel-top">
            <span className="em-kpi-title label-with-tip">
              Accumulo
              <HelpTip
                id="tip-em-batt"
                title="Accumulo Batteria"
                text="SOC attuale dal BMS, stato carica/scarica, energia transitata oggi (+carica / -scarica) e cicli equivalenti (1 ciclo = 2 × 10,24 kWh)."
                align="right"
              />
            </span>
            <span className="em-batt-state-pill">{battState}</span>
          </div>

          <div className="em-batt-hero-line">
            <div className="em-kpi-huge mono" style={{ color: "var(--battery-color)" }}>
              {kpis.socPct != null ? fmtPct(kpis.socPct) : "N/D"}
            </div>
            <SegmentedBatteryGraphic socPct={kpis.socPct} />
          </div>

          <div className="em-batt-stats-line">
            <span className="mono">
              {kpis.batteryChargeKwh != null ? `+${kpis.batteryChargeKwh.toFixed(2)}` : "—"} /{" "}
              {kpis.batteryDischargeKwh != null ? `-${kpis.batteryDischargeKwh.toFixed(2)}` : "—"} kWh
            </span>
            <span>
              Throughput {fmtKwh(kpis.batteryThroughputKwh)} · {kpis.batteryCycles ?? "—"} cicli ·{" "}
              {fmtNum(kpis.batteryVoltageV, "V", 1)}
            </span>
          </div>
        </article>
      </section>

      {/* ROW 3: 4 SECONDARY DAILY TOTALS WITH AUTHENTIC CONTINUOUS SPARKLINES */}
      <section className="em-kpi-row-secondary" aria-label="Totali giornalieri con andamento continuo">
        <article className="em-card em-kpi-spark-card">
          <div className="em-spark-head">
            <span className="em-spark-label label-with-tip">
              <Sun size={15} style={{ color: "var(--pv-color)" }} aria-hidden="true" />
              <span>Produzione oggi</span>
              <HelpTip
                id="tip-em-pv-day"
                title="Produzione fotovoltaica oggi"
                text="Energia solare generata da mezzanotte (integrata dai campioni FC04 senza zeri sintetici nei gap)."
                align="left"
              />
            </span>
            <span className="em-prov-tag">Misurato</span>
          </div>
          <div className="em-spark-val mono" style={{ color: "var(--pv-color)" }}>
            {kpis.pvKwh != null ? kpis.pvKwh.toFixed(2) : "Non disp."}
            {kpis.pvKwh != null && <span className="em-spark-unit"> kWh</span>}
          </div>
          <ContinuousSparkline id="pv" points={kpis.sparks.pv} color="var(--pv-color)" />
          <div className="em-spark-foot">{kwhYesterdayCopy(kpis.pvKwh, kpis.yesterday.pvKwh, kpis.yesterdayAvailable)}</div>
        </article>

        <article className="em-card em-kpi-spark-card">
          <div className="em-spark-head">
            <span className="em-spark-label label-with-tip">
              <Home size={15} style={{ color: "var(--home-color)" }} aria-hidden="true" />
              <span>Consumo oggi</span>
              <HelpTip
                id="tip-em-load-day"
                title="Consumo domestico oggi"
                text="Energia assorbita dalla casa oggi, calcolata dal bilancio AC istantaneo dell'inverter."
                align="left"
              />
            </span>
            <span className="em-prov-tag em-prov-derived">Derivato</span>
          </div>
          <div className="em-spark-val mono" style={{ color: "var(--home-color)" }}>
            {kpis.loadKwh != null ? kpis.loadKwh.toFixed(2) : "Non disp."}
            {kpis.loadKwh != null && <span className="em-spark-unit"> kWh</span>}
          </div>
          <ContinuousSparkline id="load" points={kpis.sparks.load} color="var(--home-color)" />
          <div className="em-spark-foot">{kwhYesterdayCopy(kpis.loadKwh, kpis.yesterday.loadKwh, kpis.yesterdayAvailable)}</div>
        </article>

        <article className="em-card em-kpi-spark-card">
          <div className="em-spark-head">
            <span className="em-spark-label label-with-tip">
              <PlugZap size={15} style={{ color: "var(--grid-color)" }} aria-hidden="true" />
              <span>Rete oggi</span>
              <HelpTip
                id="tip-em-grid-day"
                title="Scambio rete oggi"
                text="kWh importati dalla rete (imp) ed esportati verso la rete (esp) da mezzanotte."
                align="left"
              />
            </span>
            <span className="em-prov-tag">Misurato</span>
          </div>
          <div className="em-spark-val mono" style={{ color: "var(--grid-color)" }}>
            {kpis.gridImportKwh != null ? kpis.gridImportKwh.toFixed(2) : "—"}
            <span className="em-spark-unit"> imp / </span>
            {kpis.gridExportKwh != null ? kpis.gridExportKwh.toFixed(2) : "—"}
            <span className="em-spark-unit"> esp</span>
          </div>
          <ContinuousSparkline id="grid" points={kpis.sparks.grid} color="var(--grid-color)" />
          <div className="em-spark-foot">
            {kwhYesterdayCopy(kpis.gridImportKwh, kpis.yesterday.gridImportKwh, kpis.yesterdayAvailable)}
          </div>
        </article>

        <article className="em-card em-kpi-spark-card">
          <div className="em-spark-head">
            <span className="em-spark-label label-with-tip">
              <BatteryCharging size={15} style={{ color: "var(--battery-color)" }} aria-hidden="true" />
              <span>Batteria oggi</span>
              <HelpTip
                id="tip-em-batt-day"
                title="Energia batteria oggi"
                text="Energia immagazzinata (+) e ceduta (-) oggi."
                align="right"
              />
            </span>
            <span className="em-prov-tag">Misurato</span>
          </div>
          <div className="em-spark-val mono" style={{ color: "var(--battery-color)" }}>
            {kpis.batteryChargeKwh != null ? `+${kpis.batteryChargeKwh.toFixed(2)}` : "—"}
            <span className="em-spark-unit"> / </span>
            {kpis.batteryDischargeKwh != null ? `-${kpis.batteryDischargeKwh.toFixed(2)}` : "—"}
            <span className="em-spark-unit"> kWh</span>
          </div>
          <ContinuousSparkline id="batt" points={kpis.sparks.battery} color="var(--battery-color)" />
          <div className="em-spark-foot">
            {kwhYesterdayCopy(kpis.batteryChargeKwh, kpis.yesterday.batteryChargeKwh, kpis.yesterdayAvailable)}
          </div>
        </article>
      </section>
    </div>
  );
}
