import { createContext, useContext } from "react";
import { getChartPalette } from "./themeRegistry";
import {
  DEFAULT_THEME_PREFERENCES,
  type ChartPalette,
  type ResolvedColorScheme,
  type ThemePreferences,
  type ThemeStyle,
} from "./themeTypes";

export interface ThemeContextValue {
  preferences: ThemePreferences;
  effectiveStyle: ThemeStyle;
  resolvedScheme: ResolvedColorScheme;
  reducedMotion: boolean;
  chartPalette: ChartPalette;
  syncing: boolean;
  runtimeFallbackActive: boolean;
  updatePreferences: (patch: Partial<ThemePreferences>) => void;
  toggleColorScheme: () => void;
  resetToDefault: () => void;
  activateTemporaryClassicFallback: () => void;
  clearTemporaryFallback: () => void;
}

export const ThemeContext = createContext<ThemeContextValue | null>(null);

const FALLBACK_THEME_CTX: ThemeContextValue = {
  preferences: DEFAULT_THEME_PREFERENCES,
  effectiveStyle: "classic",
  resolvedScheme: "dark",
  reducedMotion: false,
  chartPalette: getChartPalette("classic", "dark"),
  syncing: false,
  runtimeFallbackActive: false,
  updatePreferences: () => {},
  toggleColorScheme: () => {},
  resetToDefault: () => {},
  activateTemporaryClassicFallback: () => {},
  clearTemporaryFallback: () => {},
};

export function useOptionalTheme(): ThemeContextValue {
  return useContext(ThemeContext) ?? FALLBACK_THEME_CTX;
}

export function useTheme(): ThemeContextValue {
  const ctx = useContext(ThemeContext);
  if (!ctx) {
    throw new Error("useTheme deve essere utilizzato all'interno di ThemeProvider");
  }
  return ctx;
}
