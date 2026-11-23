import {
  DEFAULT_THEME_PREFERENCES,
  type ResolvedColorScheme,
  type ThemePreferences,
} from "./themeTypes";

const STORAGE_PREFIX = "afore_theme_prefs_v2:";
const LAST_KEY = `${STORAGE_PREFIX}last`;
const LEGACY_KEY = "afore_companion_theme";

export function sanitizePreferences(raw: unknown): ThemePreferences {
  if (!raw || typeof raw !== "object") {
    return { ...DEFAULT_THEME_PREFERENCES };
  }
  const obj = raw as Record<string, unknown>;
  const style = obj.style === "emerald" ? "emerald" : "classic";
  const color_scheme =
    obj.color_scheme === "light" || obj.color_scheme === "system" || obj.color_scheme === "dark"
      ? obj.color_scheme
      : "dark";
  const density = obj.density === "compact" ? "compact" : "comfortable";
  const reduced_motion = Boolean(obj.reduced_motion);
  return { style, color_scheme, density, reduced_motion };
}

export function loadCachedPreferences(username?: string | null): ThemePreferences {
  if (typeof window === "undefined") {
    return { ...DEFAULT_THEME_PREFERENCES };
  }
  try {
    if (username) {
      const userRaw = window.localStorage.getItem(`${STORAGE_PREFIX}${username}`);
      if (userRaw) {
        return sanitizePreferences(JSON.parse(userRaw));
      }
      return { ...DEFAULT_THEME_PREFERENCES };
    }
    const lastRaw = window.localStorage.getItem(LAST_KEY);
    if (lastRaw) {
      return sanitizePreferences(JSON.parse(lastRaw));
    }
    const legacy = window.localStorage.getItem(LEGACY_KEY);
    if (legacy === "light") {
      return { ...DEFAULT_THEME_PREFERENCES, color_scheme: "light" };
    }
  } catch {
    // ignore storage errors and fall back safely
  }
  return { ...DEFAULT_THEME_PREFERENCES };
}

export function saveCachedPreferences(prefs: ThemePreferences, username?: string | null): void {
  if (typeof window === "undefined") return;
  try {
    const clean = sanitizePreferences(prefs);
    const json = JSON.stringify(clean);
    window.localStorage.setItem(LAST_KEY, json);
    if (username) {
      window.localStorage.setItem(`${STORAGE_PREFIX}${username}`, json);
    }
  } catch {
    // ignore quota errors
  }
}

export function resolveSystemScheme(): ResolvedColorScheme {
  if (typeof window === "undefined" || typeof window.matchMedia !== "function") {
    return "dark";
  }
  return window.matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark";
}

export function resolveSystemReducedMotion(): boolean {
  if (typeof window === "undefined" || typeof window.matchMedia !== "function") {
    return false;
  }
  return window.matchMedia("(prefers-reduced-motion: reduce)").matches;
}

export function applyThemeAttributes(
  prefs: ThemePreferences,
  resolvedScheme: ResolvedColorScheme,
  osReducedMotion: boolean,
): void {
  if (typeof document === "undefined") return;
  const root = document.documentElement;
  root.dataset.style = prefs.style;
  root.dataset.theme = resolvedScheme;
  root.dataset.density = prefs.density;
  root.dataset.motion = prefs.reduced_motion || osReducedMotion ? "reduced" : "normal";
  root.style.colorScheme = resolvedScheme;
}
