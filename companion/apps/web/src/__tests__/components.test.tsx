import { describe, expect, it, vi } from "vitest";
import { MemoryRouter } from "react-router-dom";

vi.mock("../components/EChart", () => ({
  EChart: ({ ariaLabel }: { ariaLabel?: string }) => (
    <div data-testid="mock-echart" aria-label={ariaLabel} />
  ),
}));
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import "@testing-library/jest-dom/vitest";
import { ShadowBanner } from "../components/ShadowBanner";
import { PowerFlow } from "../components/PowerFlow";
import { ThemeProvider } from "../theme/ThemeProvider";
import { ThemeSettingsCard } from "../components/shared/ThemeControls";
import { ThemeErrorBoundary } from "../components/shared/ThemeErrorBoundary";
import { ClassicAppShell } from "../components/classic/ClassicAppShell";
import { ClassicDashboardLayout } from "../components/classic/ClassicDashboardLayout";
import { EmeraldAppShell } from "../components/emerald/EmeraldAppShell";
import { EmeraldDashboardLayout } from "../components/emerald/EmeraldDashboardLayout";
import { fmtKwh, fmtWatts, humanDecisionDetail } from "../lib/format";
import { buildDashboardViewModels, type LiveResponse } from "../viewmodels/dashboardViewModel";
import { getChartPalette } from "../theme/themeRegistry";

const SAMPLE_LIVE: LiveResponse = {
  generated_at: "2026-11-19T14:30:00Z",
  timezone: "Europe/Rome",
  telemetry: {
    age_s: 1.5,
    stale: false,
    fault: false,
    soc: 68,
    pv_w: 2860,
    pv1_w: 1420,
    pv1_v: 310,
    pv2_w: 1440,
    pv2_v: 312,
    load_w: 1940,
    grid_w: -210,
    grid_v: 232.5,
    hz: 50.01,
    battery_w: 710,
    battery_v: 52.9,
    battery_a: 13.4,
    temp_c: 34.2,
    reads: 1200,
    errors: 0,
    quality: { pv_w: { state: "valid", source: "measured" }, load_w: { state: "valid", source: "derived" } },
  },
  decision: {
    id: "42",
    seq: 4201,
    source_timestamp: "2026-11-19T14:30:00Z",
    configured_strategy: "SELF_USE",
    strategy: "SELF_USE",
    candidate_w: 710,
    limited_w: 710,
    simulated_w: 710,
    safety_min_w: -1000,
    safety_max_w: 2560,
    safety_reason: "NONE",
    reason: "GRID_BALANCE",
    soc: 68,
  },
  kpi_today: {
    pv_kwh: 14.6,
    load_kwh: 11.8,
    self_consumed_pv_kwh: 10.4,
    self_consumption_pct: 71.2,
    self_sufficiency_pct: 82.5,
    grid_import_kwh: 2.06,
    grid_export_kwh: 4.2,
    battery_charge_kwh: 6.2,
    battery_discharge_kwh: 4.9,
    battery_throughput_kwh: 11.1,
    equivalent_cycles: 1.08,
    data_availability_pct: 98.4,
  },
  kpi_yesterday: {
    pv_kwh: 13.0,
    load_kwh: 11.2,
    self_consumption_pct: 68.0,
    self_sufficiency_pct: 79.0,
    grid_import_kwh: 2.4,
    battery_charge_kwh: 5.5,
  },
  comparison_yesterday: { available: true, reason: "ok" },
  coverage: {
    first_sample_at: "2026-11-18T08:00:00Z",
    effective_monitor_start_at: "2026-11-18T08:00:00Z",
    monitoring_started_today: false,
    unmonitored_pre_install_seconds: 0,
    monitored_window_seconds: 50000,
    valid_seconds: 49200,
    gap_seconds: 0,
    invalid_samples: 0,
    unavailable_seconds: 0,
    coverage_since_first_sample_pct: 98.4,
  },
  sparklines: [
    { hour: "10:00", pv_kwh: 1.2, load_kwh: 0.7, chg_kwh: 0.5, imp_kwh: 0.1 },
    { hour: "11:00", pv_kwh: 2.4, load_kwh: 0.9, chg_kwh: 1.4, imp_kwh: 0.05 },
    { hour: "12:00", pv_kwh: 3.1, load_kwh: 1.1, chg_kwh: 1.8, imp_kwh: 0.02 },
  ],
  system: {
    components: {
      esp32: { status: "online", recorded_at: "2026-11-19T14:30:00Z", detail: {} },
      mqtt: { status: "online", recorded_at: "2026-11-19T14:30:00Z", detail: {} },
      home_assistant: { status: "online", recorded_at: "2026-11-19T14:30:00Z", detail: {} },
      modbus: { status: "online", recorded_at: "2026-11-19T14:30:00Z", detail: {} },
      ntp: { status: "online", recorded_at: "2026-11-19T14:30:00Z", detail: {} },
      aux1: { status: "online", recorded_at: "2026-11-19T14:30:00Z", detail: { aux1: "OFF" } },
    },
  },
  recent_events: [
    {
      id: 1,
      occurred_at: "2026-11-19T13:15:00Z",
      event_type: "STRATEGY_CHANGE",
      severity: "info",
      title: "Cambio strategia",
      detail: "Self Use attivo con surplus FV",
    },
  ],
  resources: {
    esp32: { cpu_pct: 12.4, heap_used_pct: 41.2, nvs_used_pct: 8.1 },
    database: { db_size_mb: 142.3 },
    containers: { api: { disk_used_pct: 22.0, disk_free_gb: 14.5 } },
  },
};

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
        telemetry={SAMPLE_LIVE.telemetry}
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

