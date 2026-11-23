# Afore Emerald Control
**Dark-first Energy Analytics Design System**

---

## 1. Principi Fondanti

> *"The product is dark-first, not dark-only. Design and validate every component in dark mode first. Light mode is a fully supported secondary theme. Reserve saturated colors for energy data, state and deliberate action."*

1. **Verità del dato prima dell'estetica**: nessun valore mancante (`null`) viene trasformato in `0`; ogni grandezza distingue visivamente stato `valid` (misurato vs derivato), `stale`, `unavailable` e `NOT_MONITORED`.
2. **Chiarezza operativa SHADOW**: l'interfaccia comunica ovunque in modo inequivocabile che il Companion è in sola lettura (`executed: false`, `modbus_write_count: 0`, nessuna capacità LIVE).
3. **Gerarchia silenziosa, dati protagonisti**: la struttura (canvas, superfici, bordi hairline) usa neutri profondi con sottotono emerald; i colori saturi sono riservati ai flussi energetici (FV, Casa, Batteria, Rete) e agli stati critici (`SHADOW`, `Safety`, `Fault`).
4. **Precisione numerica**: tutti i valori numerici usano `font-variant-numeric: tabular-nums`, unità secondarie attenuate, massimo due decimali nella UI principale e scala adattiva (`W` / `kW` / `kWh`).
5. **Dual-theme sullo stesso motore**: `Classic` e `Emerald Control` condividono router, store, query, WebSocket, contratti, formule KPI e logica di sicurezza senza duplicare l'applicazione React.

### Influenze architetturali
- **Supabase**: profondità dark-emerald multi-superficie, leggibilità ad alta densità, assenza di decorazioni superflue.
- **Linear**: precisione tipografica, bordi `1px` netti, sidebar compatta con indicatori sobri, transizioni rapide e non invasive.
- **IBM Carbon**: semantica rigorosa di tabelle, grafici, legende interattive, stati `empty`/`stale`/`unavailable`/`error` e accessibilità WCAG 2.1 AA.
- **OpenEMS** *(concettuale)*: leggibilità immediata del bilanciamento energetico entro 5 secondi tramite monitor radiale a 5 nodi e KPI primari (Autoconsumo, Autosufficienza, Accumulo).

---

## 2. Palette Colori

### 2.1 Emerald Dark (tema primario)
| Ruolo | Token CSS | Valore | Note d'uso |
|---|---|---|---|
| Canvas principale | `--bg-canvas` | `#071110` | Fondo viewport (mai `#000000`) |
| Sidebar | `--bg-sidebar` | `#081412` | Navigazione laterale + stato connessione |
| Surface 1 (Card) | `--bg-surface` | `#0C1917` | Card principali e pannelli |
| Surface 2 | `--bg-surface-2` | `#11211E` | Pannelli secondari, righe KPI compatte |
| Surface elevata | `--bg-elevated` | `#162925` | Popover, tooltip, hub inverter, drawer |
| Input / Tabelle | `--bg-inset` | `#0A1614` | Celle tabellari, input form, track barre |
| Border sottile | `--border-subtle` | `#1A312C` | Divisori interni e hairline |
| Border marcato | `--border-strong` | `#28423D` | Bordi card, hover, focus container |
| Primary Emerald | `--accent-teal` | `#34D6A0` | Azione primaria, focus ring, item attivo |
| Primary Hover | `--accent-teal-hover` | `#57E4B7` | Stato hover CTA primaria |
| Primary Muted | `--accent-teal-soft` | `rgba(52, 214, 160, 0.14)` (`#163F34` opaco) | Sfondo pill selezionate |
| Fotovoltaico | `--pv-color` | `#F7C948` | Flusso, icona, serie FV |
| Casa | `--home-color` | `#A78BFA` | Carico domestico (sempre tratteggiato se derivato) |
| Batteria | `--battery-color` | `#34D399` | SOC, carica/scarica accumulo |
| Rete | `--grid-color` | `#4EA8FF` | Importazione / esportazione rete |
| Informazione / SHADOW | `--shadow-blue` | `#38BDF8` | Badge `SHADOW`, setpoint simulato ESP |
| Safety / Warning | `--safety-amber` | `#F59E0B` | Vincoli Safety, telemetria stale |
| Fault / Errore | `--fault-red` | `#F05D6C` | Fault hardware, bus error, offline critico |
| Text Primary | `--text-primary` | `#F3F8F6` | Valori KPI, titoli (mai `#FFFFFF`) |
| Text Secondary | `--text-secondary` | `#A9BDB7` | Label card, sottotitoli |
| Text Muted | `--text-muted` | `#71857F` | Unità, metadati temporali |
| Unavailable | `--missing-grey` | `#53645F` | Dati mancanti o nodi inattivi |

> **Regole cromatiche**: il verde non indica automaticamente "valore positivo" (es. scarica/carica seguono direzione esplicita), e i valori negativi energetici (es. `-0,21 kW` export rete) non sono mai colorati come errori.

