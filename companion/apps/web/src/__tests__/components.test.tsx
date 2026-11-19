import { describe, expect, it } from "vitest";
import { render, screen } from "@testing-library/react";
import "@testing-library/jest-dom/vitest";
import { ShadowBanner } from "../components/ShadowBanner";
import { PowerFlow } from "../components/PowerFlow";
import { fmtKwh, fmtWatts } from "../lib/format";

describe("formatting preserves null without substituting zero", () => {
  it("renders Non disponibile for null or undefined", () => {
    expect(fmtWatts(null)).toBe("Non disponibile");
    expect(fmtWatts(undefined, "W")).toBe("Non disponibile");
    expect(fmtKwh(null)).toBe("Non disponibile");
    expect(fmtWatts(0, "W")).toBe("0 W");
  });
});

describe("ShadowBanner", () => {
  it("always states SHADOW and zero inverter commands", () => {
    render(<ShadowBanner />);
    expect(screen.getByText(/SHADOW – Decisioni simulate/i)).toBeInTheDocument();
    expect(screen.getByText(/Nessun comando applicato all'inverter/i)).toBeInTheDocument();
  });
});

describe("PowerFlow", () => {
  it("renders unavailable state cleanly when telemetry is null", () => {
    render(<PowerFlow telemetry={null} />);
    expect(screen.getAllByText(/Non disp\./i).length).toBeGreaterThanOrEqual(4);
  });

  it("renders measured and derived provenance labels for valid telemetry", () => {
    render(
      <PowerFlow
        telemetry={{
          age_s: 1.5,
          soc: 68,
          pv_w: 2860,
          pv1_w: 1420,
          pv2_w: 1440,
          load_w: 1940,
          grid_w: -210,
          battery_w: 710,
          battery_v: 52.9,
          battery_a: 13.4,
          quality: { pv_w: { state: "valid", source: "measured" }, load_w: { state: "valid", source: "derived" } },
        }}
        safetyMinW={-1000}
        safetyMaxW={1000}
        selfConsumptionPct={71}
      />
    );
    expect(screen.getByText(/FOTOVOLTAICO · MISURATO/i)).toBeInTheDocument();
    expect(screen.getByText(/CASA · DERIVATO AC/i)).toBeInTheDocument();
    expect(screen.getByText(/68%/i)).toBeInTheDocument();
  });
});
