import { useState } from "react";
import { Link } from "react-router-dom";
import { Calendar, Clock, Server } from "lucide-react";
import { fmtAge, fmtDurationCompact, fmtPct, fmtShortTime, fmtTime } from "../../lib/format";
import {
  CHART_RANGE_TITLES,
  type DashboardViewModels,
  type LiveChartRange,
} from "../../viewmodels/dashboardViewModel";
import { EChart } from "../EChart";
import { HelpTip } from "../HelpTip";
import { EmeraldDetailDrawer, EmeraldPageHeader, EmeraldStatusPill } from "./EmeraldChrome";
import { espControlStatus } from "./espModeLabel";
import { EmeraldDecisionCard } from "./EmeraldDecisionCard";
import { EmeraldEnergyMonitor } from "./EmeraldEnergyMonitor";
import { EmeraldKpiSection } from "./EmeraldKpiSection";
import { EmeraldSystemPanel } from "./EmeraldSystemPanel";

interface Props {
  vm: DashboardViewModels;
  chartRange: LiveChartRange;
  onChangeChartRange: (range: LiveChartRange) => void;
}

function formatStartedAtLabel(iso: string | null | undefined, startedToday: boolean, tz: string): string {
  if (!iso) return "In attesa del primo campione";
  const hm = fmtShortTime(iso, tz);
  if (startedToday) return `Monitoraggio iniziato oggi alle ${hm}`;
  const d = new Date(iso).toLocaleDateString("it-IT", { timeZone: tz, day: "2-digit", month: "2-digit" });
  return `Attivo dal ${d} alle ${hm}`;
}

