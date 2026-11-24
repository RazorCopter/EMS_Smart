import { Link } from "react-router-dom";
import { ArrowRight, CloudFog, Home, ShieldAlert, ShieldCheck, Sun, Thermometer } from "lucide-react";
import { fmtTime, fmtWatts } from "../../lib/format";
import type { DecisionViewModel } from "../../viewmodels/dashboardViewModel";
import { HelpTip } from "../HelpTip";
import { EmeraldStatusPill } from "./EmeraldChrome";
import { EmsCoreBadgeSvg } from "./EmsCoreBadgeSvg";

interface Props {
  decision: DecisionViewModel;
  batteryW?: number | null;
  gridW?: number | null;
}

export function EmeraldDecisionCard({ decision, batteryW, gridW }: Props) {
  const limitsText =
    decision.safetyMinW != null && decision.safetyMaxW != null
      ? `${Math.round(decision.safetyMinW)} / ${Math.round(decision.safetyMaxW)} W`
      : "N/D";

  const setpointToneClass =
    decision.tone === "safety"
      ? "em-setpoint-safety"
      : decision.tone === "warn"
      ? "em-setpoint-warn"
      : "em-setpoint-normal";

  const ha = decision.haInputs;
  const hasHa = Boolean(
    ha &&
      (ha.forecastTodayKwh != null ||
        ha.forecastTomorrowKwh != null ||
        ha.fog != null ||
        ha.tempC != null ||
        ha.radiationWm2 != null),
  );

  return (
    <article className="em-card em-decision-card" aria-label="Decisione EMS corrente">
      <div className="em-card-head">
        <div className="em-dec-head-left">
          <EmsCoreBadgeSvg />
          <div>
            <h2 className="em-card-title">Decisione EMS</h2>
            <p className="em-card-sub">Valutazione locale &amp; input Home Assistant</p>
          </div>
        </div>
        <EmeraldStatusPill tone="shadow" label="Simulazione EMS" />
      </div>

      {/* Primary Hero Block: Strategia attiva + Setpoint simulato */}
      <div className="em-dec-hero">
        <div className="em-dec-strategy-block">
          <div className="em-dec-kicker label-with-tip">
            Strategia attiva
            <HelpTip
              id="tip-em-strat"
              title="Strategia attiva"
              text="Modalità effettiva applicata dal controllore EMS locale. Se diversa da quella salvata di base, è il modello predittivo che sta proteggendo o caricando la batteria."
              align="left"
            />
          </div>
          <div className="em-dec-strategy-val">{decision.effectiveStrategyLabel}</div>
          {decision.configuredStrategyLabel !== decision.effectiveStrategyLabel && (
            <div className="em-dec-strategy-base">Base: {decision.configuredStrategyLabel}</div>
          )}
        </div>

        <div className={`em-dec-setpoint-block ${setpointToneClass}`}>
          <div className="em-dec-kicker label-with-tip">
            Setpoint
            <HelpTip
              id="tip-em-sim"
              title="Setpoint simulato"
              text="Potenza target finale calcolata dopo deadband (±20 W) e rampa graduale. In LIVE può differire leggermente dal valore confermato dall'ESP."
              align="right"
            />
          </div>
          <div className="em-dec-setpoint-val mono">{fmtWatts(decision.simulatedW, "W")}</div>
        </div>
      </div>

      {/* Compressed 2x2 Telemetry & Safety Grid */}
      <dl className="em-dec-compare-grid em-dec-compact-grid" aria-label="Misure e vincoli">
        <div className="em-dec-tile">
          <dt className="em-dec-tile-k">Batteria / Rete</dt>
          <dd className="em-dec-tile-v mono">
            {fmtWatts(batteryW ?? null, "W")} <span className="em-dec-sep">·</span> {fmtWatts(gridW ?? null, "W")}
          </dd>
        </div>

        <div className="em-dec-tile">
          <dt className="em-dec-tile-k">Richiesta / Lim.</dt>
          <dd className="em-dec-tile-v mono">
            {fmtWatts(decision.candidateW, "W")} <span className="em-dec-sep">/</span> {fmtWatts(decision.limitedW, "W")}
          </dd>
        </div>

        <div className="em-dec-tile">
          <dt className="em-dec-tile-k">Safety</dt>
          <dd className="em-dec-tile-v em-dec-safety-row">
            {decision.hasSafetyLimit ? (
              <ShieldAlert size={14} className="em-tone-text-warn" aria-hidden="true" />
            ) : (
              <ShieldCheck size={14} className="em-tone-text-ok" aria-hidden="true" />
            )}
            <span>{decision.hasSafetyLimit ? "Limitato" : "Libero"}</span>
          </dd>
          <div className="em-dec-tile-sub mono">{limitsText}</div>
        </div>

        <div className={`em-dec-tile em-dec-reason-compact em-dec-reason-${decision.tone}`}>
          <dt className="em-dec-tile-k">Motivazione</dt>
          <dd className="em-dec-reason-title">{decision.humanTitle}</dd>
        </div>
      </dl>

      {/* Home Assistant Forecast & Weather Inputs Table */}
      {hasHa && ha && (
        <div className="em-dec-ha-box" aria-label="Dati Home Assistant">
          <div className="em-dec-ha-head">
            <span className="em-dec-kicker">Contesto Home Assistant</span>
            {ha.forecastRatioPct != null && (
              <span className="em-dec-ha-badge mono">Resa vs Forecast: {ha.forecastRatioPct}%</span>
            )}
          </div>
          <div className="em-dec-ha-table" role="table" aria-label="Tabella input Home Assistant">
            {(ha.forecastTodayKwh != null || ha.forecastTomorrowKwh != null) && (
              <div className="em-dec-ha-row" role="row">
                <span className="em-dec-ha-key" role="cell">
                  <span className="em-dec-ha-ico em-ha-ico-sun" aria-hidden="true"><Sun size={13} /></span>
                  <span>Forecast FV (Oggi / Domani)</span>
                </span>
                <span className="em-dec-ha-val mono" role="cell">
                  {ha.forecastTodayKwh != null ? `${ha.forecastTodayKwh.toFixed(1)}` : "N/D"} /{" "}
                  {ha.forecastTomorrowKwh != null ? `${ha.forecastTomorrowKwh.toFixed(1)} kWh` : "N/D"}
                </span>
              </div>
            )}
            {ha.fog != null && (
              <div className={`em-dec-ha-row ${ha.fog ? "em-dec-ha-row-warn" : ""}`} role="row">
                <span className="em-dec-ha-key" role="cell">
                  <span className={`em-dec-ha-ico ${ha.fog ? "em-ha-ico-warn" : "em-ha-ico-sky"}`} aria-hidden="true">
                    <CloudFog size={13} />
                  </span>
                  <span>Nebbia &amp; Visibilità</span>
                </span>
                <span className="em-dec-ha-val mono" role="cell">
                  {ha.fog ? "Presente" : "Assente"}
                  {ha.visibilityKm != null ? ` · ${ha.visibilityKm.toFixed(1)} km` : ""}
                </span>
              </div>
            )}
            {(ha.tempC != null || ha.radiationWm2 != null) && (
              <div className="em-dec-ha-row" role="row">
                <span className="em-dec-ha-key" role="cell">
                  <span className="em-dec-ha-ico em-ha-ico-cyan" aria-hidden="true"><Thermometer size={13} /></span>
                  <span>Meteo &amp; Irraggiamento</span>
                </span>
                <span className="em-dec-ha-val mono" role="cell">
                  {ha.tempC != null ? `${ha.tempC.toFixed(1)} °C` : ""}
                  {ha.tempC != null && ha.radiationWm2 != null ? " · " : ""}
                  {ha.radiationWm2 != null ? `${Math.round(ha.radiationWm2)} W/m²` : ""}
                </span>
              </div>
            )}
            {ha.loadProfileTodayKw != null && (
              <div className="em-dec-ha-row" role="row">
                <span className="em-dec-ha-key" role="cell">
                  <span className="em-dec-ha-ico em-ha-ico-home" aria-hidden="true"><Home size={13} /></span>
                  <span>Carico atteso oggi</span>
                </span>
                <span className="em-dec-ha-val mono" role="cell">
                  {ha.loadProfileTodayKw.toFixed(2)} kW
                </span>
              </div>
            )}
          </div>
        </div>
      )}

      <div className="em-dec-footer">
        <span className="em-dec-meta mono">
          {decision.seq != null ? `#${decision.seq} · ` : ""}
          {decision.timestamp ? fmtTime(decision.timestamp, decision.timezone) : "In attesa"}
        </span>
        <Link to={decision.detailUrl} className="em-dec-link">
          <span>Dettaglio</span>
          <ArrowRight size={15} aria-hidden="true" />
        </Link>
      </div>
    </article>
  );
}
