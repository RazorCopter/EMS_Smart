import type { ThemeMeta } from "../../theme/themeRegistry";

export const EMERALD_THEME_META: ThemeMeta = {
  id: "emerald",
  name: "Emerald Control",
  tagline: "Design system dark-first emerald con Energy Monitor radiale",
  statusLabel: "Design System v2",
  preview: {
    canvas: "#071110",
    sidebar: "#081412",
    surface: "#0C1917",
    border: "#28423D",
    accent: "#34D6A0",
    pv: "#F7C948",
    battery: "#34D399",
    grid: "#4EA8FF",
    home: "#A78BFA",
    text: "#F3F8F6",
  },
};

export const EMERALD_LAYOUT = {
  energyMonitorVariant: "emerald-radial" as const,
  heroRatio: "1.62fr 1fr",
};
