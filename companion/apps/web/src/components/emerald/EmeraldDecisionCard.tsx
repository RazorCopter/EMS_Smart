import { Link } from "react-router-dom";
import { ArrowRight, Info, ShieldAlert, ShieldCheck } from "lucide-react";
import { fmtTime, fmtWatts } from "../../lib/format";
import type { DecisionViewModel } from "../../viewmodels/dashboardViewModel";
import { HelpTip } from "../HelpTip";
import { EmeraldStatusPill } from "./EmeraldChrome";

interface Props {
  decision: DecisionViewModel;
  batteryW?: number | null;
  gridW?: number | null;
}

export function EmeraldDecisionCard({ decision, batteryW, gridW }: Props) {
  const limitsText =
    decision.safetyMinW != null && decision.safetyMaxW != null
      ? `${Math.round(decision.safetyMinW)} / ${Math.round(decision.safetyMaxW)} W`
      : "Non disponibile";

  const setpointToneClass =
    decision.tone === "safety"
      ? "em-setpoint-safety"
      : decision.tone === "warn"
      ? "em-setpoint-warn"
      : "em-setpoint-normal";

  return (
    <article className="em-card em-decision-card" aria-label="Decisione EMS corrente">
      <div className="em-card-head">
        <div>
          <h2 className="em-card-title">Decisione EMS</h2>
          <p className="em-card-sub">Valutazione autonoma locale</p>
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
            <div className="em-dec-strategy-base">Base configurata: {decision.configuredStrategyLabel}</div>
          )}
        </div>

        <div className={`em-dec-setpoint-block ${setpointToneClass}`}>
          <div className="em-dec-kicker label-with-tip">
            Setpoint simulato
            <HelpTip
              id="tip-em-sim"
              title="Setpoint simulato"
              text="Potenza target finale calcolata dopo deadband (±20 W) e rampa graduale. In modalità SHADOW è solo simulata."
              align="right"
            />
          </div>
          <div className="em-dec-setpoint-val mono">{fmtWatts(decision.simulatedW, "W")}</div>
        </div>
      </div>

      <dl className="em-dec-compare-grid em-dec-measured" aria-label="Misure dell’impianto">
        <div className="em-dec-tile">
          <dt className="em-dec-tile-k">Batteria misurata</dt>
          <dd className="em-dec-tile-v mono">{fmtWatts(batteryW ?? null, "W")}</dd>
        </div>
        <div className="em-dec-tile">
          <dt className="em-dec-tile-k">Scambio rete</dt>
          <dd className="em-dec-tile-v mono">{fmtWatts(gridW ?? null, "W")}</dd>
        </div>
      </dl>

      {/* Secondary Comparison Row: Richiesta iniziale & Safety */}
      <div className="em-dec-compare-grid">
        <div className="em-dec-tile">
          <div className="em-dec-tile-k label-with-tip">
            Richiesta iniziale
            <HelpTip
              id="tip-em-cand"
              title="Richiesta iniziale (Candidato)"
              text="Potenza richiesta dalla strategia prima dei limiti di surplus solare e dei vincoli Safety."
              align="left"
            />
          </div>
          <div className="em-dec-tile-v mono">{fmtWatts(decision.candidateW, "W")}</div>
          <div className="em-dec-tile-sub">Limited: {fmtWatts(decision.limitedW, "W")}</div>
        </div>

        <div className="em-dec-tile">
          <div className="em-dec-tile-k label-with-tip">
            Safety
            <HelpTip
              id="tip-em-safety"
              title="Limiti Safety"
              text="Inviluppo min/max consentito in questo secondo (Min = 0 W blocca la scarica)."
              align="right"
            />
          </div>
          <div className="em-dec-tile-v em-dec-safety-row">
            {decision.hasSafetyLimit ? (
              <ShieldAlert size={15} className="em-tone-text-warn" aria-hidden="true" />
            ) : (
              <ShieldCheck size={15} className="em-tone-text-ok" aria-hidden="true" />
            )}
            <span>{decision.hasSafetyLimit ? "Limitato" : "Nessuna limitazione"}</span>
          </div>
          <div className="em-dec-tile-sub mono">Limiti: {limitsText}</div>
        </div>
      </div>

      {/* Human Explanation Block */}
      <div className={`em-dec-reason em-dec-reason-${decision.tone}`}>
        <div className="em-dec-kicker">Motivazione</div>
        <div className="em-dec-reason-title">{decision.humanTitle}</div>
        <p className="em-dec-reason-sub">{decision.humanSubtitle}</p>
      </div>

      {/* Integrated SHADOW Callout */}
      <div className="em-dec-shadow-strip" role="note">
        <Info size={15} aria-hidden="true" />
        <span>
          <strong>Setpoint calcolato</strong> · Il valore confermato dall’ESP può differire durante il LIVE.
        </span>
      </div>

      <div className="em-dec-footer">
        <span className="em-dec-meta mono">
          {decision.seq != null ? `#${decision.seq} · ` : ""}
          {decision.timestamp ? fmtTime(decision.timestamp, decision.timezone) : "In attesa"}
        </span>
        <Link to={decision.detailUrl} className="em-dec-link">
          <span>Apri dettaglio decisione</span>
          <ArrowRight size={15} aria-hidden="true" />
        </Link>
      </div>
    </article>
  );
}
