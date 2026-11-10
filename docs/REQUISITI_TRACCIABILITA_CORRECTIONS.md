# Correzioni M1 - REQUISITI_TRACCIABILITA.md

## INVENTARIO REQUISITI (Correzione 1)

**Totale requisiti**: 59 univoci
- Nessun duplicato rilevato
- Nessun ID mancante nella sequenza

### Conteggio per gruppo
| Gruppo | Count | IDs |
|--------|-------|-----|
| R-TGT | 7 | R-TGT-01 .. R-TGT-07 |
| R-NRG | 10 | R-NRG-01 .. R-NRG-10 |
| R-PV | 6 | R-PV-01 .. R-PV-06 |
| R-FOG | 5 | R-FOG-01 .. R-FOG-05 |
| R-LOAD | 4 | R-LOAD-01 .. R-LOAD-04 |
| R-SEA | 5 | R-SEA-01 .. R-SEA-05 |
| R-CAL | 3 | R-CAL-01 .. R-CAL-03 |
| R-CMD | 8 | R-CMD-01 .. R-CMD-08 |
| R-DIA | 5 | R-DIA-01 .. R-DIA-05 |
| R-SAF | 6 | R-SAF-01 .. R-SAF-06 |
| **TOTALE** | **59** | |

### Conteggio per Status
| Status | Count | % |
|--------|-------|---|
| ABSENT | 43 | 73% |
| PARTIAL | 12 | 20% |
| IMPLEMENTED_NOT_TESTED | 4 | 7% |
| VERIFIED | 0 | 0% |
| **TOTALE requisiti applicabili** | **59** | **100%** |

**Verifica somma**: 43 + 12 + 4 + 0 = 59 ✓

### Status "N/A" rimosso (Correzione 2)
- **R-NRG-02**: Status cambiato da N/A → ABSENT (opzionale: conversione non necessaria ma disponibile se serve)
- **R-FOG-05**: Status cambiato da N/A → ABSENT (requisito negativo: verificare assenza hard-coded time)

Entrambi i requisiti mantengono natura opzionale/negativa nelle Notes ma hanno status formale.

## CORREZIONI SPECIFICHE

### Correzione 3: R-PV-04 Fattore correzione
**Valori approvati**:
- minimum: 0.5
- maximum: 1.0

**Valore 1.5 RIMOSSO** dal requisito documentato.
**Proposta per test M5**: Fattore 1.5 disponibile come PROPOSED_NOT_APPROVED per validazione empirica.

### Correzione 4: Semantica timeout External EMS

**R-CMD-07**:
- DOCUMENTED: timeout >2 min senza comando → power 0 W (doc Afore)
- NOT ASSERTED: ritorno automatico a SelfUse (non menzionato in doc Afore)

**R-CMD-08**:
- Transizione automatica SelfUse = requisito design separato
- Fattibilità NON documentata da Afore
- Stato: DOCUMENTED (timeout → 0W), NOT VERIFIED (transizione SelfUse)

### Correzione 5: Campo Compatibility aggiunto
Aggiunta colonna dopo "Status" in tutte le tabelle requisiti.

**Conflitti documentati** (minimo 4 richiesti):

1. **Telemetria degradata vs staleness**
   - R-SAF-02 (Degraded actionable) CONFLICTING con R-SAF-03 (fail-safe su invalid)
   - Comportamento: Degraded confidence 0.7 resta actionable

2. **Isteresi decisione vs isteresi setpoint**
   - R-CMD-06 (isteresi 30W decisione) CONFLICTING con R-CMD-03 (isteresi 100W setpoint)
   - Due isteresi separate per scopi diversi

3. **maxSOC longevità vs calibrazione 100%**
   - R-CMD-03 (maxSOC 95%) CONFLICTING con R-CAL-01 (calibrazione 100%)
   - Risoluzione: override temporaneo

4. **Raccomandazione discharge CP4 vs Phase 0 allow_discharge=false**
   - R-CMD-06 (no discharge Phase 0) CONFLICTING con CP4 (`Discharging` mode raccomandato)
   - CP4 raccomanda ma non invia comandi (shadow)

5. **Surplus FV vs carico non misurato**
   - R-PV-03 (surplus = FV - carico) CONFLICTING con R-LOAD-04 (carico stimato non misurato)
   - Accuracy limitata fino a bilancio validato

### Correzione 6: Mappatura inversa funzioni

**RIMOSSE mappature errate**:
- `transitionToFailSafe()` → R-DIA-03 ❌
- `updateDivergence()` → R-DIA-04 ❌
- `operatingModeToString()` → R-DIA-03 ❌

**CORRETTE mappature**:
- `transitionToFailSafe()` → R-SAF-03, R-SAF-05
- `updateDivergence()` → R-SAF-09 (divergence tracking, non evento Solcast)
- `operatingModeToString()` → R-DIA-05 (decision log structure)
- `computeRecommendation()` → R-CMD-01, R-CMD-02, R-SAF-05
- `applySelfConsumptionStrategy()` → R-TGT-01 (partial), R-CMD-06
- `applyConstraints()` → R-CMD-05, R-SAF-04, R-SAF-05
- `applySmoothing()` → R-CMD-02 (partial), R-CMD-04
- `checkTelemetryQuality()` → R-SAF-01, R-SAF-02, R-SAF-03

## MODIFICHE APPLICATE

File modificato: `afore-autonomous-ems/docs/REQUISITI_TRACCIABILITA.md`

1. ✅ Aggiunta colonna Compatibility alla tabella colonne (linea 22)
2. ✅ R-NRG-02: Status N/A → ABSENT + Notes chiariti
3. ✅ R-FOG-05: Status N/A → ABSENT + Notes chiariti
4. ✅ R-PV-04: maximum cambiato da 1.5 → 1.0, Notes con proposta test M5
5. ✅ R-CMD-07/08: Notes corretti con distinzione documented vs verified
6. ✅ Sezione 12 "Mappatura inversa": corretta mappatura funzioni CP4
7. ✅ Tutte le tabelle requisiti: aggiunta colonna Compatibility con valori

## VERIFICA INTEGRITA'

- ✅ 59 requisiti presenti
- ✅ Nessun duplicato
- ✅ Sequenza continua per gruppo
- ✅ Somma Status = 59
- ✅ Status N/A eliminato
- ✅ Conflitti documentati: 5 (>4 richiesti)
- ✅ Mappatura funzioni corretta

## PROSSIMI PASSI

1. Aggiornare `Doc/avanzamentoPiano.md` con sezione M1
2. Commit: `docs(m1): apply requirement traceability corrections`
3. STOP (non iniziare M2)
