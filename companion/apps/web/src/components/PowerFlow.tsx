import { fmtAbsKw, fmtAge, fmtNum, fmtPct, fmtWatts } from "../lib/format";

interface TelemetryLive {
  age_s?: number | null;
  stale?: boolean;
  soc?: number | null;
  battery_w?: number | null;
  battery_v?: number | null;
  battery_a?: number | null;
  pv_w?: number | null;
  pv1_w?: number | null;
  pv2_w?: number | null;
  grid_w?: number | null;
  grid_v?: number | null;
  hz?: number | null;
  load_w?: number | null;
  temp_c?: number | null;
  quality?: Record<string, { age_ms?: number | null; source?: string; state?: string }>;
}

interface Props {
  telemetry: TelemetryLive | null;
  safetyMinW?: number | null;
  safetyMaxW?: number | null;
  selfConsumptionPct?: number | null;
  deadbandW?: number;
}

function edgeWidth(watts: number | null | undefined): number {
  if (watts == null) return 2;
  const abs = Math.abs(watts);
  if (abs <= 20) return 2;
  return Math.min(5.5, 2 + (abs / 3500) * 3.5);
}

function edgeClass(watts: number | null | undefined, deadband: number, reverse = false): string {
  if (watts == null || Math.abs(watts) <= deadband) return "flow-edge";
  return reverse ? "flow-edge animated reverse" : "flow-edge animated";
}