### 2.2 Emerald Light (progettato per contrasto AA, non invertito)
| Ruolo | Token CSS | Valore | Contrasto su Surface (`#FAFCFB`) |
|---|---|---|---|
| Canvas principale | `--bg-canvas` | `#F4F7F5` | Fondo neutro caldo (mai bianco puro) |
| Sidebar | `--bg-sidebar` | `#EDF2EF` | Separazione netta 1px |
| Surface 1 (Card) | `--bg-surface` | `#FAFCFB` | Card nette con bordo `#CBDAD3` |
| Surface elevata | `--bg-elevated` | `#F1F6F3` | Popover / hover |
| Input / Tabelle | `--bg-inset` | `#E6EFEA` | Track barre / header tabelle |
| Border marcato | `--border-strong` | `#9EB8AD` | ≥ 3:1 su card UI |
| Primary Emerald | `--accent-teal` | `#047857` | > 5.4:1 su chiaro |
| Fotovoltaico | `--pv-color` | `#B45309` | Ambra scuro leggibile AA |
| Casa | `--home-color` | `#6D28D9` | Viola leggibile AA |
| Batteria | `--battery-color` | `#047857` | Verde smeraldo AA |
| Rete | `--grid-color` | `#1D4ED8` | Blu rete AA |
| Text Primary | `--text-primary` | `#0D1F1B` | ~15:1 |
| Text Secondary | `--text-secondary` | `#324D47` | ~8.5:1 |
| Text Muted | `--text-muted` | `#4D6862` | ~5.7:1 (WCAG AA garantito) |

---

## 3. Tipografia & Gerarchia Numerica

- **Famiglia**: `"Inter", "IBM Plex Sans", -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif` (zero dipendenze CDN esterne per garantire caricamento LAN < 2s offline).
- **Numeri**: `font-variant-numeric: tabular-nums`, unità di misura rese al ~65% di peso visivo rispetto alla cifra, massimo `2` decimali nella UI principale (`kW`, `kWh`), `1` decimale su `%`.
- **Scala tipografica**:
  - `Page title`: `28 px` desktop / `22 px` mobile (`font-weight: 700`, letter-spacing `-0.02em`)
  - `Section title`: `18 px` (`font-weight: 650`)
  - `Card title`: `14–15 px` (`font-weight: 650`)
  - `Primary KPI`: `32–36 px` (`font-weight: 750`)
  - `Body`: `14 px`
  - `Secondary / Table meta`: `12–13 px`
  - **Minimo assoluto Emerald**: `12 px` (nessuna etichetta leggibile sotto `12 px`).

---

## 4. Spacing, Density & Layout Grid

- **Spaziatura base**: scala a `4 px` (`4, 8, 12, 16, 20, 26 px`).
- **Densità configurabile dall'utente**:
  - `Comoda` (`data-density="comfortable"`): card padding `18 px`, grid gap `16 px`.
  - `Compatta` (`data-density="compact"`): card padding `12 px`, grid gap `12 px`, righe tabella `6 px 10 px`.
- **Dashboard Live Grid (Desktop >= 1181px)**:
  1. Header compatto (titolo + timestamp telemetria + badge copertura + badge `SHADOW`)
  2. Riga Hero (`minmax(0, 1.62fr) minmax(340px, 1fr)` ≈ 62% Energy Monitor radiale / 38% Decisione EMS)
  3. KPI Primari (3 colonne: `Autoconsumo`, `Autosufficienza`, `Batteria`)
  4. KPI Energetici compatti (4 colonne: `Produzione FV`, `Consumo casa`, `Import/Export`, `Carica/Scarica`)
  5. Strip Copertura & Qualità (`Copertura dal primo campione`, `Periodo precedente non monitorato`, gap/invalidi)
  6. Trend live `260–300 px` (`1h / 3h / 6h / 12h / Oggi`)
  7. Eventi significativi (max 3 raggruppati) + Stato sistema & Diagnostica risorse.

---

## 5. Componenti Chiave & Comportamenti

### 5.1 Navigazione, Pulsanti, Input, Badge
- **AppShell & Sidebar**: sidebar fissa `236 px` (collassabile a `72 px`) su desktop; header + bottom navigation a 5 voci + drawer su mobile (`<= 768 px`). Include link invisibile a comparsa su focus `Salta al contenuto principale`.
- **Pulsanti**: target touch minimo `38–44 px`; CTA primaria in emerald (`--accent-teal` con testo ad alto contrasto); azioni distruttive con `ConfirmDialog`.
- **Badge**: `SHADOW` (blu informazione), `Misurato` (smeraldo), `Derivato` (viola), `Safety` (ambra), `Fault` (rosso), `Unavailable` (neutro attenuato). Non usare MAI codici grezzi (`SOC_RESERVE`, `CAPTURE_CHARGE_FIRST`) come titolo primario.

