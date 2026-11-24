import { useRef, useState, type KeyboardEvent } from "react";
import { fmtAbsKw, fmtPct } from "../../lib/format";
import { buildEnergyFlowModel, type NodeId, type QualityKind } from "../shared/energyFlowModel";
import type { TelemetryLive } from "../PowerFlow";
import { EmeraldDetailDrawer, EmeraldStatusPill } from "./EmeraldChrome";

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
  return Number(Math.min(5.2, 2.4 + (abs / 3500) * 2.8).toFixed(2));
}

function flowSpeedSeconds(watts: number | null | undefined, deadband: number): string {
  const abs = Math.abs(watts ?? 0);
  if (abs <= deadband) return "1.9s";
  const dur = Math.max(0.7, Math.min(2.1, 2.1 - (abs / 3500) * 1.4));
  return `${dur.toFixed(2)}s`;
}

function shortPvState(pv: number | null, active: boolean): string {
  if (pv == null) return "Non disp.";
  return active ? "Produzione" : "Inattivo";
}

function shortGridState(grid: number | null, imp: boolean, exp: boolean): string {
  if (grid == null) return "Non disp.";
  if (imp) return "Prelievo";
  if (exp) return "Immissione";
  return "Equilibrio";
}

function shortHomeState(load: number | null, active: boolean): string {
  if (load == null) return "Non disp.";
  return active ? "Consumo" : "Standby";
}

function shortBattState(batt: number | null, chg: boolean, dis: boolean): string {
  if (batt == null) return "Non disp.";
  if (chg) return "In carica";
  if (dis) return "In scarica";
  return "Inattiva";
}

function formatSplitPower(val: number | null, signed = false): { num: string; unit: string } {
  const txt = signed && val != null && Math.abs(val) >= 1000
    ? `${val >= 0 ? "+" : "-"}${(Math.abs(val) / 1000).toFixed(2)} kW`
    : signed && val != null
    ? `${val >= 0 ? "+" : "-"}${Math.round(Math.abs(val))} W`
    : fmtAbsKw(val);

  if (txt === "Non disponibile") return { num: "N/D", unit: "" };
  const idx = txt.lastIndexOf(" ");
  if (idx === -1) return { num: txt, unit: "" };
  return { num: txt.slice(0, idx), unit: txt.slice(idx + 1) };
}

function QualityMicroGlyph({ cx, cy, kind }: { cx: number; cy: number; kind: QualityKind }) {
  const palette = {
    measured: { bg: "rgba(52, 214, 160, 0.2)", fg: "var(--battery-color)", glyph: "✓", title: "Dato misurato" },
    derived: { bg: "rgba(167, 139, 250, 0.22)", fg: "var(--home-color)", glyph: "≈", title: "Dato derivato" },
    stale: { bg: "rgba(245, 158, 11, 0.22)", fg: "var(--safety-amber)", glyph: "!", title: "Dato non aggiornato" },
    unavailable: { bg: "rgba(100, 116, 139, 0.22)", fg: "var(--missing-grey)", glyph: "○", title: "Dato assente" },
  }[kind];

  return (
    <g aria-hidden="true">
      <title>{palette.title}</title>
      <circle cx={cx} cy={cy} r="9" fill={palette.bg} stroke={palette.fg} strokeWidth="1" />
      <text x={cx} y={cy + 4} textAnchor="middle" fill={palette.fg} fontSize="12" fontWeight="800">
        {palette.glyph}
      </text>
    </g>
  );
}

