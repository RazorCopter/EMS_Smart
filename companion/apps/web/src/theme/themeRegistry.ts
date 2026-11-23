import type { ChartPalette, ResolvedColorScheme, ThemeStyle } from "./themeTypes";
import { CLASSIC_THEME_META } from "../themes/classic/layout";
import { EMERALD_THEME_META } from "../themes/emerald/layout";

export interface ThemeMeta {
  id: ThemeStyle;
  name: string;
  tagline: string;
  statusLabel: string;
  preview: {
    canvas: string;
    sidebar: string;
    surface: string;
    border: string;
    accent: string;
    pv: string;
    battery: string;
    grid: string;
    home: string;
    text: string;
  };
}

export const THEME_REGISTRY: Record<ThemeStyle, ThemeMeta> = {
  classic: CLASSIC_THEME_META,
  emerald: EMERALD_THEME_META,
};

export function getChartPalette(style: ThemeStyle, scheme: ResolvedColorScheme): ChartPalette {
  if (style === "emerald" && scheme === "dark") {
    return {
      canvasBg: "#071110",
      tooltipBg: "#11211E",
      tooltipBorder: "#28423D",
      textPrimary: "#F3F8F6",
      textSecondary: "#A9BDB7",
      axisLabel: "#71857F",
      splitLine: "#1A312C",
      sliderBorder: "#28423D",
      pv: "#F7C948",
      pvArea: "rgba(247, 201, 72, 0.11)",
      home: "#A78BFA",
      battery: "#34D399",
      batteryArea: "rgba(52, 211, 153, 0.12)",
      grid: "#4EA8FF",
      soc: "#F3F8F6",
      shadow: "#38BDF8",
      safety: "#F59E0B",
      sem: "#C084FC",
      candidate: "#8FA49E",
    };
  }
  if (style === "emerald" && scheme === "light") {
    return {
      canvasBg: "#F4F7F5",
      tooltipBg: "#FAFCFB",
      tooltipBorder: "#9EB8AD",
      textPrimary: "#0D1F1B",
      textSecondary: "#324D47",
      axisLabel: "#4D6862",
      splitLine: "#D2DFD9",
      sliderBorder: "#A7BEB4",
      pv: "#B45309",
      pvArea: "rgba(180, 83, 9, 0.10)",
      home: "#6D28D9",
      battery: "#047857",
      batteryArea: "rgba(4, 120, 87, 0.11)",
      grid: "#1D4ED8",
      soc: "#0D1F1B",
      shadow: "#0369A1",
      safety: "#B45309",
      sem: "#7E22CE",
      candidate: "#526660",
    };
  }
  if (scheme === "light") {
    return {
      canvasBg: "#f1f5f4",
      tooltipBg: "#ffffff",
      tooltipBorder: "#adc2bf",
      textPrimary: "#0f201e",
      textSecondary: "#3f5a57",
      axisLabel: "#5a7471",
      splitLine: "#d5e1df",
      sliderBorder: "#adc2bf",
      pv: "#b45309",
      pvArea: "rgba(180, 83, 9, 0.12)",
      home: "#6d28d9",
      battery: "#047857",
      batteryArea: "rgba(4, 120, 87, 0.12)",
      grid: "#1d4ed8",
      soc: "#0f201e",
      shadow: "#0369a1",
      safety: "#b45309",
      sem: "#7e22ce",
      candidate: "#64748b",
    };
  }
  // Classic + Dark (exact frozen baseline)
  return {
    canvasBg: "#091011",
    tooltipBg: "#0e191b",
    tooltipBorder: "#2d464a",
    textPrimary: "#eef5f3",
    textSecondary: "#98b0ad",
    axisLabel: "#6e8683",
    splitLine: "#1f3134",
    sliderBorder: "#1f3134",
    pv: "#f5b942",
    pvArea: "rgba(245, 185, 66, 0.16)",
    home: "#a78bfa",
    battery: "#34d399",
    batteryArea: "rgba(52, 211, 153, 0.14)",
    grid: "#60a5fa",
    soc: "#eef5f3",
    shadow: "#38bdf8",
    safety: "#f59e0b",
    sem: "#c084fc",
    candidate: "#94a3b8",
  };
}
