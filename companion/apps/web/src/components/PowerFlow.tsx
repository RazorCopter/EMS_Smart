import { lazy, Suspense, useEffect, useMemo, useState, type KeyboardEvent } from "react";
import { X } from "lucide-react";
import { fmtAbsKw, fmtPct, fmtWatts } from "../lib/format";
import { useOptionalTheme } from "../theme/useTheme";
import { ThemeErrorBoundary } from "./shared/ThemeErrorBoundary";
import { buildEnergyFlowModel, type NodeId } from "./shared/energyFlowModel";

const EmeraldEnergyMonitor = lazy(() => import("./emerald/EmeraldEnergyMonitor"));

export interface TelemetryLive {
  age_s?: number | null;
  stale?: boolean;
  fault?: boolean | null;
  soc?: number | null;
  battery_w?: number | null;
  battery_v?: number | null;
  battery_a?: number | null;
  pv_w?: number | null;
  pv1_w?: number | null;
  pv1_v?: number | null;
  pv1_a?: number | null;
  pv2_w?: number | null;
  pv2_v?: number | null;
  pv2_a?: number | null;
  grid_w?: number | null;
  grid_v?: number | null;
  hz?: number | null;
  load_w?: number | null;
  temp_c?: number | null;
  reads?: number | null;
  errors?: number | null;
  quality?: Record<string, { age_ms?: number | null; source?: string; state?: string }>;
}

interface Props {
  telemetry: TelemetryLive | null;
  safetyMinW?: number | null;
  safetyMaxW?: number | null;
  selfConsumptionPct?: number | null;
  deadbandW?: number;
}

function flowStrokeWidth(watts: number | null | undefined, deadband: number): number {
  if (watts == null) return 2.2;
  const abs = Math.abs(watts);
  if (abs <= deadband) return 2.2;
  return Number(Math.min(5.6, 2.5 + (abs / 3500) * 3.1).toFixed(2));
}

function flowSpeedSeconds(watts: number | null | undefined, deadband: number): string {
  const abs = Math.abs(watts ?? 0);
  if (abs <= deadband) return "1.8s";
  const dur = Math.max(0.65, Math.min(2.1, 2.1 - (abs / 3500) * 1.45));
  return `${dur.toFixed(2)}s`;
}

function QualityGlyph({
  x,
  y,
  kind,
}: {
  x: number;
  y: number;
  kind: "measured" | "derived" | "stale" | "unavailable";
}) {
  const palette = {
    measured: { bg: "rgba(52, 211, 153, 0.16)", fg: "var(--battery-color)", glyph: "✓" },
    derived: { bg: "rgba(167, 139, 250, 0.18)", fg: "var(--home-color)", glyph: "≈" },
    stale: { bg: "rgba(245, 158, 11, 0.2)", fg: "var(--safety-amber)", glyph: "!" },
    unavailable: { bg: "rgba(100, 116, 139, 0.2)", fg: "var(--missing-grey)", glyph: "○" },
  }[kind];

  return (
    <g transform={`translate(${x}, ${y})`} aria-hidden="true">
      <circle r="8.5" fill={palette.bg} />
      <text y="3.5" textAnchor="middle" fill={palette.fg} fontSize="10" fontWeight="700">
        {palette.glyph}
      </text>
    </g>
  );
}

