import type { KeyboardEvent, ReactNode } from "react";
import { fmtAbsKw, fmtNum, fmtPct, fmtWatts } from "../../lib/format";
import { useOptionalTheme } from "../../theme/useTheme";
import type { TelemetryLive } from "../PowerFlow";
import type { DerivedEnergyFlow, NodeId, QualityKind } from "../shared/energyFlowModel";

interface Props {
  telemetry: TelemetryLive | null;
  model: DerivedEnergyFlow;
  deadbandW: number;
  compactViewport: boolean;
  activeNode: NodeId | null;
  onToggleNode: (id: NodeId) => void;
  onNodeKey: (id: NodeId, e: KeyboardEvent<SVGGElement>) => void;
  popover: ReactNode;
}

function strokeWidthForWatts(watts: number | null | undefined, deadband: number): number {
  if (watts == null) return 2.2;
  const abs = Math.abs(watts);
  if (abs <= deadband) return 2.2;
  return Number(Math.min(5.2, 2.5 + (abs / 3500) * 2.7).toFixed(2));
}

function durationForWatts(watts: number | null | undefined, deadband: number): string {
  const abs = Math.abs(watts ?? 0);
  if (abs <= deadband) return "1.8s";
  return `${Math.max(0.65, Math.min(2.0, 2.0 - (abs / 3500) * 1.35)).toFixed(2)}s`;
}

function QualityBadgeSvg({ x, y, kind }: { x: number; y: number; kind: QualityKind }) {
  const cfg = {
    measured: { bg: "var(--battery-soft)", fg: "var(--battery-color)", glyph: "✓" },
    derived: { bg: "var(--home-soft)", fg: "var(--home-color)", glyph: "≈" },
    stale: { bg: "var(--safety-soft)", fg: "var(--safety-amber)", glyph: "!" },
    unavailable: { bg: "var(--missing-soft)", fg: "var(--missing-grey)", glyph: "○" },
  }[kind];
  return (
    <g transform={`translate(${x}, ${y})`} aria-hidden="true">
      <circle r="9" fill={cfg.bg} />
      <text y="3.5" textAnchor="middle" fill={cfg.fg} fontSize="10.5" fontWeight="700">
        {cfg.glyph}
      </text>
    </g>
  );
}

function RadialArcFlow({
  d,
  color,
  watts,
  active,
  deadband,
  markerId,
  dashed = false,
  reducedMotion = false,
}: {
  d: string;
  color: string;
  watts: number | null | undefined;
  active: boolean;
  deadband: number;
  markerId: string;
  dashed?: boolean;
  reducedMotion?: boolean;
}) {
  const sw = strokeWidthForWatts(watts, deadband);
  const dur = durationForWatts(watts, deadband);
  return (
    <g>
      <path
        d={d}
        fill="none"
        stroke={color}
        strokeWidth={sw}
        strokeLinecap="round"
        strokeDasharray={dashed ? "6 5" : undefined}
        opacity={active ? (dashed ? 0.62 : 0.45) : 0.35}
        markerEnd={active ? `url(#${markerId})` : undefined}
      />
      {active && !reducedMotion && (
        <>
          <circle r={Math.min(4.2, sw * 0.85)} fill={color}>
            <animateMotion dur={dur} repeatCount="indefinite" path={d} />
          </circle>
          <circle r={Math.min(3.5, sw * 0.72)} fill={color} opacity="0.65">
            <animateMotion dur={dur} begin={`${Number.parseFloat(dur) * 0.5}s`} repeatCount="indefinite" path={d} />
          </circle>
        </>
      )}
    </g>
  );
}