function RadialFlowTrack({
  id,
  path,
  active,
  reverse,
  color,
  watts,
  deadband,
  dashed = false,
}: {
  id: string;
  path: string;
  active: boolean;
  reverse?: boolean;
  color: string;
  watts: number | null;
  deadband: number;
  dashed?: boolean;
}) {
  const width = flowStrokeWidth(watts, deadband);
  const dur = flowSpeedSeconds(watts, deadband);

  return (
    <g>
      <path d={path} fill="none" stroke="var(--border-subtle)" strokeWidth="2.4" strokeLinecap="round" opacity="0.55" />
      <path
        id={id}
        d={path}
        fill="none"
        stroke={active ? color : "var(--missing-grey)"}
        strokeWidth={active ? width : 2}
        strokeLinecap="round"
        strokeDasharray={dashed ? "5 5" : undefined}
        opacity={active ? 0.94 : 0.35}
      />
      {active && (
        <g className="flow-motion">
          <circle r="4.2" fill={color}>
            <animateMotion
              dur={dur}
              repeatCount="indefinite"
              keyPoints={reverse ? "1;0" : "0;1"}
              keyTimes="0;1"
              calcMode="linear"
            >
              <mpath href={`#${id}`} />
            </animateMotion>
          </circle>
          <circle r="2.6" fill={color} opacity="0.55">
            <animateMotion
              dur={dur}
              begin={`-${(parseFloat(dur) / 2).toFixed(2)}s`}
              repeatCount="indefinite"
              keyPoints={reverse ? "1;0" : "0;1"}
              keyTimes="0;1"
              calcMode="linear"
            >
              <mpath href={`#${id}`} />
            </animateMotion>
          </circle>
        </g>
      )}
    </g>
  );
}

function AforeInverterGlyph({
  x,
  y,
  scale = 1,
  statusColor,
}: {
  x: number;
  y: number;
  scale?: number;
  statusColor: string;
}) {
  return (
    <g transform={`translate(${x}, ${y}) scale(${scale})`} aria-hidden="true">
      {/* Main Afore chassis */}
      <rect x="2" y="1" width="44" height="48" rx="5" fill="#e6edf5" stroke="rgba(0, 240, 255, 0.45)" strokeWidth="1.2" />
      {/* Top red Afore sunburst */}
      <g fill="#e11d48">
        <rect x="23" y="4.5" width="1.8" height="4.2" rx="0.9" />
        <rect x="19.5" y="5.4" width="1.7" height="4" rx="0.8" transform="rotate(-24 20.3 7.4)" />
        <rect x="26.8" y="5.4" width="1.7" height="4" rx="0.8" transform="rotate(24 27.6 7.4)" />
        <rect x="16.5" y="7.4" width="1.6" height="3.8" rx="0.8" transform="rotate(-48 17.3 9.3)" />
        <rect x="29.9" y="7.4" width="1.6" height="3.8" rx="0.8" transform="rotate(48 30.7 9.3)" />
      </g>
      {/* Afore wordmark */}
      <text x="24" y="16.5" textAnchor="middle" fill="#0f172a" fontSize="6.5" fontWeight="900" letterSpacing="0.02em">
        Afore
      </text>
      {/* Dark control bezel + live status LCD */}
      <rect x="13.5" y="21" width="21" height="10" rx="2" fill="#0b111a" />
      <rect x="18" y="24.2" width="12" height="3.6" rx="0.7" fill={statusColor} opacity="0.92" />
      {/* Lower cover seam */}
      <line x1="2.5" y1="36.5" x2="45.5" y2="36.5" stroke="#94a3b8" strokeWidth="0.9" />
      {/* 4 bottom cable glands */}
      <rect x="8.5" y="49" width="4.2" height="4" rx="1" fill="#1e293b" />
      <rect x="16.5" y="49" width="4.2" height="4" rx="1" fill="#1e293b" />
      <rect x="27.3" y="49" width="4.2" height="4" rx="1" fill="#1e293b" />
      <rect x="35.3" y="49" width="4.2" height="4" rx="1" fill="#1e293b" />
    </g>
  );
}

