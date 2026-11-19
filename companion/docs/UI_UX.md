# UI/UX & Design System · Afore EMS Companion

## Identità Visiva e Design Tokens (`apps/web/src/styles/tokens.css`)

- **Temi**: Dark theme predefinito (`#091011` canvas, `#111d1f` surface) + Light theme opzionale (`#f1f5f4`).
- **Colori semantici**:
  - Accento primario: verde acqua `#2dd4bf`
  - Fotovoltaico: giallo/ambra `#f5b942`
  - Casa / carico derivato: viola `#a78bfa`
  - Batteria: verde `#34d399`
  - Rete: azzurro/blu `#60a5fa`
  - Safety: ambra `#f59e0b`
  - Fault: rosso `#f87171`
  - Dato mancante / non disponibile: grigio `#64748b`
  - Modalità SHADOW: blu/verde acqua `#38bdf8` (mai confondibile con stati LIVE)

## Regole di rappresentazione

1. **Power Flow**: linee animate solo sopra deadband (`±20 W`) e rispettando `prefers-reduced-motion`, tratteggiate per valori derivati (Casa), grigie se `null`, ferme vicino allo zero.
2. **Nessun 0 fittizio**: valori assenti mostrati come `Non disponibile` o `—`, e lasciati interrotti (`connectNulls: false`) nei grafici ECharts.
3. **Distinzione SEM vs ESP**: banner permanente nella vista Confronto SEM e nella Dashboard Live.
4. **Accessibilità WCAG 2.1 AA**: navigazione da tastiera con focus ring visibile, attributi `aria-label` e `role="status"`/`role="alert"`, stato mai affidato solo al colore.
5. **Responsive**: verificato su `1920×1080`, `1440×900`, `1366×768`, `768px` tablet e `390×844` mobile.