export default function EmeraldEnergyMonitor({
  telemetry,
  model,
  deadbandW,
  compactViewport,
  activeNode,
  onToggleNode,
  onNodeKey,
  popover,
}: Props) {
  const { reducedMotion } = useOptionalTheme();
  const {
    isStale,
    pv,
    load,
    grid,
    batt,
    soc,
    pvActive,
    loadActive,
    gridExport,
    gridActive,
    battDischarge,
    battActive,
    pvColor,
    loadColor,
    gridColor,
    battColor,
    inactiveColor,
    pvQualityKind,
    gridQualityKind,
    loadQualityKind,
    battQualityKind,
    pvStateLabel,
    gridDirLabel,
    loadDirLabel,
    battDirLabel,
    invStatusLabel,
    accessibleSummary,
  } = model;

  const socBarWidth = soc != null ? Math.max(6, Math.min(314, (soc / 100) * 314)) : 0;
  const hubStroke = telemetry?.fault
    ? "var(--fault-red)"
    : isStale
    ? "var(--safety-amber)"
    : "var(--accent-teal)";

  return (
    <div className="emerald-monitor-wrap">
      <div className="card-header">
        <div>
          <h2 className="card-title">Energy Monitor</h2>
          <p className="card-sub">
            Bilancio energetico radiale in tempo reale · Tocca un nodo per dettagli elettrici
          </p>
        </div>
        <div className="energy-legend-pills">
          <span className="legend-dot-pill">
            <span className="legend-swatch solid" aria-hidden="true" /> Misurato
          </span>
          <span className="legend-dot-pill">
            <span className="legend-swatch dashed" aria-hidden="true" /> Derivato
          </span>
        </div>
      </div>

      <p className="sr-only" aria-live="polite">
        {accessibleSummary}
      </p>

      <div className="energy-stage">
        {compactViewport ? (
          <svg
            viewBox="0 0 340 318"
            className="emerald-radial-svg"
            role="img"
            aria-label={`Energy Monitor Emerald: ${accessibleSummary}`}
          >
            <defs>
              <marker id="em2-m-pv" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={pvColor} />
              </marker>
              <marker id="em2-m-home" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={loadColor} />
              </marker>
              <marker id="em2-m-grid" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={gridColor} />
              </marker>
              <marker id="em2-m-batt" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={battColor} />
              </marker>
            </defs>

            <circle cx="170" cy="152" r="96" fill="none" stroke="var(--border-subtle)" strokeWidth="1" strokeDasharray="3 5" />

            <RadialArcFlow d="M170 78 L170 116" color={pvColor} watts={pv} active={pvActive} deadband={deadbandW} markerId="em2-m-pv" reducedMotion={reducedMotion} />
            <RadialArcFlow d={gridExport ? "M130 152 L106 152" : "M104 152 L128 152"} color={gridColor} watts={grid} active={gridActive} deadband={deadbandW} markerId="em2-m-grid" reducedMotion={reducedMotion} />
            <RadialArcFlow d="M212 152 L234 152" color={loadColor} watts={load} active={loadActive} deadband={deadbandW} markerId="em2-m-home" dashed reducedMotion={reducedMotion} />
            <RadialArcFlow d={battDischarge ? "M170 222 L170 188" : "M170 186 L170 220"} color={battColor} watts={batt} active={battActive} deadband={deadbandW} markerId="em2-m-batt" reducedMotion={reducedMotion} />

            {/* Mobile Top: FV */}
            <g transform="translate(74, 8)" className={`emerald-node-card ${activeNode === "pv" ? "selected" : ""}`} role="button" tabIndex={0} aria-expanded={activeNode === "pv"} aria-label={`Fotovoltaico: ${pv == null ? "Non disponibile" : fmtAbsKw(pv)}, ${pvStateLabel}`} onClick={() => onToggleNode("pv")} onKeyDown={(e) => onNodeKey("pv", e)}>
              <rect className="node-frame" width="192" height="70" rx="14" fill="var(--bg-surface-2)" stroke={pv == null || isStale ? inactiveColor : "var(--pv-color)"} strokeWidth="1.5" />
              <text x="14" y="22" fill="var(--text-secondary)" fontSize="12" fontWeight="650">Fotovoltaico</text>
              <text x="14" y="45" fill={pv == null || isStale ? inactiveColor : "var(--pv-color)"} fontSize="18.5" fontWeight="750">{pv == null ? "Non disp." : fmtAbsKw(pv)}</text>
              <text x="14" y="61" fill="var(--text-muted)" fontSize="12">{pvStateLabel}</text>
              <QualityBadgeSvg x={174} y={19} kind={pvQualityKind} />
            </g>

            {/* Mobile Left: Rete */}
            <g transform="translate(4, 110)" className={`emerald-node-card ${activeNode === "grid" ? "selected" : ""}`} role="button" tabIndex={0} aria-expanded={activeNode === "grid"} aria-label={`Rete: ${grid == null ? "Non disponibile" : fmtWatts(grid)}, ${gridDirLabel}`} onClick={() => onToggleNode("grid")} onKeyDown={(e) => onNodeKey("grid", e)}>
              <rect className="node-frame" width="102" height="84" rx="14" fill="var(--bg-surface-2)" stroke={grid == null || isStale ? inactiveColor : "var(--grid-color)"} strokeWidth="1.5" />
              <text x="10" y="22" fill="var(--text-secondary)" fontSize="12" fontWeight="650">Rete</text>
              <text x="10" y="47" fill={grid == null || isStale ? inactiveColor : "var(--grid-color)"} fontSize="15.5" fontWeight="750">{grid == null ? "Non disp." : fmtWatts(grid)}</text>
              <text x="10" y="68" fill="var(--text-muted)" fontSize="12">{gridDirLabel}</text>
              <QualityBadgeSvg x={86} y={19} kind={gridQualityKind} />
            </g>

            {/* Mobile Center: Inverter */}
            <g transform="translate(170, 152)" className={`emerald-node-card ${activeNode === "inverter" ? "selected" : ""}`} role="button" tabIndex={0} aria-expanded={activeNode === "inverter"} aria-label={`Inverter Afore: ${invStatusLabel}`} onClick={() => onToggleNode("inverter")} onKeyDown={(e) => onNodeKey("inverter", e)}>
              <circle className="node-frame" r="38" fill="var(--bg-elevated)" stroke={hubStroke} strokeWidth="2" />
              <text y="-2" textAnchor="middle" fill="var(--text-primary)" fontSize="13.5" fontWeight="750">Afore</text>
              <text y="15" textAnchor="middle" fill={hubStroke} fontSize="12" fontWeight="650">{invStatusLabel}</text>
            </g>

            {/* Mobile Right: Casa */}
            <g transform="translate(234, 110)" className={`emerald-node-card ${activeNode === "home" ? "selected" : ""}`} role="button" tabIndex={0} aria-expanded={activeNode === "home"} aria-label={`Casa: ${load == null ? "Non disponibile" : fmtAbsKw(load)}, ${loadDirLabel}`} onClick={() => onToggleNode("home")} onKeyDown={(e) => onNodeKey("home", e)}>
              <rect className="node-frame" width="102" height="84" rx="14" fill="var(--bg-surface-2)" stroke={load == null || isStale ? inactiveColor : "var(--home-color)"} strokeWidth="1.5" strokeDasharray="5 3" />
              <text x="10" y="22" fill="var(--text-secondary)" fontSize="12" fontWeight="650">Casa</text>
              <text x="10" y="47" fill={load == null || isStale ? inactiveColor : "var(--home-color)"} fontSize="15.5" fontWeight="750">{load == null ? "Non disp." : fmtAbsKw(load)}</text>
              <text x="10" y="68" fill="var(--text-muted)" fontSize="12">{loadDirLabel}</text>
              <QualityBadgeSvg x={86} y={19} kind={loadQualityKind} />
            </g>

            {/* Mobile Bottom: Batteria */}
            <g transform="translate(22, 222)" className={`emerald-node-card ${activeNode === "battery" ? "selected" : ""}`} role="button" tabIndex={0} aria-expanded={activeNode === "battery"} aria-label={`Batteria: SOC ${fmtPct(soc)}, ${batt == null ? "Non disponibile" : fmtWatts(batt)}`} onClick={() => onToggleNode("battery")} onKeyDown={(e) => onNodeKey("battery", e)}>
              <rect className="node-frame" width="296" height="88" rx="14" fill="var(--bg-surface-2)" stroke={batt == null && soc == null ? inactiveColor : "var(--battery-color)"} strokeWidth="1.5" />
              <text x="14" y="22" fill="var(--text-secondary)" fontSize="12" fontWeight="650">Batteria · {battDirLabel}</text>
              <text x="14" y="49" fill={batt == null || isStale ? inactiveColor : "var(--battery-color)"} fontSize="19" fontWeight="750">{batt == null ? "Non disp." : fmtWatts(batt)}</text>
              <text x="260" y="48" textAnchor="end" fill={soc == null ? inactiveColor : "var(--battery-color)"} fontSize="22" fontWeight="800">{soc != null ? fmtPct(soc) : "—"}</text>
              <QualityBadgeSvg x={278} y={20} kind={battQualityKind} />
              <rect x="14" y="64" width="268" height="10" rx="5" fill="var(--bg-inset)" stroke="var(--border-subtle)" />
              {soc != null && (
                <rect x="14" y="64" width={Math.max(6, Math.min(268, (soc / 100) * 268))} height="10" rx="5" fill={isStale ? inactiveColor : "var(--battery-color)"} />
              )}
            </g>
          </svg>
        ) : (
          <svg
            viewBox="0 0 700 376"
            className="emerald-radial-svg"
            role="img"
            aria-label={`Energy Monitor Emerald: ${accessibleSummary}`}
          >
            <defs>
              <marker id="em-d-pv" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={pvColor} />
              </marker>
              <marker id="em-d-home" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={loadColor} />
              </marker>
              <marker id="em-d-grid" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={gridColor} />
              </marker>
              <marker id="em-d-batt" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={battColor} />
              </marker>
            </defs>

            {/* Radial orbital guide rings */}
            <circle cx="350" cy="184" r="144" fill="none" stroke="var(--border-subtle)" strokeWidth="1" strokeDasharray="3 6" opacity="0.85" />
            <circle cx="350" cy="184" r="76" fill="none" stroke="var(--border-subtle)" strokeWidth="1" opacity="0.65" />

            {/* Curved Radial Energy Paths */}
            <RadialArcFlow
              d="M350 96 L350 130"
              color={pvColor}
              watts={pv}
              active={pvActive}
              deadband={deadbandW}
              markerId="em-d-pv"
              reducedMotion={reducedMotion}
            />
            <RadialArcFlow
              d={gridExport ? "M294 184 Q248 184 202 184" : "M202 184 Q248 184 294 184"}
              color={gridColor}
              watts={grid}
              active={gridActive}
              deadband={deadbandW}
              markerId="em-d-grid"
              reducedMotion={reducedMotion}
            />
            <RadialArcFlow
              d="M406 184 Q452 184 498 184"
              color={loadColor}
              watts={load}
              active={loadActive}
              deadband={deadbandW}
              markerId="em-d-home"
              dashed
              reducedMotion={reducedMotion}
            />
            <RadialArcFlow
              d={battDischarge ? "M350 266 L350 238" : "M350 238 L350 266"}
              color={battColor}
              watts={batt}
              active={battActive}
              deadband={deadbandW}
              markerId="em-d-batt"
              reducedMotion={reducedMotion}
            />

            {/* TOP NODE: Fotovoltaico */}
            <g
              transform="translate(242, 10)"
              className={`emerald-node-card ${activeNode === "pv" ? "selected" : ""}`}
              role="button"
              tabIndex={0}
              aria-expanded={activeNode === "pv"}
              aria-label={`Fotovoltaico: ${pv == null ? "Non disponibile" : fmtAbsKw(pv)}, stato ${pvStateLabel}. Premi Invio per i dettagli`}
              onClick={() => onToggleNode("pv")}
              onKeyDown={(e) => onNodeKey("pv", e)}
            >
              <rect
                className="node-frame"
                width="216"
                height="86"
                rx="14"
                fill="var(--bg-surface-2)"
                stroke={pv == null || isStale ? inactiveColor : "var(--pv-color)"}
                strokeWidth={activeNode === "pv" ? 2.2 : 1.5}
              />
              <g transform="translate(14,13)" stroke={pv == null || isStale ? inactiveColor : "var(--pv-color)"} strokeWidth="1.8" fill="none">
                <circle cx="9" cy="9" r="3.6" />
                <path d="M9 1v2.2M9 14.8V17M1 9h2.2M14.8 9H17M3.4 3.4l1.5 1.5M13.1 13.1l1.5 1.5M14.6 3.4l-1.5 1.5M4.9 13.1l-1.5 1.5" />
              </g>
              <text x="38" y="26" fill="var(--text-secondary)" fontSize="12.5" fontWeight="650">
                Fotovoltaico
              </text>
              <text x="16" y="54" fill={pv == null || isStale ? inactiveColor : "var(--pv-color)"} fontSize="22" fontWeight="750">
                {pv == null ? "Non disp." : fmtAbsKw(pv)}
              </text>
              <text x="16" y="73" fill="var(--text-muted)" fontSize="12">
                {pvStateLabel}
              </text>
              <QualityBadgeSvg x={196} y={20} kind={pvQualityKind} />
            </g>

            {/* LEFT NODE: Rete */}
            <g
              transform="translate(16, 134)"
              className={`emerald-node-card ${activeNode === "grid" ? "selected" : ""}`}
              role="button"
              tabIndex={0}
              aria-expanded={activeNode === "grid"}
              aria-label={`Rete elettrica: ${grid == null ? "Non disponibile" : fmtWatts(grid)}, ${gridDirLabel}. Premi Invio per i dettagli`}
              onClick={() => onToggleNode("grid")}
              onKeyDown={(e) => onNodeKey("grid", e)}
            >
              <rect
                className="node-frame"
                width="186"
                height="98"
                rx="14"
                fill="var(--bg-surface-2)"
                stroke={grid == null || isStale ? inactiveColor : "var(--grid-color)"}
                strokeWidth={activeNode === "grid" ? 2.2 : 1.5}
              />
              <g transform="translate(14,13)" stroke={grid == null || isStale ? inactiveColor : "var(--grid-color)"} strokeWidth="1.8" fill="none" strokeLinecap="round">
                <path d="M9 2v14M4 6h10M3 10h12M6 16l3-10 3 10" />
              </g>
              <text x="38" y="26" fill="var(--text-secondary)" fontSize="12.5" fontWeight="650">
                Rete
              </text>
              <text x="16" y="57" fill={grid == null || isStale ? inactiveColor : "var(--grid-color)"} fontSize="20.5" fontWeight="750">
                {grid == null ? "Non disp." : fmtWatts(grid)}
              </text>
              <text x="16" y="79" fill="var(--text-muted)" fontSize="12">
                {gridDirLabel}
              </text>
              <QualityBadgeSvg x={166} y={20} kind={gridQualityKind} />
            </g>

            {/* RIGHT NODE: Casa (dashed derived indicator) */}
            <g
              transform="translate(498, 134)"
              className={`emerald-node-card ${activeNode === "home" ? "selected" : ""}`}
              role="button"
              tabIndex={0}
              aria-expanded={activeNode === "home"}
              aria-label={`Casa: ${load == null ? "Non disponibile" : fmtAbsKw(load)}, ${loadDirLabel}. Premi Invio per i dettagli`}
              onClick={() => onToggleNode("home")}
              onKeyDown={(e) => onNodeKey("home", e)}
            >
              <rect
                className="node-frame"
                width="186"
                height="98"
                rx="14"
                fill="var(--bg-surface-2)"
                stroke={load == null || isStale ? inactiveColor : "var(--home-color)"}
                strokeWidth={activeNode === "home" ? 2.2 : 1.5}
                strokeDasharray="5 3"
              />
              <g transform="translate(14,13)" stroke={load == null || isStale ? inactiveColor : "var(--home-color)"} strokeWidth="1.8" fill="none" strokeLinecap="round" strokeLinejoin="round">
                <path d="M2 9.5L9 3l7 6.5M4 8.5V16h10V8.5" />
              </g>
              <text x="38" y="26" fill="var(--text-secondary)" fontSize="12.5" fontWeight="650">
                Casa
              </text>
              <text x="16" y="57" fill={load == null || isStale ? inactiveColor : "var(--home-color)"} fontSize="20.5" fontWeight="750">
                {load == null ? "Non disp." : fmtAbsKw(load)}
              </text>
              <text x="16" y="79" fill="var(--text-muted)" fontSize="12">
                {loadDirLabel}
              </text>
              <QualityBadgeSvg x={166} y={20} kind={loadQualityKind} />
            </g>

            {/* CENTER HUB: Inverter Afore */}
            <g
              transform="translate(350, 184)"
              className={`emerald-node-card ${activeNode === "inverter" ? "selected" : ""}`}
              role="button"
              tabIndex={0}
              aria-expanded={activeNode === "inverter"}
              aria-label={`Inverter Afore: ${invStatusLabel}. Premi Invio per i dettagli`}
              onClick={() => onToggleNode("inverter")}
              onKeyDown={(e) => onNodeKey("inverter", e)}
            >
              <circle
                className="node-frame"
                r="52"
                fill="var(--bg-elevated)"
                stroke={hubStroke}
                strokeWidth={activeNode === "inverter" ? 2.8 : 2.1}
              />
              <text y="-6" textAnchor="middle" fill="var(--text-primary)" fontSize="15" fontWeight="750">
                Afore
              </text>
              <text y="14" textAnchor="middle" fill={hubStroke} fontSize="12" fontWeight="650">
                {invStatusLabel}
              </text>
            </g>

            {/* BOTTOM NODE: Batteria */}
            <g
              transform="translate(176, 268)"
              className={`emerald-node-card ${activeNode === "battery" ? "selected" : ""}`}
              role="button"
              tabIndex={0}
              aria-expanded={activeNode === "battery"}
              aria-label={`Batteria: SOC ${fmtPct(soc)}, potenza ${batt == null ? "Non disponibile" : fmtWatts(batt)}, stato ${battDirLabel}. Premi Invio per i dettagli`}
              onClick={() => onToggleNode("battery")}
              onKeyDown={(e) => onNodeKey("battery", e)}
            >
              <rect
                className="node-frame"
                width="348"
                height="98"
                rx="14"
                fill="var(--bg-surface-2)"
                stroke={batt == null && soc == null ? inactiveColor : isStale ? inactiveColor : "var(--battery-color)"}
                strokeWidth={activeNode === "battery" ? 2.2 : 1.5}
              />
              <g transform="translate(16,14)" stroke={batt == null && soc == null ? inactiveColor : "var(--battery-color)"} strokeWidth="1.8" fill="none" strokeLinecap="round">
                <rect x="1" y="4" width="16" height="10" rx="2" />
                <path d="M19 7v4" />
              </g>
              <text x="42" y="26" fill="var(--text-secondary)" fontSize="12.5" fontWeight="650">
                Batteria · {battDirLabel}
              </text>
              <text x="16" y="56" fill={batt == null || isStale ? inactiveColor : "var(--battery-color)"} fontSize="22" fontWeight="750">
                {batt == null ? "Non disp." : fmtWatts(batt)}
              </text>
              {telemetry?.battery_v != null && (
                <text x="160" y="55" fill="var(--text-muted)" fontSize="12">
                  {fmtNum(telemetry.battery_v, "V", 1)}
                </text>
              )}

              <text x="306" y="24" textAnchor="end" fill="var(--text-secondary)" fontSize="12" fontWeight="650">
                SOC
              </text>
              <text x="306" y="55" textAnchor="end" fill={soc == null ? inactiveColor : "var(--battery-color)"} fontSize="27" fontWeight="800">
                {soc != null ? fmtPct(soc) : "—"}
              </text>
              <QualityBadgeSvg x={326} y={20} kind={battQualityKind} />

              <rect x="16" y="70" width="316" height="11" rx="5.5" fill="var(--bg-inset)" stroke="var(--border-subtle)" />
              {soc != null && (
                <rect
                  x="16"
                  y="70"
                  width={socBarWidth}
                  height="11"
                  rx="5.5"
                  fill={isStale ? inactiveColor : "var(--battery-color)"}
                />
              )}
            </g>
          </svg>
        )}

        {popover}
      </div>
    </div>
  );
}