export function PowerFlow({
  telemetry,
  safetyMinW,
  safetyMaxW,
  selfConsumptionPct,
  deadbandW = 20,
}: Props) {
  const { effectiveStyle, activateTemporaryClassicFallback, resetToDefault } = useOptionalTheme();
  const [activeNode, setActiveNode] = useState<NodeId | null>(null);
  const [compactViewport, setCompactViewport] = useState<boolean>(() =>
    typeof window !== "undefined" && typeof window.matchMedia === "function"
      ? window.matchMedia("(max-width: 680px)").matches
      : false,
  );

  useEffect(() => {
    if (typeof window === "undefined" || typeof window.matchMedia !== "function") return;
    const mq = window.matchMedia("(max-width: 680px)");
    const onChange = () => setCompactViewport(mq.matches);
    mq.addEventListener("change", onChange);
    return () => mq.removeEventListener("change", onChange);
  }, []);

  const model = useMemo(
    () => buildEnergyFlowModel(telemetry, deadbandW, safetyMinW, safetyMaxW, selfConsumptionPct),
    [telemetry, deadbandW, safetyMinW, safetyMaxW, selfConsumptionPct],
  );

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
    specs,
  } = model;

  function toggleNode(id: NodeId) {
    setActiveNode((cur) => (cur === id ? null : id));
  }

  function handleNodeKey(id: NodeId, e: KeyboardEvent<SVGGElement>) {
    if (e.key === "Enter" || e.key === " ") {
      e.preventDefault();
      toggleNode(id);
    } else if (e.key === "Escape") {
      setActiveNode(null);
    }
  }

  const edgeOverlayClass = (active: boolean, dashed = false) => {
    if (!active) return "flow-pulse hidden";
    return dashed ? "flow-pulse animated derived-flow" : "flow-pulse animated";
  };

  function renderFlowLine({
    d,
    color,
    watts,
    active,
    markerId,
    dashed = false,
  }: {
    d: string;
    color: string;
    watts: number | null | undefined;
    active: boolean;
    markerId: string;
    dashed?: boolean;
  }) {
    const sw = flowStrokeWidth(watts, deadbandW);
    return (
      <g>
        <path
          d={d}
          className="flow-base"
          stroke={color}
          strokeWidth={sw}
          strokeDasharray={dashed ? "6 5" : undefined}
          opacity={active ? (dashed ? 0.52 : 0.36) : 0.45}
          markerEnd={active ? `url(#${markerId})` : undefined}
        />
        {active && (
          <path
            d={d}
            className={edgeOverlayClass(active, dashed)}
            style={{ animationDuration: flowSpeedSeconds(watts, deadbandW) }}
            stroke={color}
            strokeWidth={sw}
          />
        )}
      </g>
    );
  }

  const renderPopoverContent = () => {
    if (!activeNode) return null;
    const spec = specs[activeNode];
    return (
      <div
        className="node-popover"
        role="dialog"
        aria-label={spec.title}
        onKeyDown={(e) => {
          if (e.key === "Escape") setActiveNode(null);
        }}
      >
        <div className="node-popover-head">
          <strong style={{ color: spec.accent }}>{spec.title}</strong>
          <button
            type="button"
            className="btn-icon"
            aria-label="Chiudi dettaglio nodo"
            onClick={() => setActiveNode(null)}
          >
            <X size={15} />
          </button>
        </div>
        <div className="node-popover-grid">
          {spec.rows.map(([k, v]) => (
            <div key={k} className="node-popover-row">
              <span>{k}</span>
              <strong>{v}</strong>
            </div>
          ))}
        </div>
      </div>
    );
  };

  if (effectiveStyle === "emerald") {
    return (
      <ThemeErrorBoundary
        styleName={effectiveStyle}
        onFallbackToClassic={activateTemporaryClassicFallback}
        onResetDefault={resetToDefault}
      >
        <Suspense fallback={<div className="energy-stage" style={{ minHeight: 320 }} />}>
          <EmeraldEnergyMonitor
            telemetry={telemetry}
            model={model}
            deadbandW={deadbandW}
            compactViewport={compactViewport}
            activeNode={activeNode}
            onToggleNode={toggleNode}
            onNodeKey={handleNodeKey}
            popover={renderPopoverContent()}
          />
        </Suspense>
      </ThemeErrorBoundary>
    );
  }

  const socWidth = soc != null ? Math.max(4, Math.min(284, (soc / 100) * 284)) : 0;

  return (
    <div className="energy-monitor-wrap">
      <div className="card-header">
        <div>
          <h2 className="card-title">Energy Monitor</h2>
          <p className="card-sub">
            Flussi energetici in tempo reale · Seleziona un nodo per i parametri elettrici
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

      <div className="energy-stage">
        {compactViewport ? (
          <svg
            viewBox="0 0 340 296"
            width="100%"
            className="energy-svg mobile"
            role="img"
            aria-label={`Energy Monitor: Fotovoltaico ${pv == null ? "Non disponibile" : fmtAbsKw(pv)}, Rete ${fmtWatts(grid)}, Casa ${load == null ? "Non disponibile" : fmtAbsKw(load)}, Batteria ${fmtWatts(batt)}, SOC ${fmtPct(soc)}`}
          >
            <defs>
              <marker id="em-arr-pv" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={pvColor} />
              </marker>
              <marker id="em-arr-home" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={loadColor} />
              </marker>
              <marker id="em-arr-grid" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={gridColor} />
              </marker>
              <marker id="em-arr-batt" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={battColor} />
              </marker>
            </defs>
            <circle cx="170" cy="144" r="84" fill="none" stroke="var(--border-subtle)" strokeWidth="1.5" strokeDasharray="4 6" opacity="0.8" />
            {renderFlowLine({ d: "M170 72 L170 116", color: pvColor, watts: pv, active: pvActive, markerId: "em-arr-pv" })}
            {renderFlowLine({
              d: gridExport ? "M130 144 L106 144" : "M104 144 L128 144",
              color: gridColor,
              watts: grid,
              active: gridActive,
              markerId: "em-arr-grid",
            })}
            {renderFlowLine({ d: "M212 144 L234 144", color: loadColor, watts: load, active: loadActive, markerId: "em-arr-home", dashed: true })}
            {renderFlowLine({
              d: battDischarge ? "M170 212 L170 174" : "M170 172 L170 210",
              color: battColor,
              watts: batt,
              active: battActive,
              markerId: "em-arr-batt",
            })}

            {/* Mobile Top: PV */}
            <g
              transform="translate(76, 4)"
              className={`energy-node ${activeNode === "pv" ? "selected" : ""}`}
              role="button"
              tabIndex={0}
              aria-expanded={activeNode === "pv"}
              aria-label={`Fotovoltaico: ${pv == null ? "Non disponibile" : fmtAbsKw(pv)}, stato ${pvStateLabel}`}
              onClick={() => toggleNode("pv")}
              onKeyDown={(e) => handleNodeKey("pv", e)}
            >
              <rect width="188" height="68" rx="14" fill="var(--bg-surface)" stroke={pv == null || isStale ? inactiveColor : "var(--pv-color)"} strokeWidth="1.7" />
              <text x="14" y="22" fill="var(--text-secondary)" fontSize="11.5" fontWeight="650">Fotovoltaico</text>
              <text x="14" y="45" fill={pv == null || isStale ? inactiveColor : "var(--pv-color)"} fontSize="19" fontWeight="750">{pv == null ? "Non disp." : fmtAbsKw(pv)}</text>
              <text x="14" y="60" fill="var(--text-muted)" fontSize="10.5">{pvStateLabel}</text>
              <QualityGlyph x={170} y={18} kind={pvQualityKind} />
            </g>

            {/* Mobile Left: Rete */}
            <g
              transform="translate(2, 102)"
              className={`energy-node ${activeNode === "grid" ? "selected" : ""}`}
              role="button"
              tabIndex={0}
              aria-expanded={activeNode === "grid"}
              aria-label={`Rete: ${grid == null ? "Non disponibile" : fmtWatts(grid)}, stato ${gridDirLabel}`}
              onClick={() => toggleNode("grid")}
              onKeyDown={(e) => handleNodeKey("grid", e)}
            >
              <rect width="102" height="84" rx="14" fill="var(--bg-surface)" stroke={grid == null || isStale ? inactiveColor : "var(--grid-color)"} strokeWidth="1.7" />
              <text x="10" y="22" fill="var(--text-secondary)" fontSize="11.5" fontWeight="650">Rete</text>
              <text x="10" y="47" fill={grid == null || isStale ? inactiveColor : "var(--grid-color)"} fontSize="16" fontWeight="750">{grid == null ? "Non disp." : fmtWatts(grid)}</text>
              <text x="10" y="68" fill="var(--text-muted)" fontSize="10.5">{gridDirLabel}</text>
              <QualityGlyph x={86} y={18} kind={gridQualityKind} />
            </g>

            {/* Mobile Center: Inverter */}
            <g
              transform="translate(130, 118)"
              className={`energy-node ${activeNode === "inverter" ? "selected" : ""}`}
              role="button"
              tabIndex={0}
              aria-expanded={activeNode === "inverter"}
              aria-label={`Inverter Afore: stato ${invStatusLabel}`}
              onClick={() => toggleNode("inverter")}
              onKeyDown={(e) => handleNodeKey("inverter", e)}
            >
              <rect width="80" height="54" rx="15" fill="var(--bg-elevated)" stroke={telemetry?.fault ? "var(--fault-red)" : "var(--accent-teal)"} strokeWidth="1.9" />
              <text x="40" y="26" textAnchor="middle" fill="var(--text-primary)" fontSize="13.5" fontWeight="750">Afore</text>
              <text x="40" y="42" textAnchor="middle" fill="var(--accent-teal)" fontSize="10.5" fontWeight="650">{invStatusLabel}</text>
            </g>

            {/* Mobile Right: Casa */}
            <g
              transform="translate(236, 102)"
              className={`energy-node ${activeNode === "home" ? "selected" : ""}`}
              role="button"
              tabIndex={0}
              aria-expanded={activeNode === "home"}
              aria-label={`Casa: ${load == null ? "Non disponibile" : fmtAbsKw(load)}, stato ${loadDirLabel}`}
              onClick={() => toggleNode("home")}
              onKeyDown={(e) => handleNodeKey("home", e)}
            >
              <rect width="102" height="84" rx="14" fill="var(--bg-surface)" stroke={load == null || isStale ? inactiveColor : "var(--home-color)"} strokeWidth="1.7" strokeDasharray="5 3" />
              <text x="10" y="22" fill="var(--text-secondary)" fontSize="11.5" fontWeight="650">Casa</text>
              <text x="10" y="47" fill={load == null || isStale ? inactiveColor : "var(--home-color)"} fontSize="16" fontWeight="750">{load == null ? "Non disp." : fmtAbsKw(load)}</text>
              <text x="10" y="68" fill="var(--text-muted)" fontSize="10.5">{loadDirLabel}</text>
              <QualityGlyph x={86} y={18} kind={loadQualityKind} />
            </g>

            {/* Mobile Bottom: Batteria */}
            <g
              transform="translate(32, 212)"
              className={`energy-node ${activeNode === "battery" ? "selected" : ""}`}
              role="button"
              tabIndex={0}
              aria-expanded={activeNode === "battery"}
              aria-label={`Batteria: SOC ${fmtPct(soc)}, potenza ${batt == null ? "Non disponibile" : fmtWatts(batt)}`}
              onClick={() => toggleNode("battery")}
              onKeyDown={(e) => handleNodeKey("battery", e)}
            >
              <rect width="276" height="80" rx="15" fill="var(--bg-surface)" stroke={batt == null && soc == null ? inactiveColor : isStale ? inactiveColor : "var(--battery-color)"} strokeWidth="1.7" />
              <text x="14" y="23" fill="var(--text-secondary)" fontSize="11.5" fontWeight="650">Batteria · {battDirLabel}</text>
              <text x="14" y="49" fill={batt == null || isStale ? inactiveColor : "var(--battery-color)"} fontSize="18" fontWeight="750">{batt == null ? "Non disp." : fmtWatts(batt)}</text>
              <text x="240" y="48" textAnchor="end" fill={soc == null ? inactiveColor : "var(--battery-color)"} fontSize="22" fontWeight="800">{soc != null ? fmtPct(soc) : "—"}</text>
              <QualityGlyph x={258} y={19} kind={battQualityKind} />
              <rect x="14" y="59" width="248" height="9" rx="4.5" fill="var(--bg-inset)" stroke="var(--border-subtle)" />
              {soc != null && (
                <rect x="14" y="59" width={Math.max(4, Math.min(248, (soc / 100) * 248))} height="9" rx="4.5" fill={isStale ? inactiveColor : "var(--battery-color)"} />
              )}
            </g>
          </svg>
        ) : (
          <svg
            viewBox="0 0 680 356"
            width="100%"
            className="energy-svg"
            role="img"
            aria-label={`Energy Monitor: Fotovoltaico ${pv == null ? "Non disponibile" : fmtAbsKw(pv)}, Rete ${fmtWatts(grid)}, Casa ${load == null ? "Non disponibile" : fmtAbsKw(load)}, Batteria ${fmtWatts(batt)}, SOC ${fmtPct(soc)}`}
          >
            <defs>
              <marker id="em-arr-pv" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={pvColor} />
              </marker>
              <marker id="em-arr-home" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={loadColor} />
              </marker>
              <marker id="em-arr-grid" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={gridColor} />
              </marker>
              <marker id="em-arr-batt" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
                <path d="M0 0L6 3L0 6Z" fill={battColor} />
              </marker>
            </defs>

            {/* Subtle OpenEMS-inspired radial orbital guide */}
            <circle
              cx="340"
              cy="178"
              r="122"
              fill="none"
              stroke="var(--border-subtle)"
              strokeWidth="1.5"
              strokeDasharray="4 6"
              opacity="0.85"
            />

            {/* Flow: PV -> Hub */}
            {renderFlowLine({ d: "M340 88 L340 142", color: pvColor, watts: pv, active: pvActive, markerId: "em-arr-pv" })}

            {/* Flow: Grid <-> Hub */}
            {renderFlowLine({
              d: gridExport ? "M286 178 L184 178" : "M180 178 L282 178",
              color: gridColor,
              watts: grid,
              active: gridActive,
              markerId: "em-arr-grid",
            })}

            {/* Flow: Hub -> Home (derived = discreet dashed) */}
            {renderFlowLine({ d: "M398 178 L496 178", color: loadColor, watts: load, active: loadActive, markerId: "em-arr-home", dashed: true })}

            {/* Flow: Hub <-> Battery */}
            {renderFlowLine({
              d: battDischarge ? "M340 256 L340 216" : "M340 214 L340 254",
              color: battColor,
              watts: batt,
              active: battActive,
              markerId: "em-arr-batt",
            })}

          {/* TOP NODE: Fotovoltaico */}
          <g
            transform="translate(222, 6)"
            className={`energy-node ${activeNode === "pv" ? "selected" : ""}`}
            role="button"
            tabIndex={0}
            aria-expanded={activeNode === "pv"}
            aria-label={`Fotovoltaico: ${pv == null ? "Non disponibile" : fmtAbsKw(pv)}, stato ${pvStateLabel}. Premi Invio per dettagli tecnici`}
            onClick={() => toggleNode("pv")}
            onKeyDown={(e) => handleNodeKey("pv", e)}
          >
            <rect
              width="236"
              height="80"
              rx="16"
              fill="var(--bg-surface)"
              stroke={pv == null || isStale ? inactiveColor : "var(--pv-color)"}
              strokeWidth={activeNode === "pv" ? 2.4 : 1.7}
            />
            <circle cx="32" cy="40" r="17" fill="var(--pv-soft)" />
            <g transform="translate(20,28)" stroke={pv == null || isStale ? inactiveColor : "var(--pv-color)"} strokeWidth="1.9" fill="none" strokeLinecap="round">
              <circle cx="12" cy="12" r="4" />
              <path d="M12 2v2M12 20v2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2 12h2M20 12h2M4.93 19.07l1.41-1.41M17.66 6.34l1.41-1.41" />
            </g>
            <text x="58" y="25" fill="var(--text-secondary)" fontSize="12" fontWeight="650">
              Fotovoltaico
            </text>
            <text x="58" y="51" fill={pv == null || isStale ? inactiveColor : "var(--pv-color)"} fontSize="22" fontWeight="750">
              {pv == null ? "Non disp." : fmtAbsKw(pv)}
            </text>
            <text x="58" y="69" fill="var(--text-muted)" fontSize="11.5">
              {pvStateLabel}
            </text>
            <QualityGlyph x={216} y={19} kind={pvQualityKind} />
          </g>

          {/* LEFT NODE: Rete */}
          <g
            transform="translate(8, 134)"
            className={`energy-node ${activeNode === "grid" ? "selected" : ""}`}
            role="button"
            tabIndex={0}
            aria-expanded={activeNode === "grid"}
            aria-label={`Rete: ${grid == null ? "Non disponibile" : fmtWatts(grid)}, stato ${gridDirLabel}. Premi Invio per dettagli tecnici`}
            onClick={() => toggleNode("grid")}
            onKeyDown={(e) => handleNodeKey("grid", e)}
          >
            <rect
              width="172"
              height="88"
              rx="16"
              fill="var(--bg-surface)"
              stroke={grid == null || isStale ? inactiveColor : "var(--grid-color)"}
              strokeWidth={activeNode === "grid" ? 2.4 : 1.7}
            />
            <g transform="translate(14,14)" stroke={grid == null || isStale ? inactiveColor : "var(--grid-color)"} strokeWidth="1.8" fill="none" strokeLinecap="round" strokeLinejoin="round">
              <path d="M9 2L4 18M9 2l5 16M5 10h8M3 6h12M6 14h6" />
            </g>
            <text x="36" y="27" fill="var(--text-secondary)" fontSize="12" fontWeight="650">
              Rete
            </text>
            <text x="16" y="54" fill={grid == null || isStale ? inactiveColor : "var(--grid-color)"} fontSize="20.5" fontWeight="750">
              {grid == null ? "Non disp." : fmtWatts(grid)}
            </text>
            <text x="16" y="73" fill="var(--text-muted)" fontSize="11.5">
              {gridDirLabel}
            </text>
            <QualityGlyph x={153} y={20} kind={gridQualityKind} />
          </g>

          {/* RIGHT NODE: Casa */}
          <g
            transform="translate(500, 134)"
            className={`energy-node ${activeNode === "home" ? "selected" : ""}`}
            role="button"
            tabIndex={0}
            aria-expanded={activeNode === "home"}
            aria-label={`Casa: ${load == null ? "Non disponibile" : fmtAbsKw(load)}, stato ${loadDirLabel}. Premi Invio per dettagli tecnici`}
            onClick={() => toggleNode("home")}
            onKeyDown={(e) => handleNodeKey("home", e)}
          >
            <rect
              width="172"
              height="88"
              rx="16"
              fill="var(--bg-surface)"
              stroke={load == null || isStale ? inactiveColor : "var(--home-color)"}
              strokeWidth={activeNode === "home" ? 2.4 : 1.7}
              strokeDasharray="5 3"
            />
            <g transform="translate(14,14)" stroke={load == null || isStale ? inactiveColor : "var(--home-color)"} strokeWidth="1.8" fill="none" strokeLinecap="round" strokeLinejoin="round">
              <path d="M2 9.5L9 3l7 6.5M4 8.5V16h10V8.5" />
            </g>
            <text x="36" y="27" fill="var(--text-secondary)" fontSize="12" fontWeight="650">
              Casa
            </text>
            <text x="16" y="54" fill={load == null || isStale ? inactiveColor : "var(--home-color)"} fontSize="20.5" fontWeight="750">
              {load == null ? "Non disp." : fmtAbsKw(load)}
            </text>
            <text x="16" y="73" fill="var(--text-muted)" fontSize="11.5">
              {loadDirLabel}
            </text>
            <QualityGlyph x={153} y={20} kind={loadQualityKind} />
          </g>

          {/* CENTER HUB: Inverter Afore */}
          <g
            transform="translate(288, 144)"
            className={`energy-node ${activeNode === "inverter" ? "selected" : ""}`}
            role="button"
            tabIndex={0}
            aria-expanded={activeNode === "inverter"}
            aria-label={`Inverter Afore: stato ${invStatusLabel}. Premi Invio per dettagli tecnici`}
            onClick={() => toggleNode("inverter")}
            onKeyDown={(e) => handleNodeKey("inverter", e)}
          >
            <rect
              width="104"
              height="68"
              rx="18"
              fill="var(--bg-elevated)"
              stroke={telemetry?.fault ? "var(--fault-red)" : "var(--accent-teal)"}
              strokeWidth={activeNode === "inverter" ? 2.6 : 2}
            />
            <text x="52" y="31" textAnchor="middle" fill="var(--text-primary)" fontSize="14.5" fontWeight="750">
              Afore
            </text>
            <text
              x="52"
              y="49"
              textAnchor="middle"
              fill={telemetry?.fault ? "var(--fault-red)" : isStale ? "var(--safety-amber)" : "var(--accent-teal)"}
              fontSize="11.5"
              fontWeight="650"
            >
              {invStatusLabel}
            </text>
          </g>

          {/* BOTTOM NODE: Batteria */}
          <g
            transform="translate(182, 258)"
            className={`energy-node ${activeNode === "battery" ? "selected" : ""}`}
            role="button"
            tabIndex={0}
            aria-expanded={activeNode === "battery"}
            aria-label={`Batteria: SOC ${fmtPct(soc)}, potenza ${batt == null ? "Non disponibile" : fmtWatts(batt)}, stato ${battDirLabel}. Premi Invio per dettagli tecnici`}
            onClick={() => toggleNode("battery")}
            onKeyDown={(e) => handleNodeKey("battery", e)}
          >
            <rect
              width="316"
              height="90"
              rx="16"
              fill="var(--bg-surface)"
              stroke={batt == null && soc == null ? inactiveColor : isStale ? inactiveColor : "var(--battery-color)"}
              strokeWidth={activeNode === "battery" ? 2.4 : 1.7}
            />
            <g transform="translate(16,15)" stroke={batt == null && soc == null ? inactiveColor : "var(--battery-color)"} strokeWidth="1.8" fill="none" strokeLinecap="round">
              <rect x="1" y="4" width="16" height="10" rx="2" />
              <path d="M19 7v4" />
            </g>
            <text x="41" y="27" fill="var(--text-secondary)" fontSize="12" fontWeight="650">
              Batteria · {battDirLabel}
            </text>
            <text
              x="16"
              y="55"
              fill={batt == null || isStale ? inactiveColor : "var(--battery-color)"}
              fontSize="21"
              fontWeight="750"
            >
              {batt == null ? "Non disp." : fmtWatts(batt)}
            </text>

            <text x="278" y="25" textAnchor="end" fill="var(--text-secondary)" fontSize="11" fontWeight="650">
              SOC
            </text>
            <text
              x="278"
              y="54"
              textAnchor="end"
              fill={soc == null ? inactiveColor : "var(--battery-color)"}
              fontSize="26"
              fontWeight="800"
            >
              {soc != null ? fmtPct(soc) : "—"}
            </text>
            <QualityGlyph x={296} y={20} kind={battQualityKind} />

            {/* Graphic battery fill bar */}
            <rect x="16" y="66" width="284" height="11" rx="5.5" fill="var(--bg-inset)" stroke="var(--border-subtle)" />
            {soc != null && (
              <rect
                x="16"
                y="66"
                width={socWidth}
                height="11"
                rx="5.5"
                fill={isStale ? inactiveColor : "var(--battery-color)"}
              />
            )}
          </g>
        </svg>
        )}

        {renderPopoverContent()}
      </div>
    </div>
  );
}