describe("Classic vs Emerald Structural Differentiation", () => {
  it("renders distinct shells and dashboard DOM templates sharing one ViewModel", () => {
    const vm = buildDashboardViewModels(SAMPLE_LIVE, [], getChartPalette("classic", "dark"), "6h");
    const mockUser = { username: "admin", role: "admin" as const, csrf_token: "abc" };

    const { container: classicShell } = render(
      <MemoryRouter>
        <ThemeProvider username={null}>
          <ClassicAppShell user={mockUser} system={SAMPLE_LIVE.system} onLogout={vi.fn()} />
        </ThemeProvider>
      </MemoryRouter>
    );
    expect(classicShell.querySelector('[data-ui-template="classic"]')).toBeInTheDocument();
    expect(classicShell.querySelector(".em-shell")).not.toBeInTheDocument();

    const { container: emeraldShell } = render(
      <MemoryRouter>
        <ThemeProvider username={null}>
          <EmeraldAppShell user={mockUser} system={SAMPLE_LIVE.system} onLogout={vi.fn()} />
        </ThemeProvider>
      </MemoryRouter>
    );
    expect(emeraldShell.querySelector('[data-ui-template="emerald"]')).toBeInTheDocument();
    expect(emeraldShell.querySelector(".em-shell")).toBeInTheDocument();
    expect(emeraldShell.querySelector(".em-nav-group-label")).toHaveTextContent(/Monitoraggio/i);

    const { container: classicDash } = render(
      <MemoryRouter>
        <ThemeProvider username={null}>
          <ClassicDashboardLayout vm={vm} chartRange="6h" onChangeChartRange={vi.fn()} />
        </ThemeProvider>
      </MemoryRouter>
    );
    expect(classicDash.querySelector(".grid-hero-v2")).toBeInTheDocument();
    expect(classicDash.querySelector(".em-row-hero")).not.toBeInTheDocument();

    const { container: emeraldDash } = render(
      <MemoryRouter>
        <ThemeProvider username={null}>
          <EmeraldDashboardLayout vm={vm} chartRange="6h" onChangeChartRange={vi.fn()} />
        </ThemeProvider>
      </MemoryRouter>
    );
    expect(emeraldDash.querySelector('[data-ui-template="emerald"]')).toBeInTheDocument();
    expect(emeraldDash.querySelector(".em-row-hero")).toBeInTheDocument();
    expect(emeraldDash.querySelector(".em-row-bottom")).toBeInTheDocument();
    expect(emeraldDash.querySelector(".em-decision-card")).toBeInTheDocument();
    expect(emeraldDash.querySelectorAll(".em-circle-node").length).toBeGreaterThanOrEqual(5);
    expect(emeraldDash.querySelector("rect.node-frame")).not.toBeInTheDocument();
    expect(emeraldDash.querySelectorAll(".em-seg-cell.active").length).toBe(7);
    expect(emeraldDash.querySelectorAll(".em-spark-svg polyline").length).toBe(4);
    expect(emeraldDash.querySelectorAll(".em-spark-svg polygon").length).toBe(4);
    expect(emeraldDash.querySelector(".em-monitor-mobile-wrap svg")).toBeInTheDocument();

    // Verify circular node click opens Emerald detail drawer with electrical specs
    const desktopBattNode = emeraldDash.querySelector('.em-monitor-desktop-wrap .em-circle-node[aria-label*="Batteria:"]');
    expect(desktopBattNode).toBeTruthy();
    fireEvent.click(desktopBattNode!);
    expect(screen.getByRole("dialog", { name: /Dettaglio nodo · Batteria/i })).toBeInTheDocument();
    expect(screen.getByText(/Misurato FC04/i)).toBeInTheDocument();
  });
});