export function PowerFlow({
  telemetry,
  safetyMinW,
  safetyMaxW,
  selfConsumptionPct,
  deadbandW = 20,
}: Props) {
  const pv = telemetry?.pv_w ?? null;
  const load = telemetry?.load_w ?? null;
  const grid = telemetry?.grid_w ?? null;
  const batt = telemetry?.battery_w ?? null;
  const soc = telemetry?.soc ?? null;
  const ageText = telemetry ? fmtAge(telemetry.age_s) : "Nessun dato";

  const pvActive = pv != null && pv > deadbandW;
  const loadActive = load != null && load > deadbandW;
  const gridImport = grid != null && grid > deadbandW;
  const gridExport = grid != null && grid < -deadbandW;
  const battCharge = batt != null && batt > deadbandW;
  const battDischarge = batt != null && batt < -deadbandW;

  const pvColor = pv == null ? "var(--missing-grey)" : pvActive ? "var(--pv-color)" : "#505d5c";
  const loadColor = load == null ? "var(--missing-grey)" : loadActive ? "var(--home-color)" : "#505d5c";
  const gridColor =
    grid == null ? "var(--missing-grey)" : gridImport || gridExport ? "var(--grid-color)" : "#505d5c";
  const battColor =
    batt == null ? "var(--missing-grey)" : battCharge || battDischarge ? "var(--battery-color)" : "#505d5c";

  const battDir =
    batt == null ? "Non disponibile" : battCharge ? "In carica" : battDischarge ? "In scarica" : "Inattiva";
  const gridDir =
    grid == null ? "Non disponibile" : gridImport ? "Importazione" : gridExport ? "Esportazione" : "In equilibrio";

  const capLabel =
    safetyMinW != null && safetyMaxW != null
      ? `Limiti ${Math.round(safetyMinW)} / ${Math.round(safetyMaxW)} W`
      : "Limiti non disponibili";

  const pvQuality = telemetry?.quality?.pv_w?.state ?? (pv != null ? "valid" : "unavailable");
  const loadSource = telemetry?.quality?.load_w?.source ?? "derived";

  return (
    <div>
      <div className="card-header">
        <div>
          <h2 className="card-title">Flusso di Potenza Istantaneo</h2>
          <p className="card-sub">
            Deadband ±{deadbandW} W · Tratteggiato = derivato · Grigio = non disponibile · Aggiornato: {ageText}
          </p>
        </div>
        <div style={{ display: "flex", gap: 6, flexWrap: "wrap" }}>
          <span className={`badge ${pvQuality === "valid" ? "measured" : "unavailable"}`}>
            FV/Rete/Batt: {pvQuality === "valid" ? "Misurato FC04" : "Non disp."}
          </span>
          <span className="badge derived">Casa: {loadSource === "derived" ? "Derivato AC" : loadSource}</span>
        </div>
      </div>

      <div
        style={{
          background: "var(--bg-inset)",
          border: "1px solid var(--border-subtle)",
          borderRadius: 12,
          padding: 14,
        }}
      >
        <svg
          viewBox="0 0 700 296"
          width="100%"
          style={{ display: "block", maxHeight: 280 }}
          role="img"
          aria-label={`Flusso potenza: FV ${fmtAbsKw(pv)}, Casa ${fmtAbsKw(load)}, Rete ${fmtWatts(grid)}, Batteria ${fmtWatts(batt)}, SOC ${fmtPct(soc)}`}
        >
          <defs>
            <marker id="pf-arr-pv" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
              <path d="M0 0L6 3L0 6Z" fill={pvColor} />
            </marker>
            <marker id="pf-arr-home" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
              <path d="M0 0L6 3L0 6Z" fill={loadColor} />
            </marker>
            <marker id="pf-arr-grid" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
              <path d="M0 0L6 3L0 6Z" fill={gridColor} />
            </marker>
            <marker id="pf-arr-batt" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
              <path d="M0 0L6 3L0 6Z" fill={battColor} />
            </marker>
          </defs>

          {/* PV -> Inverter */}
          <path
            d="M135 90 L135 147 L300 147"
            className={edgeClass(pv, deadbandW)}
            stroke={pvColor}
            strokeWidth={edgeWidth(pv)}
            markerEnd={pvActive ? "url(#pf-arr-pv)" : undefined}
          />

          {/* Inverter -> Home (derived = dashed) */}
          <path
            d="M350 118 L350 92"
            className={edgeClass(load, deadbandW)}
            stroke={loadColor}
            strokeWidth={edgeWidth(load)}
            strokeDasharray="6 5"
            markerEnd={loadActive ? "url(#pf-arr-home)" : undefined}
          />

          {/* Grid <-> Inverter */}
          {gridImport ? (
            <path
              d="M565 90 L565 147 L400 147"
              className={edgeClass(grid, deadbandW)}
              stroke={gridColor}
              strokeWidth={edgeWidth(grid)}
              markerEnd="url(#pf-arr-grid)"
            />
          ) : (
            <path
              d="M400 147 L565 147 L565 90"
              className={edgeClass(grid, deadbandW)}
              stroke={gridColor}
              strokeWidth={edgeWidth(grid)}
              markerEnd={gridExport ? "url(#pf-arr-grid)" : undefined}
            />
          )}

          {/* Inverter <-> Battery */}
          {battDischarge ? (
            <path
              d="M350 206 L350 179"
              className={edgeClass(batt, deadbandW)}
              stroke={battColor}
              strokeWidth={edgeWidth(batt)}
              markerEnd="url(#pf-arr-batt)"
            />
          ) : (
            <path
              d="M350 177 L350 204"
              className={edgeClass(batt, deadbandW)}
              stroke={battColor}
              strokeWidth={edgeWidth(batt)}
              markerEnd={battCharge ? "url(#pf-arr-batt)" : undefined}
            />
          )}

          {/* PV Node */}
          <g transform="translate(22,12)">
            <rect
              width="224"
              height="78"
              rx="14"
              fill="var(--bg-surface)"
              stroke={pv == null ? "var(--missing-grey)" : "var(--pv-color)"}
              strokeWidth="1.5"
            />
            <text x="14" y="24" fill="var(--text-secondary)" fontSize="11" fontWeight="650">
              FOTOVOLTAICO · MISURATO
            </text>
            <text x="14" y="52" fill={pv == null ? "var(--missing-grey)" : "var(--pv-color)"} fontSize="20" fontWeight="700">
              {pv == null ? "Non disp." : fmtAbsKw(pv)}
            </text>
            <text x="122" y="45" fill="var(--text-muted)" fontSize="11">
              PV1: {telemetry?.pv1_w != null ? `${Math.round(telemetry.pv1_w)} W` : "—"}
            </text>
            <text x="122" y="61" fill="var(--text-muted)" fontSize="11">
              PV2: {telemetry?.pv2_w != null ? `${Math.round(telemetry.pv2_w)} W` : "—"}
            </text>
          </g>

          {/* Home Node */}
          <g transform="translate(238,12)">
            <rect
              width="224"
              height="78"
              rx="14"
              fill="var(--bg-surface)"
              stroke={load == null ? "var(--missing-grey)" : "var(--home-color)"}
              strokeWidth="1.5"
              strokeDasharray="5 3"
            />
            <text x="14" y="24" fill="var(--text-secondary)" fontSize="11" fontWeight="650">
              CASA · DERIVATO AC
            </text>
            <text x="14" y="52" fill={load == null ? "var(--missing-grey)" : "var(--home-color)"} fontSize="20" fontWeight="700">
              {load == null ? "Non disp." : fmtAbsKw(load)}
            </text>
            <text x="122" y="45" fill="var(--battery-color)" fontSize="11" fontWeight="600">
              Autocons: {selfConsumptionPct != null ? fmtPct(selfConsumptionPct) : "—"}
            </text>
            <text x="122" y="61" fill="var(--text-muted)" fontSize="11">
              {fmtNum(telemetry?.grid_v, "V", 1)} · {fmtNum(telemetry?.hz, "Hz", 2)}
            </text>
          </g>

          {/* Grid Node */}
          <g transform="translate(454,12)">
            <rect
              width="224"
              height="78"
              rx="14"
              fill="var(--bg-surface)"
              stroke={grid == null ? "var(--missing-grey)" : "var(--grid-color)"}
              strokeWidth="1.5"
            />
            <text x="14" y="24" fill="var(--text-secondary)" fontSize="11" fontWeight="650">
              RETE · {gridDir.toUpperCase()}
            </text>
            <text x="14" y="52" fill={grid == null ? "var(--missing-grey)" : "var(--grid-color)"} fontSize="20" fontWeight="700">
              {grid == null ? "Non disp." : fmtWatts(grid)}
            </text>
            <text x="126" y="45" fill="var(--text-muted)" fontSize="11">
              {gridImport ? "+ Prelievo" : gridExport ? "- Immissione" : "Equilibrio"}
            </text>
            <text x="126" y="61" fill="var(--text-muted)" fontSize="11">
              {ageText}
            </text>
          </g>

          {/* Inverter Hub */}
          <g transform="translate(300,118)">
            <rect width="100" height="58" rx="14" fill="var(--bg-elevated)" stroke="var(--accent-teal)" strokeWidth="1.6" />
            <text x="50" y="26" textAnchor="middle" fill="var(--text-primary)" fontSize="13" fontWeight="700">
              Afore
            </text>
            <text x="50" y="44" textAnchor="middle" fill="var(--accent-teal)" fontSize="11">
              {telemetry?.temp_c != null ? fmtNum(telemetry.temp_c, "°C", 1) : "Inverter"}
            </text>
          </g>

          {/* Battery Node */}
          <g transform="translate(170,206)">
            <rect
              width="360"
              height="78"
              rx="14"
              fill="var(--bg-surface)"
              stroke={batt == null ? "var(--missing-grey)" : "var(--battery-color)"}
              strokeWidth="1.5"
            />
            <text x="16" y="24" fill="var(--text-secondary)" fontSize="11" fontWeight="650">
              BATTERIA · {battDir.toUpperCase()}
            </text>
            <text x="16" y="53" fill={batt == null ? "var(--missing-grey)" : "var(--battery-color)"} fontSize="20" fontWeight="700">
              {batt == null ? "Non disp." : fmtWatts(batt)}
            </text>
            <text x="158" y="24" fill="var(--text-secondary)" fontSize="11">
              SOC
            </text>
            <text x="344" y="24" textAnchor="end" fill="var(--text-primary)" fontSize="14" fontWeight="700">
              {soc != null ? fmtPct(soc) : "Non disp."}
            </text>
            <rect x="158" y="33" width="186" height="10" rx="5" fill="var(--bg-inset)" stroke="var(--border-subtle)" />
            {soc != null && (
              <rect
                x="158"
                y="33"
                width={Math.max(4, Math.min(186, (soc / 100) * 186))}
                height="10"
                rx="5"
                fill="var(--battery-color)"
              />
            )}
            <text x="158" y="61" fill="var(--text-muted)" fontSize="11">
              {fmtNum(telemetry?.battery_v, "V", 1)} · {fmtNum(telemetry?.battery_a, "A", 1)} · {capLabel}
            </text>
          </g>
        </svg>
      </div>
    </div>
  );
}
