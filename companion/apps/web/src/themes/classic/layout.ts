import type { ThemeMeta } from "../../theme/themeRegistry";

export const CLASSIC_THEME_META: ThemeMeta = {
  id: "classic",
  name: "Classic",
  tagline: "Interfaccia operativa originale congelata come baseline stabile",
  statusLabel: "Predefinito",
  preview: {
    canvas: "#091011",
    sidebar: "#0d1618",
    surface: "#111d1f",
    border: "#2d464a",
    accent: "#2dd4bf",
    pv: "#f5b942",
    battery: "#34d399",
    grid: "#60a5fa",
    home: "#a78bfa",
    text: "#eef5f3",
  },
};

export const CLASSIC_LAYOUT = {
  energyMonitorVariant: "classic" as const,
  heroRatio: "1.55fr 1fr",
};