describe("Multi-Theme Provider, Emerald Energy Monitor & Fallback", () => {
  it("defaults to Classic + Scuro and switches style, color scheme, density, and motion live", async () => {
    window.localStorage.clear();
    render(
      <ThemeProvider username={null}>
        <ThemeSettingsCard />
        <PowerFlow telemetry={{ age_s: 2, soc: 55, pv_w: 1800, load_w: 400, grid_w: -10, battery_w: 1390 }} />
      </ThemeProvider>
    );

    expect(document.documentElement.dataset.style).toBe("classic");
    expect(document.documentElement.dataset.theme).toBe("dark");

    fireEvent.click(screen.getByRole("radio", { name: /Emerald Control/i }));
    expect(document.documentElement.dataset.style).toBe("emerald");
    await waitFor(
      () => {
        expect(screen.getByText(/Flussi radiali istantanei/i)).toBeInTheDocument();
      },
      { timeout: 3000 },
    );

    fireEvent.click(screen.getByRole("radio", { name: /Chiaro/i }));
    expect(document.documentElement.dataset.theme).toBe("light");

    fireEvent.click(screen.getByRole("radio", { name: /Compatta/i }));
    expect(document.documentElement.dataset.density).toBe("compact");

    fireEvent.click(screen.getByRole("checkbox"));
    expect(document.documentElement.dataset.motion).toBe("reduced");

    fireEvent.click(screen.getByRole("button", { name: /Ripristina tema predefinito/i }));
    expect(document.documentElement.dataset.style).toBe("classic");
    expect(document.documentElement.dataset.theme).toBe("dark");
    expect(document.documentElement.dataset.density).toBe("comfortable");
  });

  it("recovers via ThemeErrorBoundary and offers fallback to Classic without crashing", () => {
    const spy = vi.spyOn(console, "error").mockImplementation(() => {});
    const onFallback = vi.fn();
    function BrokenEmerald(): JSX.Element {
      throw new Error("boom-emerald");
    }
    render(
      <ThemeErrorBoundary styleName="emerald" onFallbackToClassic={onFallback}>
        <BrokenEmerald />
      </ThemeErrorBoundary>
    );
    expect(screen.getByRole("alert")).toHaveTextContent(/Fallback automatico disponibile/i);
    fireEvent.click(screen.getByRole("button", { name: /Passa temporaneamente a Classic/i }));
    expect(onFallback).toHaveBeenCalledTimes(1);
    spy.mockRestore();
  });
});
