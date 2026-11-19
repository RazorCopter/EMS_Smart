# Home Assistant e Afore EMS SHADOW

L'istanza Home Assistant usa il package `afore_esp_inputs.yaml` in `/homeassistant/packages/`. Per reinstallarlo, copia il file, valida YAML e ricarica script, automazioni, entità template ed entità MQTT configurate in YAML.

La dashboard Energia usa la risorsa `/local/energy/energy-obsidian-card-m6b.js?v=20261119-m6b`. La patch `energy-obsidian-card.patch` si applica alla risorsa Obsidian originale con versione interna `20261024-5`. In Energia → Impostazioni Obsidian → **Afore EMS** trovi la diagnostica e i controlli SHADOW. **Inverter live** gestisce le entità Solarman reali. Durante il collaudo inverter in SelfUse nativo, automazione SEM OFF ed ESP in SHADOW.

Il broker usa il prefisso Discovery `homeassistant` e la base `afore/afore-ems/v1`. I cinque input HA viaggiano su `input/{heartbeat,forecast_solar,forecast_ratio,weather,load_profile}` con schema 2. Il firmware espone lo stato di ricezione su `system/ha_inputs`. Gli input sono solo informativi. Il rapporto reale/previsto può essere nullo quando la previsione FV è troppo bassa per un confronto. Solcast scade a 14 ore dall'ultimo polling effettivo; l'heartbeat viene emesso ogni 30 secondi e scade dopo 90 secondi.

I comandi HA pubblicano su `ems/command`. Le risposte arrivano su `ems/ack` con revisione, strategia richiesta, strategia configurata e strategia effettiva al momento dell'ACK. La decisione successiva conferma la strategia realmente eseguita in simulazione. Il toggle AUX1 invia auto-spegnimento a 30 secondi, e lo stato MQTT riporta la posizione comandata. Il firmware non dispone di feedback fisico dei contatti.

- [Inventario Discovery](ENTITA_DISCOVERY.md)
- [Report di collaudo](../MISSION_6.md)
