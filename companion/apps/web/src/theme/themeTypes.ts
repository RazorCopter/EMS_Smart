export type ThemeStyle = "classic" | "emerald";
export type ColorSchemePreference = "dark" | "light" | "system";
export type ResolvedColorScheme = "dark" | "light";
export type UiDensity = "comfortable" | "compact";

export interface ThemePreferences {
  style: ThemeStyle;
  color_scheme: ColorSchemePreference;
  density: UiDensity;
  reduced_motion: boolean;
}

export const DEFAULT_THEME_PREFERENCES: ThemePreferences = {
  style: "classic",
  color_scheme: "dark",
  density: "comfortable",
  reduced_motion: false,
};

export interface ChartPalette {
  canvasBg: string;
  tooltipBg: string;
  tooltipBorder: string;
  textPrimary: string;
  textSecondary: string;
  axisLabel: string;
  splitLine: string;
  sliderBorder: string;
  pv: string;
  pvArea: string;
  home: string;
  battery: string;
  batteryArea: string;
  grid: string;
  soc: string;
  shadow: string;
  safety: string;
  sem: string;
  candidate: string;
}
