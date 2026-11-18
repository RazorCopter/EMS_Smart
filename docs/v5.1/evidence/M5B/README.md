# Missione 5B

Console ESP32 all'indirizzo [10.0.0.110](http://10.0.0.110/). La revisione prima dell'OTA ha usato una copia della telemetria reale e dati orari controllati per mostrare sia eventi con ora assoluta sia eventi precedenti alla sincronizzazione. Le schermate finali provengono dalla console installata.

| Pagina | Desktop prima | Desktop dopo | Mobile prima | Mobile dopo |
|---|---|---|---|---|
| Overview | [Prima](before/desktop-overview.png) | [Dopo](after/desktop-overview.png) | [Prima](before/mobile-overview.png) | [Dopo](after/mobile-overview.png) |
| Strategie EMS | [Prima](before/desktop-strategies.png) | [Dopo](after/desktop-strategies.png) | [Prima](before/mobile-strategies.png) | [Dopo](after/mobile-strategies.png) |
| Batteria e limiti | [Prima](before/desktop-limits.png) | [Dopo](after/desktop-limits.png) | [Prima](before/mobile-limits.png) | [Dopo](after/mobile-limits.png) |
| MQTT | [Prima](before/desktop-mqtt.png) | [Dopo](after/desktop-mqtt.png) | [Prima](before/mobile-mqtt.png) | [Dopo](after/mobile-mqtt.png) |
| Sistema | [Prima](before/desktop-system.png) | [Dopo](after/desktop-system.png) | [Prima](before/mobile-system.png) | [Dopo](after/mobile-system.png) |

- Pannello di aiuto [desktop](after/desktop-help.png) e [mobile](after/mobile-help.png).
- Impostazioni orarie [desktop](after/desktop-time-settings.png) e [mobile](after/mobile-time-settings.png).
- Controllati i viewport 1366×768, 820×1000 e 390×844. Verificati selezione strategia, formato `15:30`, assenza di scorrimento orizzontale, apertura aiuto da tastiera, chiusura con Escape e assenza di sovrapposizione sui campi.

## Orologio

- `GET /api/v1/time` restituisce configurazione e stato. Il `POST` sullo stesso endpoint salva `enabled`, `zone`, `primary`, `fallback1` e `fallback2`. `POST /api/v1/time/sync` richiede una nuova sincronizzazione. Le tre operazioni richiedono autenticazione.
- Il PCF85063AT conserva UTC. Un RTC valido inizializza l'orologio al boot con sorgente `RTC`; il primo campione NTP valido passa la sorgente a `NTP` e aggiorna l'RTC con lettura di verifica. Con orologio invalido la sorgente è `UNSYNCED` e l'evento conserva `timestamp: null`. Un evento già registrato mantiene la sorgente originale. L'ultima sincronizzazione NTP è conservata in NVS e rimane visibile dopo il riavvio.
- NTP riparte alla connessione Wi-Fi e alla riconnessione. L'intervallo impostato nel client SNTP è di sei ore. Il pulsante richiede una sincronizzazione immediata.
- TARGET_SOC giornaliero usa l'ora locale della zona IANA. In caso di ora ripetuta il termine è alla prima occorrenza; in caso di ora saltata il termine slitta della durata del salto. Watchdog, durata e scadenza degli override, validità runtime degli input HA e tempo di mantenimento usano contatori monotoni.
- I payload JSON MQTT di stato e audit portano `timestamp` UTC ISO 8601, `uptime_ms`, `time_source` e `time_valid`. Il messaggio MQTT di disponibilità resta compatibile con Home Assistant (`online`/`offline`).

## Verifiche sulla scheda

- Primo avvio con RTC fermo e sorgente `UNSYNCED`. Il collegamento MQTT a `uptime_ms=2108` ha conservato `timestamp: null`. La prima sincronizzazione è arrivata alle `21:22:17Z`, a `uptime_ms=6029`, con scrittura RTC verificata.
- Verificati il pulsante Sincronizza ora, l'avvio con solo RTC, il recupero della data dell'ultimo sync e l'uso separato di ciascun fallback. Il broker è rimasto configurato come prima. Stato, decisione SHADOW e metadati sono nei [risultati della scheda](device-check.json) e nei [campioni MQTT](mqtt-timing.json).
- Stato finale, hash e limiti della prova sono riportati in [verifica finale](final-check.json).