### 5.2 Energy Monitor Radiale (`EmeraldEnergyMonitor`)
- **Geometria**: configurazione radiale/semicircolare a croce morbida su SVG scalabile (`viewBox="0 0 700 390"` su desktop, stack radiale semplificato su mobile):
  - **Alto**: Fotovoltaico (`#F7C948`)
  - **Sinistra**: Rete (`#4EA8FF`)
  - **Centro**: Inverter Afore (anello di stato operativo)
  - **Destra**: Casa (`#A78BFA`, bordo tratteggiato = grandezza derivata)
  - **Basso**: Batteria (`#34D399`, SOC prominente + barra riempimento graduata)
- **Flussi**: archi curvi inline con particelle/frecce animate lungo il verso reale di potenza (`dur` proporzionale a `|W|`, disabilitate sotto `deadbandW = 20 W` o se `stale`/`unavailable`/`reduced_motion`).
- **Popover di dettaglio**: cliccando o premendo `Invio`/`Spazio` su un nodo si apre un `dialog` accessibile con provenienza (`measured`/`derived`/`configured`), qualità, timestamp, età in secondi e grandezze elettriche secondarie (`V`, `A`, `Hz`, `°C`, `Safety envelope`).

### 5.3 Tabelle, Popover, Drawer, Dialog
- **ResponsiveTable**: su desktop tabella tabellare a righe sottili `1px`; su mobile (`<= 768 px`) conversione pulita a card verticali per evitare scroll orizzontale.
- **DetailDrawer**: pannello laterale destro (`460 px` desktop, `100vw` mobile) con chiusura via `Esc`, click overlay o pulsante dedicato.
- **HelpTip / FormulaPopover**: attivabile via hover, focus tastiera e click/tap, con allineamento sinistro/destro automatico per evitare clipping.

### 5.4 Grafici (ECharts sincronizzato)
- Istanza ECharts **persistente**: i cambi di tema (`Classic`/`Emerald`, `Scuro`/`Chiaro`) o nuovi snapshot WebSocket aggiornano solo le opzioni necessarie (`setOption` incrementale) senza distruggere il canvas né resettare zoom/serie selezionate.
- Serie principali `2 px`, secondarie `1.5 px`, area fill `8–12%`, SOC tratteggiato su asse Y secondario `0–100%`, `connectNulls: false` (i buchi restano gap visivi espliciti), tooltip verticale ordinato, esportazione PNG coerente con il background del tema attivo.

---

## 6. Stati del Dato & Accessibilità (WCAG 2.1 AA)

| Stato | Rappresentazione | Divieti |
|---|---|---|
| `loading` | Skeleton animato (statico se `reduced_motion`) | Nessun valore placeholder numerico `0` |
| `empty` | `EmptyState` esplicito con causa (es. SEM topic assente) | Nessuna concordanza o metrica inventata |
| `stale` | Badge ambra + banner discreto + stop animazioni flusso | Non mostrare come live fresco |
| `unavailable` | Testo `Non disponibile` / `—` con icona `○` | Mai sostituire `null` con `0` |
| `error` | `ErrorState` / `ThemeErrorBoundary` con fallback a `Classic` | Nessun loop di render |

- **Accessibilità**: contrasto minimo `4.5:1` per testo e `3:1` per elementi grafici; focus ring visibile `2px solid var(--accent-teal)`; riepilogo testuale `.sr-only` dinamico nell'Energy Monitor; rispetto completo di `prefers-reduced-motion` e preferenza utente `reduced_motion`.

---

## 7. Component Inventory

- **Motore temi (`src/theme/`)**: `ThemeProvider`, `useTheme`, `themeRegistry`, `themeTypes`, `themeStorage`
- **Token (`src/themes/`)**: `classic/tokens.css`, `emerald/tokens.css`, `componentVariants.ts`, `layout.ts`
- **Condivisi (`src/components/`)**: `Layout` (AppShell + Sidebar + Mobile Nav + Skip Link), `EChart`, `HelpTip`, `ShadowBanner`, `LoginScreen`, `PowerFlow` (dispatcher + popover accessibile)
- **Specifici (`src/components/shared/`, `components/emerald/`)**: `ThemeErrorBoundary`, `ThemeControls` (`ThemeSelector`, `ColorSchemeSelector`, `DensitySelector`), `EmeraldEnergyMonitor`, `StatusCards` (`EmptyState`, `ErrorState`, `SkeletonCard`)

---

## 8. Esempi & Anti-pattern

**Corretto**:
- Mostrare `Consumo casa: Non disponibile` con icona `Derivato` attenuata se l'inverter non risponde.
- Tradurre `CAPTURE_CHARGE_FIRST` → *"Cattura solare massima (Charge First fino al 100%)"* in card e mostrare il codice tecnico solo nel dettaglio secondario / drawer.

**Anti-pattern vietati**:
- Usare `#000000` o `#FFFFFF` puri in Emerald Control.
- Colorare `-450 W` (esportazione solare in rete) di rosso o applicare glow neon attorno a tutte le card.
- Duplicare pagine intere (`DashboardClassic.tsx` / `DashboardEmerald.tsx`) invece di variare token e slot visuali condivisi.
- Distruggere e ricreare il grafico ECharts ogni 5 secondi su arrivo frame MQTT/WS.
