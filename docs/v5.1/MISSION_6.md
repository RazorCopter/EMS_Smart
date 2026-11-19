# Missione 6 — Collaudo Home Assistant

ESP32 `10.0.0.110`, firmware in prova `v5.1-console-5c`, modalità SHADOW. Inverter in SelfUse nativo. Il confronto Obsidian mostra il calcolo SEM separato dalla decisione ESP; l'automazione SEM è risultata OFF.

| Prova | Esito ed evidenza |
|---|---|
| Installazione HA e Discovery | Package caricato e ricaricato. Rilevate 62 configurazioni Discovery. I cinque input HA sono arrivati con schema 2. [Inventario](ha/ENTITA_DISCOVERY.md) |
| Strategia dalla UI HA | Da SELF_USE a NATIVE_INVERTER, ACK accettato con revisione 2, decisione ESP NATIVE_INVERTER con `executed=false`, schermata HA aggiornata. Ripristinato SELF_USE dalla stessa UI. L'ACK riporta lo stato effettivo osservato prima del prossimo ciclo EMS. |
| Altri comandi EMS | SOC target, limiti di carica e scarica, Target SOC e Maintenance con annullamento, override con scadenza e annullamento superati dalle azioni della UI HA. Valori di produzione ripristinati. Stato e revisione verificati sull’ESP e nei topic di stato; le entità Discovery usano feedback MQTT. [ACK](evidence/M6/6b-ui-controls.json) · [Decisioni](evidence/M6/6b-ui-decisions.json) |
| LIVE | Rifiutato da HA con `LIVE_NOT_QUALIFIED`. [ACK MQTT](evidence/M6/mqtt-check.json) |
| AUX | ON da UI HA e OFF per TTL con origine TIMER. OFF esplicito da HA con ACK accettato. [ACK](evidence/M6/6b-ui-controls.json) Al reboot ESP da AUX ON con TTL, GPIO47 è tornato LOW e AUX OFF. [TTL](evidence/M6/6b-aux-ui.json) · [Reboot](evidence/M6/6b-reboot-replay.json) |
| Riavvio HA | Comando di restart Core dalla UI. L'ESP ha rilevato il nuovo heartbeat di avvio alle 07:42:35 CET e poi la cadenza normale. Il campionamento HTTP è iniziato dopo il comando e non ha osservato la finestra di indisponibilità. [Campioni](evidence/M6/6b-ha-restart.json) |
| Riavvio broker EMQX | Perdita e ripristino rilevati da TCP e/o stato MQTT. Le letture sono salite da 52.316 a 52.363 e il ciclo EMS da 23.295 a 23.316. Uptime crescente, SELF_USE, zero scritture. [Campioni](evidence/M6/6b-broker-restart.json) |
| Reboot e anti-replay | Configurazione persistente verificata per confronto. Override retained rifiutato dopo il reboot; vecchio comando rifiutato per scadenza. Strategia tornata SELF_USE. [ACK e stato](evidence/M6/6b-reboot-replay.json) |
| Forecast e heartbeat | Rifiutati payload previsionali malformati, fuori schema temporale e scaduti. Un payload Solcast reale riprodotto con TTL di 10 s è passato da valido a obsoleto con dato nullo. [Scadenza](evidence/M6/6b-forecast-expiry.json) Provato HA Online → Offline dopo 90 s → Online con SELF_USE sempre operativo. [Forecast](evidence/M6/mqtt-check.json) · [Heartbeat](evidence/M6/ha-heartbeat-fault.json) |
| Dashboard HA | Energia e Afore EMS controllate in Edge desktop. Vista mobile controllata in Edge a 390 px. Obsidian 20261119-m6b legge lo stato strategia dall’entità Discovery; valore RUN confermato nella UI. |

## Osservazione continua

La finestra utile parte dopo la ripresa di MQTT, heartbeat e Solcast dei fault test. [CSV](evidence/M6/observation.csv) · [Riepilogo](evidence/M6/observation-summary.json)

- Durata 7200.6 s, 671 campioni, 19/11/2026 08:23:51 CET â€“ 19/11/2026 10:23:51 CET.
- Uptime 2287673 â†’ 9488128 ms, zero reset osservati.
- Heap iniziale / minimo campionato / finale 151684 / 145572 / 151688 B; minimo storico del boot 130324 B.
- RSSI -67â€¦-54 dBm, 0 riconnessioni MQTT.
- FC04 5009 â†’ 20893 (+15884), 0 errori nella finestra.
- Input HA +352 accettati e +0 rifiutati, 28 aggiornamenti forecast. 7070 nuovi cicli EMS.
- Revisione 25, AUX OFF, GPIO47 LOW, zero scritture e decisioni esclusivamente SHADOW. ACK di collaudo in [6b-ui-controls.json](evidence/M6/6b-ui-controls.json).
