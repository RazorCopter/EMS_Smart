# Visual Review, Template Split & Correspondence Checklist (`Mission 8 — Correzione Template Distinto`)

- **Baseline monorepo commit**: `513a353` (`offline-full-product-completion`)
- **Baseline standalone repo (`RazorCopter/dashboard_ems` `main`)**: `4a9e9e0c1ed48e7f85afe727ebcd3cabe79a3688`
- **Default primo deploy**: `classic` + `dark` (`Classic + Scuro` invariato)

---

## 1. Architettura a Template Distinti (Zero Duplicazione Dati)

- **Shared ViewModel & State**:
  - `src/viewmodels/dashboardViewModel.ts` produce `DashboardViewModels` (`energyFlow`, `decision`, `kpis`, `coverage`, `system`, `liveChart`, `eventsTop3`, `telemetry`) da `/api/v1/live` e `/api/v1/history`.
  - Tutte le formule KPI, formatter (`src/lib/format.ts`), TanStack Query, autenticazione, export, `ThemeProvider` e WebSocket `ws/live` sono condivisi tra entrambi i template.
- **Classic (`data-ui-template="classic"`) — Baseline Congelata**:
  - `ClassicAppShell` (`src/components/classic/ClassicAppShell.tsx`)
  - `ClassicDashboardLayout` (`src/components/classic/ClassicDashboardLayout.tsx`)
  - `ClassicEnergyMonitor` (`src/components/classic/ClassicEnergyMonitor.tsx`)
- **Emerald Control (`data-ui-template="emerald"`) — Template Strutturalmente Distinto**:
  - `EmeraldAppShell` (`src/components/emerald/EmeraldAppShell.tsx`): sidebar `240px` raggruppata (`Monitoraggio`, `Analisi`, `Sistema`), header mobile dedicato con accesso a `Stato impianto`.
  - `EmeraldDashboardLayout` (`src/components/emerald/EmeraldDashboardLayout.tsx`): header contestuale alto con data/ora live, stato telemetria, strategia e badge SHADOW.
  - `EmeraldEnergyMonitor` (`src/components/emerald/EmeraldEnergyMonitor.tsx`): anello radiale segmentato con 4 nodi circolari iconici (nessuna cornice rettangolare, zero testo tecnico sul canvas), anello SOC concentrico sulla batteria, layout mobile semplificato (3 nodi circolari in alto + Batteria in basso), dettaglio elettrico in drawer (full-screen su mobile).
  - `EmeraldDecisionCard` (`src/components/emerald/EmeraldDecisionCard.tsx`): gerarchia fedele al mockup (`Strategia attiva`, `Setpoint simulato`, confronto `Richiesta iniziale` / `Safety`, `Motivazione` umana, strip `SHADOW`).
  - `EmeraldKpiSection` (`src/components/emerald/EmeraldKpiSection.tsx`): Row 2 con 3 pannelli (`Autoconsumo`, `Autosufficienza` con barre spesse; `Accumulo` con **batteria grafica a 10 segmenti**) e Row 3 con 4 card con **sparkline SVG continue** (`<polyline>` + `<polygon>`).
  - `EmeraldSystemPanel` (`src/components/emerald/EmeraldSystemPanel.tsx`) & `EmeraldChrome` (`src/components/emerald/EmeraldChrome.tsx`).

---

## 2. Checklist Corrispondenza Mockup & Requisiti Correzione Missione 8

| Requisito | Verifica Implementata | Esito |
|---|---|---|
| Classic invariato + default al primo deploy (`classic` + `dark`) | `classic_local_1440.png` vs `classic_baseline/` + test default | Conforme |
| Distinzione strutturale anche in scala di grigi | `emerald_local_grayscale_1440.png` vs `classic_local_1440.png` | Conforme |
| `data-ui-template="classic"` / `"emerald"` | Verificato su Shell e Dashboard + test Vitest dedicati | Conforme |
| AppShell Emerald: Sidebar ~240px, gruppi, connessioni, SHADOW | `EmeraldAppShell` (`240px`, 3 gruppi, ESP32/MQTT/HA + SHADOW) | Conforme |
| Header contestuale Emerald grande con ora/telemetria/SHADOW | `EmeraldPageHeader` su Dashboard e tutte le 7 sotto-pagine | Conforme |
| Row 1 `65% / 35%`: Monitor radiale a nodi circolari + Decision Card gerarchica | `em-row-hero` (`1.85fr / 1fr`), no box nodi, no `.kv-row` su Decision | Conforme |
| Row 2: 3 pannelli ad alto impatto (`Autoconsumo`, `Autosufficienza`, `Accumulo` segmentato) | `em-kpi-row-hero` + `SegmentedBatteryGraphic` 10 celle | Conforme |
| Row 3: 4 card energetiche con sparkline SVG continue (`polyline`+`polygon`) | `ContinuousSparkline` senza barre rettangolari | Conforme |
| Row 4 `75% / 25%`: Grafico `280px` a sinistra + `Stato impianto` autonomo a destra | `em-row-bottom` (`3fr / 1fr`) + `EmeraldSystemPanel` (ESP32, MQTT, HA, Controllo reale, SEM, Modalità ESP, AUX, NTP + risorse) | Conforme |
| Mobile Emerald (`390x844`): monitor 3 nodi su + Batteria giù, `Impianto` in drawer, detail full-screen, KPI 2/riga | `emerald_local_390_mobile.png`, `emerald_local_390_node_detail.png` | Conforme (`overflow=False`) |
| Font minimo leggibile (`>= 12px`) & `0` errori console/JS | Verificato programmaticamente via Playwright (`smallestFontPx=12`, `errors=0`) | Conforme |
| Dark + Light + Sistema indipendenti dallo stile senza ricarica o reset WS | `emerald_local_light_1440.png`, ECharts in-place `setOption`, test live | Conforme |