export function EmeraldEnergyMonitor({
  telemetry,
  safetyMinW,
  safetyMaxW,
  selfConsumptionPct,
  deadbandW = 20,
}: Props) {
  const [selectedNode, setSelectedNode] = useState<NodeId | null>(null);
  const invLossEmaRef = useRef<number | null>(null);

  const flow = buildEnergyFlowModel(
    telemetry,
    deadbandW,
    safetyMinW,
    safetyMaxW,
    selfConsumptionPct,
  );

  // Representative smoothed inverter tare/conversion loss from live balance
  let invLossW: number | null = invLossEmaRef.current != null ? Math.round(invLossEmaRef.current) : null;
  if (flow.pv != null && flow.load != null && flow.batt != null && flow.grid != null) {
    const rawLoss = Math.round(flow.pv + flow.grid - flow.load - flow.batt);
    if (rawLoss >= 0 && rawLoss <= 1500) {
      invLossEmaRef.current =
        invLossEmaRef.current == null ? rawLoss : invLossEmaRef.current * 0.75 + rawLoss * 0.25;
      invLossW = Math.round(invLossEmaRef.current);
    }
  }

  const onNodeKey = (node: NodeId) => (e: KeyboardEvent<SVGGElement>) => {
    if (e.key === "Enter" || e.key === " ") {
      e.preventDefault();
      setSelectedNode((cur) => (cur === node ? null : node));
    }
  };

  const pvSplit = formatSplitPower(flow.pv, false);
  const gridSplit = formatSplitPower(flow.grid, true);
  const homeSplit = formatSplitPower(flow.load, false);
  const battSplit = formatSplitPower(flow.batt, true);

  const socClamped = flow.soc != null && Number.isFinite(flow.soc) ? Math.max(0, Math.min(100, flow.soc)) : 0;
  const battRingR = 39;
  const battRingCirc = 2 * Math.PI * battRingR;
  const battRingDash = (socClamped / 100) * battRingCirc;

  const activeSpec = selectedNode ? flow.specs[selectedNode] : null;
  const statusPillTone = !telemetry
    ? "warn"
    : flow.isStale
    ? "warn"
    : telemetry.fault
    ? "err"
    : "ok";
  const statusPillLabel = !telemetry
    ? "In attesa di telemetria"
    : flow.isStale
    ? "Dati non aggiornati"
    : telemetry.fault
    ? "Anomalia inverter"
    : "Flusso attivo in tempo reale";

  return (
    <div className="em-card em-monitor-card">
      <div className="em-monitor-header">
        <div>
          <h2 className="em-card-title">Energy Monitor</h2>
          <p className="em-card-sub">Flussi radiali istantanei · tocca un nodo per i dettagli elettrici</p>
        </div>
        <EmeraldStatusPill tone={statusPillTone} label={statusPillLabel} />
      </div>

      <p className="sr-only">{flow.accessibleSummary}</p>

      {/* DESKTOP RADIAL SEGMENTED RING + 4 ICONIC CIRCULAR NODES (NO RECTANGLES) */}
      <div className="em-monitor-desktop-wrap">
        <svg
          className="em-radial-svg"
          viewBox="0 0 660 410"
          role="group"
          aria-label="Diagramma radiale flussi energetici Emerald"
        >
          <defs>
            <radialGradient id="em-center-glow" cx="50%" cy="50%" r="50%">
              <stop offset="0%" stopColor="var(--accent-teal)" stopOpacity="0.07" />
              <stop offset="100%" stopColor="var(--accent-teal)" stopOpacity="0" />
            </radialGradient>
          </defs>

          {/* Outer segmented structural guide ring connecting the 4 quadrant arcs */}
          <circle
            cx="330"
            cy="205"
            r="156"
            fill="none"
            stroke="var(--accent-teal)"
            strokeWidth="1"
            strokeDasharray="2 10"
            opacity="0.32"
            className="em-hud-orbit"
          />
          <circle
            cx="330"
            cy="205"
            r="142"
            fill="none"
            stroke="var(--border-strong)"
            strokeWidth="1.2"
            strokeDasharray="6 7"
            opacity="0.75"
          />
          <circle cx="330" cy="205" r="96" fill="url(#em-center-glow)" />

          {/* Four clean radial arcs from iconic circular nodes toward center hub */}
          <RadialFlowTrack
            id="em-d-pv"
            path="M 330 98 C 330 124 330 140 330 156"
            active={flow.pvActive}
            color={flow.pvColor}
            watts={flow.pv}
            deadband={deadbandW}
          />
          <RadialFlowTrack
            id="em-d-grid"
            path="M 164 205 C 210 205 244 205 278 205"
            active={flow.gridActive}
            reverse={flow.gridExport}
            color={flow.gridColor}
            watts={flow.grid}
            deadband={deadbandW}
          />
          <RadialFlowTrack
            id="em-d-home"
            path="M 382 205 C 418 205 452 205 496 205"
            active={flow.loadActive}
            color={flow.loadColor}
            watts={flow.load}
            deadband={deadbandW}
            dashed={flow.loadQualityKind === "derived"}
          />
          <RadialFlowTrack
            id="em-d-batt"
            path="M 330 254 C 330 272 330 288 330 308"
            active={flow.battActive}
            reverse={flow.battDischarge}
            color={flow.battColor}
            watts={flow.batt}
            deadband={deadbandW}
          />

          {/* TOP ICONIC CIRCULAR NODE: FOTOVOLTAICO */}
          <g
            className={`em-circle-node ${selectedNode === "pv" ? "selected" : ""}`}
            role="button"
            tabIndex={0}
            aria-label={`Fotovoltaico: ${fmtAbsKw(flow.pv)}`}
            onClick={() => setSelectedNode((c) => (c === "pv" ? null : "pv"))}
            onKeyDown={onNodeKey("pv")}
          >
            <rect x="290" y="22" width="195" height="80" rx="14" fill="transparent" />
            <circle cx="330" cy="62" r="36" fill="var(--bg-surface-2)" stroke={flow.pvColor} strokeWidth="2.2" />
            <circle cx="330" cy="62" r="29" fill="var(--pv-soft)" stroke="none" />
            <g transform="translate(318, 50)" stroke={flow.pvColor} strokeWidth="1.9" fill="none" strokeLinecap="round">
              <circle cx="12" cy="12" r="4.5" fill={flow.pvColor} opacity="0.88" />
              <path d="M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M19.1 4.9L17 7M7 17l-2.1 2.1" />
            </g>
            <QualityMicroGlyph cx={366} cy={26} kind={flow.pvQualityKind} />
            <text x="380" y="45" fill="var(--text-secondary)" fontSize="12" fontWeight="700">
              Fotovoltaico
            </text>
            <text x="380" y="72" fill={flow.pvColor} fontSize="24" fontWeight="800" className="mono">
              {pvSplit.num}
              {pvSplit.unit && (
                <tspan fill="var(--text-muted)" fontSize="13" fontWeight="600">
                  {" "}
                  {pvSplit.unit}
                </tspan>
              )}
            </text>
            <text x="380" y="90" fill="var(--text-muted)" fontSize="12" fontWeight="600">
              {shortPvState(flow.pv, flow.pvActive)}
            </text>
          </g>

          {/* LEFT ICONIC CIRCULAR NODE: RETE */}
          <g
            className={`em-circle-node ${selectedNode === "grid" ? "selected" : ""}`}
            role="button"
            tabIndex={0}
            aria-label={`Rete elettrica: ${fmtAbsKw(flow.grid)}`}
            onClick={() => setSelectedNode((c) => (c === "grid" ? null : "grid"))}
            onKeyDown={onNodeKey("grid")}
          >
            <rect x="76" y="165" width="100" height="145" rx="14" fill="transparent" />
            <circle cx="126" cy="205" r="36" fill="var(--bg-surface-2)" stroke={flow.gridColor} strokeWidth="2.2" />
            <circle cx="126" cy="205" r="29" fill="var(--grid-soft)" stroke="none" />
            <g transform="translate(114, 192)" stroke={flow.gridColor} strokeWidth="1.8" fill="none" strokeLinecap="round" strokeLinejoin="round">
              <path d="M12 2v20M7 22l5-16 5 16M5 9h14M6.5 14h11" />
            </g>
            <QualityMicroGlyph cx={162} cy={169} kind={flow.gridQualityKind} />
            <text x="126" y="259" textAnchor="middle" fill="var(--text-secondary)" fontSize="12" fontWeight="700">
              Rete
            </text>
            <text x="126" y="285" textAnchor="middle" fill={flow.gridColor} fontSize="23" fontWeight="800" className="mono">
              {gridSplit.num}
              {gridSplit.unit && (
                <tspan fill="var(--text-muted)" fontSize="13" fontWeight="600">
                  {" "}
                  {gridSplit.unit}
                </tspan>
              )}
            </text>
            <text x="126" y="303" textAnchor="middle" fill="var(--text-muted)" fontSize="12" fontWeight="600">
              {shortGridState(flow.grid, flow.gridImport, flow.gridExport)}
            </text>
          </g>

          {/* RIGHT ICONIC CIRCULAR NODE: CASA */}
          <g
            className={`em-circle-node ${selectedNode === "home" ? "selected" : ""}`}
            role="button"
            tabIndex={0}
            aria-label={`Casa: ${fmtAbsKw(flow.load)}`}
            onClick={() => setSelectedNode((c) => (c === "home" ? null : "home"))}
            onKeyDown={onNodeKey("home")}
          >
            <rect x="484" y="165" width="100" height="145" rx="14" fill="transparent" />
            <circle cx="534" cy="205" r="36" fill="var(--bg-surface-2)" stroke={flow.loadColor} strokeWidth="2.2" />
            <circle cx="534" cy="205" r="29" fill="var(--home-soft)" stroke="none" />
            <g transform="translate(522, 193)" stroke={flow.loadColor} strokeWidth="1.9" fill="none" strokeLinecap="round" strokeLinejoin="round">
              <path d="M3 10.5L12 3l9 7.5V20a1 1 0 0 1-1 1h-5v-6H9v6H4a1 1 0 0 1-1-1z" />
            </g>
            <QualityMicroGlyph cx={570} cy={169} kind={flow.loadQualityKind} />
            <text x="534" y="259" textAnchor="middle" fill="var(--text-secondary)" fontSize="12" fontWeight="700">
              Casa
            </text>
            <text x="534" y="285" textAnchor="middle" fill={flow.loadColor} fontSize="23" fontWeight="800" className="mono">
              {homeSplit.num}
              {homeSplit.unit && (
                <tspan fill="var(--text-muted)" fontSize="13" fontWeight="600">
                  {" "}
                  {homeSplit.unit}
                </tspan>
              )}
            </text>
            <text x="534" y="303" textAnchor="middle" fill="var(--text-muted)" fontSize="12" fontWeight="600">
              {shortHomeState(flow.load, flow.loadActive)}
            </text>
          </g>

          {/* BOTTOM ICONIC CIRCULAR NODE: BATTERIA WITH SOC PROGRESS RING */}
          <g
            className={`em-circle-node ${selectedNode === "battery" ? "selected" : ""}`}
            role="button"
            tabIndex={0}
            aria-label={`Batteria: SOC ${fmtPct(flow.soc)}, potenza ${fmtAbsKw(flow.batt)}`}
            onClick={() => setSelectedNode((c) => (c === "battery" ? null : "battery"))}
            onKeyDown={onNodeKey("battery")}
          >
            <rect x="288" y="306" width="195" height="84" rx="14" fill="transparent" />
            <circle cx="330" cy="348" r="39" fill="none" stroke="var(--border-subtle)" strokeWidth="4.5" />
            {flow.soc != null && (
              <circle
                cx="330"
                cy="348"
                r={battRingR}
                fill="none"
                stroke={flow.battColor}
                strokeWidth="4.5"
                strokeLinecap="round"
                strokeDasharray={`${battRingDash.toFixed(1)} ${battRingCirc.toFixed(1)}`}
                transform="rotate(-90 330 348)"
              />
            )}
            <circle cx="330" cy="348" r="33" fill="var(--bg-surface-2)" stroke="none" />
            <circle cx="330" cy="348" r="27" fill="var(--battery-soft)" stroke="none" />
            <text x="330" y="346" textAnchor="middle" fill={flow.battColor} fontSize="14.5" fontWeight="800" className="mono">
              {flow.soc != null ? `${Math.round(flow.soc)}%` : "N/D"}
            </text>
            <text x="330" y="362" textAnchor="middle" fill="var(--text-muted)" fontSize="12" fontWeight="700">
              SOC
            </text>
            <QualityMicroGlyph cx={369} cy={309} kind={flow.battQualityKind} />

            <text x="382" y="330" fill="var(--text-secondary)" fontSize="12" fontWeight="700">
              Batteria
            </text>
            <text x="382" y="356" fill={flow.battColor} fontSize="23" fontWeight="800" className="mono">
              {battSplit.num}
              {battSplit.unit && (
                <tspan fill="var(--text-muted)" fontSize="13" fontWeight="600">
                  {" "}
                  {battSplit.unit}
                </tspan>
              )}
            </text>
            <text x="382" y="374" fill="var(--text-muted)" fontSize="12" fontWeight="600">
              {shortBattState(flow.batt, flow.battCharge, flow.battDischarge)}
            </text>
          </g>

          {/* CENTER HUB: AFORE INVERTER CIRCLE */}
          <g
            className={`em-circle-node ${selectedNode === "inverter" ? "selected" : ""}`}
            role="button"
            tabIndex={0}
            aria-label={`Inverter Afore: ${flow.invStatusLabel}`}
            onClick={() => setSelectedNode((c) => (c === "inverter" ? null : "inverter"))}
            onKeyDown={onNodeKey("inverter")}
          >
            <circle
              cx="330"
              cy="205"
              r="54"
              fill="var(--bg-surface)"
              stroke={telemetry?.fault ? "var(--fault-red)" : flow.isStale ? "var(--safety-amber)" : "var(--accent-teal)"}
              strokeWidth="2.4"
            />
            <circle cx="330" cy="205" r="46" fill="none" stroke="var(--border-subtle)" strokeWidth="1.2" strokeDasharray="3 4" />
            <AforeInverterGlyph
              x={306}
              y={162}
              scale={1}
              statusColor={telemetry?.fault ? "var(--fault-red)" : flow.isStale ? "var(--safety-amber)" : "var(--accent-teal)"}
            />
            <text
              x="330"
              y={invLossW != null ? "230" : "235"}
              textAnchor="middle"
              fill={telemetry?.fault ? "var(--fault-red)" : flow.isStale ? "var(--safety-amber)" : "var(--accent-teal)"}
              fontSize="11.5"
              fontWeight="700"
            >
              {flow.invStatusLabel}
            </text>
            {invLossW != null && (
              <text x="330" y="244" textAnchor="middle" fill="var(--text-muted)" fontSize="10" fontWeight="650" className="mono">
                {`Inv ~${invLossW} W`}
              </text>
            )}
          </g>
        </svg>
      </div>

      {/* MOBILE DEDICATED SIMPLIFIED SVG: 3 TOP CIRCULAR NODES (FV / RETE / CASA) + BATTERIA BELOW */}
      <div className="em-monitor-mobile-wrap">
        <svg
          className="em-radial-svg"
          viewBox="0 0 360 326"
          role="group"
          aria-label="Diagramma flussi energetici mobile Emerald"
        >
          <RadialFlowTrack
            id="em-m-pv"
            path="M 60 100 C 60 152 108 180 140 180"
            active={flow.pvActive}
            color={flow.pvColor}
            watts={flow.pv}
            deadband={deadbandW}
          />
          <RadialFlowTrack
            id="em-m-grid"
            path="M 180 100 L 180 140"
            active={flow.gridActive}
            reverse={flow.gridExport}
            color={flow.gridColor}
            watts={flow.grid}
            deadband={deadbandW}
          />
          <RadialFlowTrack
            id="em-m-home"
            path="M 300 100 C 300 152 252 180 220 180"
            active={flow.loadActive}
            color={flow.loadColor}
            watts={flow.load}
            deadband={deadbandW}
            dashed={flow.loadQualityKind === "derived"}
          />
          <RadialFlowTrack
            id="em-m-batt"
            path="M 180 220 C 180 236 144 238 144 252"
            active={flow.battActive}
            reverse={flow.battDischarge}
            color={flow.battColor}
            watts={flow.batt}
            deadband={deadbandW}
          />

          {/* Top Left: PV Circle (Labels cleanly above circle) */}
          <g
            className="em-circle-node"
            role="button"
            tabIndex={0}
            aria-label={`Fotovoltaico: ${fmtAbsKw(flow.pv)}`}
            onClick={() => setSelectedNode("pv")}
            onKeyDown={onNodeKey("pv")}
          >
            <rect x="24" y="4" width="72" height="96" rx="12" fill="transparent" />
            <circle cx="60" cy="72" r="30" fill="transparent" />
            <text x="60" y="16" textAnchor="middle" fill="var(--text-secondary)" fontSize="12" fontWeight="700">
              FV
            </text>
            <text x="60" y="34" textAnchor="middle" fill={flow.pvColor} fontSize="14.5" fontWeight="800" className="mono">
              {pvSplit.num} {pvSplit.unit}
            </text>
            <circle cx="60" cy="72" r="25" fill="var(--bg-surface-2)" stroke={flow.pvColor} strokeWidth="2.2" />
            <g transform="translate(48, 60)" stroke={flow.pvColor} strokeWidth="1.8" fill="none">
              <circle cx="12" cy="12" r="4" fill={flow.pvColor} />
              <path d="M12 2v3M12 19v3M2 12h3M19 12h3" />
            </g>
            <QualityMicroGlyph cx={94} cy={55} kind={flow.pvQualityKind} />
          </g>

          {/* Top Center: Grid Circle */}
          <g
            className="em-circle-node"
            role="button"
            tabIndex={0}
            aria-label={`Rete: ${fmtAbsKw(flow.grid)}`}
            onClick={() => setSelectedNode("grid")}
            onKeyDown={onNodeKey("grid")}
          >
            <rect x="144" y="4" width="72" height="96" rx="12" fill="transparent" />
            <circle cx="180" cy="72" r="30" fill="transparent" />
            <text x="180" y="16" textAnchor="middle" fill="var(--text-secondary)" fontSize="12" fontWeight="700">
              Rete
            </text>
            <text x="180" y="34" textAnchor="middle" fill={flow.gridColor} fontSize="14.5" fontWeight="800" className="mono">
              {gridSplit.num} {gridSplit.unit}
            </text>
            <circle cx="180" cy="72" r="25" fill="var(--bg-surface-2)" stroke={flow.gridColor} strokeWidth="2.2" />
            <g transform="translate(168, 60)" stroke={flow.gridColor} strokeWidth="1.8" fill="none">
              <path d="M12 2v20M7 22l5-16 5 16M5 9h14" />
            </g>
            <QualityMicroGlyph cx={214} cy={55} kind={flow.gridQualityKind} />
          </g>

          {/* Top Right: Home Circle */}
          <g
            className="em-circle-node"
            role="button"
            tabIndex={0}
            aria-label={`Casa: ${fmtAbsKw(flow.load)}`}
            onClick={() => setSelectedNode("home")}
            onKeyDown={onNodeKey("home")}
          >
            <rect x="264" y="4" width="72" height="96" rx="12" fill="transparent" />
            <circle cx="300" cy="72" r="30" fill="transparent" />
            <text x="300" y="16" textAnchor="middle" fill="var(--text-secondary)" fontSize="12" fontWeight="700">
              Casa
            </text>
            <text x="300" y="34" textAnchor="middle" fill={flow.loadColor} fontSize="14.5" fontWeight="800" className="mono">
              {homeSplit.num} {homeSplit.unit}
            </text>
            <circle cx="300" cy="72" r="25" fill="var(--bg-surface-2)" stroke={flow.loadColor} strokeWidth="2.2" />
            <g transform="translate(288, 60)" stroke={flow.loadColor} strokeWidth="1.8" fill="none">
              <path d="M3 10.5L12 3l9 7.5V20a1 1 0 0 1-1 1h-5v-6H9v6H4a1 1 0 0 1-1-1z" />
            </g>
            <QualityMicroGlyph cx={334} cy={55} kind={flow.loadQualityKind} />
          </g>

          {/* Center Afore Circle */}
          <g
            className="em-circle-node"
            role="button"
            tabIndex={0}
            aria-label={`Inverter Afore: ${flow.invStatusLabel}`}
            onClick={() => setSelectedNode("inverter")}
            onKeyDown={onNodeKey("inverter")}
          >
            <circle cx="180" cy="180" r="42" fill="var(--bg-surface)" stroke="var(--accent-teal)" strokeWidth="2.2" />
            <AforeInverterGlyph
              x={162}
              y={146}
              scale={0.76}
              statusColor={telemetry?.fault ? "var(--fault-red)" : flow.isStale ? "var(--safety-amber)" : "var(--accent-teal)"}
            />
            <text x="180" y={invLossW != null ? "200" : "205"} textAnchor="middle" fill="var(--accent-teal)" fontSize="10.5" fontWeight="700">
              {flow.invStatusLabel}
            </text>
            {invLossW != null && (
              <text x="180" y="212" textAnchor="middle" fill="var(--text-muted)" fontSize="9.5" fontWeight="650" className="mono">
                {`Inv ~${invLossW} W`}
              </text>
            )}
          </g>

          {/* Bottom Battery Circle with SOC ring */}
          <g
            className="em-circle-node"
            role="button"
            tabIndex={0}
            aria-label={`Batteria: SOC ${fmtPct(flow.soc)}, ${fmtAbsKw(flow.batt)}`}
            onClick={() => setSelectedNode("battery")}
            onKeyDown={onNodeKey("battery")}
          >
            <rect x="108" y="248" width="172" height="70" rx="14" fill="transparent" />
            <circle cx="144" cy="282" r="34" fill="transparent" />
            <circle cx="144" cy="282" r="30" fill="none" stroke="var(--border-subtle)" strokeWidth="4" />
            {flow.soc != null && (
              <circle
                cx="144"
                cy="282"
                r="30"
                fill="none"
                stroke={flow.battColor}
                strokeWidth="4"
                strokeLinecap="round"
                strokeDasharray={`${((socClamped / 100) * 2 * Math.PI * 30).toFixed(1)} ${(2 * Math.PI * 30).toFixed(1)}`}
                transform="rotate(-90 144 282)"
              />
            )}
            <circle cx="144" cy="282" r="24" fill="var(--bg-surface-2)" />
            <text x="144" y="287" textAnchor="middle" fill={flow.battColor} fontSize="14" fontWeight="800" className="mono">
              {flow.soc != null ? `${Math.round(flow.soc)}%` : "N/D"}
            </text>
            <QualityMicroGlyph cx={179} cy={253} kind={flow.battQualityKind} />
            <text x="192" y="278" fill={flow.battColor} fontSize="17" fontWeight="800" className="mono">
              {battSplit.num} {battSplit.unit}
            </text>
            <text x="192" y="297" fill="var(--text-muted)" fontSize="12" fontWeight="600">
              {shortBattState(flow.batt, flow.battCharge, flow.battDischarge)}
            </text>
          </g>
        </svg>
      </div>

      {/* Node Detail Drawer (Desktop slide-over, Mobile full-screen overlay) */}
      <EmeraldDetailDrawer
        open={Boolean(activeSpec)}
        title={activeSpec?.title ?? ""}
        subtitle="Misure elettriche e provenienza campione"
        accentColor={activeSpec?.accent}
        onClose={() => setSelectedNode(null)}
      >
        {activeSpec && (
          <div className="em-node-detail-list">
            {activeSpec.rows.map(([k, v]) => (
              <div key={k} className="em-node-detail-row">
                <span className="em-node-detail-k">{k}</span>
                <strong className="em-node-detail-v mono">{v}</strong>
              </div>
            ))}
          </div>
        )}
      </EmeraldDetailDrawer>
    </div>
  );
}
