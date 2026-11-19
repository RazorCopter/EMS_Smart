# Visual Review & Multi-Theme Refactor Plan (`Mission 8`)

- **Baseline monorepo commit**: `513a353` (`offline-full-product-completion`)
- **Baseline standalone repo (`RazorCopter/dashboard_ems` `main`)**: `4a9e9e0c1ed48e7f85afe727ebcd3cabe79a3688`
- **Default primo deploy**: `classic` + `dark` (Classic + Scuro invariato)

## 1. Differenze Classic vs Emerald Control

| Aspetto | Classic (Congelato) | Emerald Control |
|---|---|---|
| **Palette Dark** | Slate-teal `#091011` / `#111d1f` | Deep forest-emerald `#071110` / `#0C1917` / `#11211E` |
| **Palette Light** | `#f1f5f4` / `#ffffff` | Neutro caldo `#F4F7F5` / `#FAFCFB` con testo `#0D1F1B` (AA) |
| **Accent & Focus** | `#2dd4bf` | `#34D6A0` (hover `#57E4B7`, focus ring nitido) |
| **Energy Monitor** | Schema rettangolare a blocchi | Hub radiale/semicircolare SVG con archi curvi, particelle proporzionali e gauge SOC circolare/barra |
| **Card & Bordi** | Raggio `14–16px`, ombra morbida | Bordi `1px` Linear-style `#28423D`, gerarchia netta, zero shadow pesanti |
| **KPI & Decision Card** | Griglia esistente congelata | Maggiore respiro verticale (o compatta su scelta utente), gauge semicircolari netti, ribbon Decisione EMS con envelope |
| **Densità** | Fissa | Selezionabile (`Comoda` / `Compatta`) |

## 2. Elementi Condivisi (Zero Duplicazione)

- **Backend / Data**: REST API (`api.ts`), WebSocket live stream (`useQuery` + `qc.setQueryData`), formule KPI, calcoli `coverage`, `format.ts`, `auth`, router React.
- **Pagine**: tutte le 8 route (`/`, `/storico`, `/decisioni`, `/analisi`, `/confronto-sem`, `/batteria`, `/report`, `/impostazioni`) restano un unico albero React guidato dai design token di `ThemeProvider`.
- **Grafici**: tutte le istanze usano `EChart.tsx` + `useChartPalette()` sincronizzato al tema attivo senza ricreare l'istanza.

## 3. Gerarchia, Spaziature & Densità

1. Priorità visiva assoluta a **Energy Monitor + Decisione EMS** (`~62% / 38%` su desktop).
2. Subito sotto: **KPI Primari** (`Autoconsumo`, `Autosufficienza`, `Batteria`) e **KPI Giornalieri Compatti** (`FV`, `Casa`, `Rete`, `Batteria`) con sparkline reali orarie.
3. Strip diagnostica di **Copertura telemetrica** (distinguendo `Periodo precedente non monitorato` da `GAP` reali).
4. **Trend live 6h**, **Ultimi 3 eventi significativi raggruppati** e **Stato sistema + Diagnostica risorse**.

## 4. Strategia Responsive & Accessibilità

- Breakpoint coerenti a `1180px` (stack hero verticalmente, KPI 2x2) e `768px` (bottom nav 5 item + drawer mobile, tabelle convertite in card, Energy Monitor verticale touch-friendly con nodi >= 44px, DetailDrawer full-screen).
- Skip link (`Salta al contenuto principale`), aria-expanded su tutti i popover e tooltip, testo minimo >= 12px in Emerald Control, `reduced_motion` sia da OS sia da impostazione utente.

## 5. Rischi Mitigati & Alternative Scartate

- **Scartata duplicazione pagine** (`DashboardClassic.tsx` / `DashboardEmerald.tsx`): avrebbe raddoppiato manutenzione KPI e introdotto rischio di regressione su SHADOW. Scelto invece un singolo albero controllato da `data-style`, `data-theme`, `data-density`, `data-motion` + lazy-load della sola vista SVG `EmeraldEnergyMonitor`.
- **Rischio FOUC / Flash bianco**: mitigato con sincronizzazione immediata prima del mount React (`themeStorage.ts`) basata sull'ultima preferenza in `localStorage`.
- **Rischio crash variante Emerald**: mitigato con `ThemeErrorBoundary` dedicata che non sovrascrive la preferenza persistita senza consenso e consente ripristino immediato a `Classic`.
- **Rischio reset ECharts su cambio tema**: mitigato aggiornando `setOption(..., { notMerge: false })` anziché smontare l'istanza in `useEffect`.
