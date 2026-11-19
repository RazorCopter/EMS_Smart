import { type ReactNode, useCallback, useEffect, useMemo, useState } from "react";
import { apiFetch } from "../lib/api";
import { getChartPalette } from "./themeRegistry";
import {
  applyThemeAttributes,
  loadCachedPreferences,
  resolveSystemReducedMotion,
  resolveSystemScheme,
  sanitizePreferences,
  saveCachedPreferences,
} from "./themeStorage";
import {
  DEFAULT_THEME_PREFERENCES,
  type ResolvedColorScheme,
  type ThemePreferences,
} from "./themeTypes";
import { ThemeContext, type ThemeContextValue } from "./useTheme";

interface Props {
  username?: string | null;
  children: ReactNode;
}

export function ThemeProvider({ username, children }: Props) {
  const [preferences, setPreferences] = useState<ThemePreferences>(() => loadCachedPreferences(username));
  const [systemScheme, setSystemScheme] = useState<ResolvedColorScheme>(() => resolveSystemScheme());
  const [osReducedMotion, setOsReducedMotion] = useState<boolean>(() => resolveSystemReducedMotion());
  const [runtimeFallbackActive, setRuntimeFallbackActive] = useState(false);
  const [syncing, setSyncing] = useState(false);

  // Sync OS prefers-color-scheme and prefers-reduced-motion live
  useEffect(() => {
    if (typeof window === "undefined" || typeof window.matchMedia !== "function") return;
    const mqScheme = window.matchMedia("(prefers-color-scheme: light)");
    const mqMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
    const onScheme = () => setSystemScheme(mqScheme.matches ? "light" : "dark");
    const onMotion = () => setOsReducedMotion(mqMotion.matches);
    mqScheme.addEventListener("change", onScheme);
    mqMotion.addEventListener("change", onMotion);
    return () => {
      mqScheme.removeEventListener("change", onScheme);
      mqMotion.removeEventListener("change", onMotion);
    };
  }, []);

  // Load user-specific preference on auth state change (without wiping when anonymous)
  useEffect(() => {
    if (!username) {
      const cached = loadCachedPreferences(null);
      setPreferences(cached);
      setRuntimeFallbackActive(false);
      return;
    }

    const cachedUser = loadCachedPreferences(username);
    setPreferences(cachedUser);
    setRuntimeFallbackActive(false);

    let cancelled = false;
    apiFetch<ThemePreferences>("/api/v1/user/preferences")
      .then((serverPrefs) => {
        if (cancelled) return;
        const clean = sanitizePreferences(serverPrefs);
        setPreferences(clean);
        saveCachedPreferences(clean, username);
      })
      .catch(() => {
        // Keep local cache / default if API unavailable
      });

    return () => {
      cancelled = true;
    };
  }, [username]);

  const effectiveStyle = runtimeFallbackActive ? "classic" : preferences.style;
  const resolvedScheme: ResolvedColorScheme =
    preferences.color_scheme === "system" ? systemScheme : preferences.color_scheme;
  const reducedMotion = preferences.reduced_motion || osReducedMotion;

  useEffect(() => {
    const effectivePrefs: ThemePreferences = { ...preferences, style: effectiveStyle };
    applyThemeAttributes(effectivePrefs, resolvedScheme, osReducedMotion);
  }, [preferences, effectiveStyle, resolvedScheme, osReducedMotion]);

  const persist = useCallback(
    (next: ThemePreferences) => {
      const clean = sanitizePreferences(next);
      setPreferences(clean);
      setRuntimeFallbackActive(false);
      saveCachedPreferences(clean, username);
      if (username) {
        setSyncing(true);
        apiFetch<ThemePreferences>("/api/v1/user/preferences", {
          method: "PUT",
          body: JSON.stringify(clean),
        })
          .catch(() => {
            // Keep localStorage cache if offline; next session reconciles
          })
          .finally(() => setSyncing(false));
      }
    },
    [username],
  );

  const updatePreferences = useCallback(
    (patch: Partial<ThemePreferences>) => {
      persist({ ...preferences, ...patch });
    },
    [preferences, persist],
  );

  const toggleColorScheme = useCallback(() => {
    const nextScheme: ResolvedColorScheme = resolvedScheme === "dark" ? "light" : "dark";
    updatePreferences({ color_scheme: nextScheme });
  }, [resolvedScheme, updatePreferences]);

  const resetToDefault = useCallback(() => {
    persist({ ...DEFAULT_THEME_PREFERENCES });
  }, [persist]);

  const activateTemporaryClassicFallback = useCallback(() => {
    setRuntimeFallbackActive(true);
  }, []);

  const clearTemporaryFallback = useCallback(() => {
    setRuntimeFallbackActive(false);
  }, []);

  const chartPalette = useMemo(
    () => getChartPalette(effectiveStyle, resolvedScheme),
    [effectiveStyle, resolvedScheme],
  );

  const value = useMemo<ThemeContextValue>(
    () => ({
      preferences,
      effectiveStyle,
      resolvedScheme,
      reducedMotion,
      chartPalette,
      syncing,
      runtimeFallbackActive,
      updatePreferences,
      toggleColorScheme,
      resetToDefault,
      activateTemporaryClassicFallback,
      clearTemporaryFallback,
    }),
    [
      preferences,
      effectiveStyle,
      resolvedScheme,
      reducedMotion,
      chartPalette,
      syncing,
      runtimeFallbackActive,
      updatePreferences,
      toggleColorScheme,
      resetToDefault,
      activateTemporaryClassicFallback,
      clearTemporaryFallback,
    ],
  );

  return <ThemeContext.Provider value={value}>{children}</ThemeContext.Provider>;
}
