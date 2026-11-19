import { describe, expect, it } from "vitest";
import { fireEvent, render, screen } from "@testing-library/react";
import "@testing-library/jest-dom/vitest";
import { ShadowBanner } from "../components/ShadowBanner";
import { PowerFlow } from "../components/PowerFlow";
import { fmtKwh, fmtWatts, humanDecisionDetail } from "../lib/format";

describe("formatting preserves null without substituting zero", () => {
  it("renders Non disponibile for null or undefined", () => {
    expect(fmtWatts(null)).toBe("Non disponibile");
    expect(fmtWatts(undefined, "W")).toBe("Non disponibile");
    expect(fmtKwh(null)).toBe("Non disponibile");
    expect(fmtWatts(0, "W")).toBe("0 W");
  });

  it("translates decision codes to human Italian copy", () => {
    const d = humanDecisionDetail("SOC_RESERVE", "GRID_BALANCE", 20, 0, 434);
    expect(d.title).toMatch(/Riserva SOC raggiunta/i);
    expect(d.subtitle).toMatch(/SOC 20%/i);
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

  it("keeps diagram clean of technical strings and reveals them in node popover on click", () => {
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
          hz: 50.01,
          quality: { pv_w: { state: "valid", source: "measured" }, load_w: { state: "valid", source: "derived" } },
        }}
        safetyMinW={-1000}
        safetyMaxW={1000}
        selfConsumptionPct={71}
      />
    );
    expect(screen.queryByText(/Misurato FC04/i)).not.toBeInTheDocument();
    expect(screen.queryByText(/Derivato AC/i)).not.toBeInTheDocument();
    expect(screen.getByText(/68%/i)).toBeInTheDocument();

    fireEvent.click(screen.getByRole("button", { name: /Batteria:/i }));
    expect(screen.getByRole("dialog", { name: /Dettaglio nodo · Batteria/i })).toBeInTheDocument();
    expect(screen.getByText(/Misurato FC04/i)).toBeInTheDocument();
  });
});