export function EmeraldDashboardLayout({ vm, chartRange, onChangeChartRange }: Props) {
  const [sysDrawerOpen, setSysDrawerOpen] = useState(false);

  const tel = vm.telemetry;
  const meter = vm.raw.house_meter;
  const meterAge = meter ? (Date.now() - Date.parse(meter.received_at)) / 1000 : Infinity;
  const measured = meter && meterAge >= -2 && meterAge <= 15 ? meter.watts : null;
  const flowTelemetry = tel && measured !== null && !tel.stale ? {
    ...tel,
    load_w: measured,
    quality: { ...tel.quality, load_w: { source: "measured", state: "valid", age_ms: Math.max(0, meterAge * 1000) } },
  } : tel;
  const cov = vm.coverage;
  const telStateLabel = !tel
    ? "In attesa di telemetria"
    : tel.stale
    ? `Dati non aggiornati (${fmtAge(tel.age_s)})`
    : `Telemetria live · ${fmtAge(tel.age_s)}`;

  const dateLabel = new Date(vm.generatedAt).toLocaleDateString("it-IT", {
    timeZone: vm.timezone,
    weekday: "short",
    day: "2-digit",
    month: "short",
  });

  return (
    <div className="em-dashboard" data-ui-template="emerald">
      {/* TALL CONTEXTUAL EMERALD HEADER */}
      <EmeraldPageHeader
        title="Dashboard Live"
        subtitle="Flussi energetici e regolazione dell’impianto"
        badges={
          <>
            <span className="em-header-clock mono">
              <Calendar size={14} aria-hidden="true" />
              <span>{dateLabel}</span>
              <Clock size={14} aria-hidden="true" />
              <strong>{fmtTime(vm.generatedAt, vm.timezone)}</strong>
            </span>
            <EmeraldStatusPill tone={vm.telStateClass} label={telStateLabel} />
            <EmeraldStatusPill tone="info" label={vm.decision.effectiveStrategyLabel} />
            <EmeraldStatusPill tone={espControlStatus(vm.system.components.aux1).pillTone} label={espControlStatus(vm.system.components.aux1).modeLabel} />
          </>
        }
        actions={
          <button
            type="button"
            className="btn em-mobile-only-btn"
            onClick={() => setSysDrawerOpen(true)}
            aria-label="Mostra stato impianto completo"
          >
            <Server size={15} />
            <span>Stato impianto</span>
          </button>
        }
      />

      {/* ROW 1: 65% RADIAL ENERGY MONITOR / 35% HIERARCHICAL DECISION CARD */}
      <section className="em-row-hero">
        <EmeraldEnergyMonitor
          telemetry={flowTelemetry}
          safetyMinW={vm.decision.safetyMinW}
          safetyMaxW={vm.decision.safetyMaxW}
          selfConsumptionPct={vm.kpis.selfConsumptionPct}
        />
        <EmeraldDecisionCard decision={vm.decision} batteryW={tel?.stale ? null : tel?.battery_w} gridW={tel?.stale ? null : tel?.grid_w} />
      </section>

      {/* ROW 4: 75% WIDE LIVE CHART + 3 HERO KPIs | 25% AUTONOMOUS STATO IMPIANTO PANEL */}
      <section className="em-row-bottom">
        <div className="em-bottom-left-col">
          <article className="em-card em-chart-panel">
            <div className="em-card-head em-chart-head">
              <div>
                <h3 className="em-card-title">{CHART_RANGE_TITLES[chartRange]}</h3>
                <p className="em-card-sub">FV, Casa, Batteria, Rete (W) e SOC (%) · Gap espliciti</p>
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
                      onClick={() => onChangeChartRange(key)}
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
            <EChart option={vm.liveChart.option} height={270} ariaLabel="Grafico andamento ultime ore FV, Casa, Batteria, Rete e SOC" />
          </article>

          <EmeraldKpiSection
            kpis={vm.kpis}
            battPowerW={tel?.battery_w}
            formulas={vm.raw.kpi_today?.formulas}
            section="hero"
          />
        </div>

        <div className="em-system-desktop-slot">
          <EmeraldSystemPanel system={vm.system} variant="panel" />
        </div>
      </section>

      <h2 className="em-card-title">Energia di oggi</h2>
      <EmeraldKpiSection
        kpis={vm.kpis}
        battPowerW={tel?.battery_w}
        formulas={vm.raw.kpi_today?.formulas}
        section="daily"
      />

      {/* SLEEK COVERAGE & DATA QUALITY RIBBON */}
      <section className="em-coverage-ribbon" aria-label="Copertura e qualità acquisizione dati">
        <div className="em-cov-left">
          <EmeraldStatusPill
            tone={cov.pct != null && cov.pct >= 95 ? "ok" : cov.pct == null ? "neutral" : "warn"}
            label={`Copertura: ${cov.pct != null ? fmtPct(cov.pct) : "N/D"}`}
          />
          <span className="label-with-tip em-cov-note">
            {formatStartedAtLabel(cov.firstSampleAt, cov.monitoringStartedToday, vm.timezone)}
            <HelpTip
              id="tip-em-coverage"
              title="Copertura telemetrica"
              text="Percentuale di secondi con dati validi dal primo avvio del Companion."
              align="left"
            />
          </span>
        </div>
        <div className="em-cov-pills">
          {cov.monitoringStartedToday && (
            <span className="em-cov-chip">Pre-avvio escluso ({fmtDurationCompact(cov.unmonitoredPreInstallSec)})</span>
          )}
          <span className="em-cov-chip">Gap: {fmtDurationCompact(cov.gapSeconds)}</span>
          <span className="em-cov-chip">Invalidi: {cov.invalidSamples}</span>
          {cov.unavailableSeconds > 0 && (
            <span className="em-cov-chip em-cov-chip-err">Unavailable: {fmtDurationCompact(cov.unavailableSeconds)}</span>
          )}
        </div>
      </section>

      {/* Mobile Drawer for Stato Impianto when launched from dashboard button */}
      <EmeraldDetailDrawer
        open={sysDrawerOpen}
        title="Stato impianto"
        subtitle="Sottosistemi locali e risorse"
        onClose={() => setSysDrawerOpen(false)}
      >
        <EmeraldSystemPanel system={vm.system} variant="drawer" />
      </EmeraldDetailDrawer>
    </div>
  );
}
