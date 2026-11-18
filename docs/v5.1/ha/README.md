# Afore ESP push from Home Assistant

Install `afore_esp_inputs.yaml` as a new HA package after checking the listed entity IDs. Keep the original SEM package, automation and Inverter live tab. The package only publishes input data and reads MQTT state. It does not provide inverter commands. The ESP never requests HA API data or credentials.

Add the card patch from the directory containing `energy-obsidian-card.js`: `git apply energy-obsidian-card.patch` (or use a patch utility outside a Git checkout). Reload the browser asset using a distinct cache version. The added EMS tab compares the local SEM and ESP SHADOW plans. Inverter live remains the SEM/HA control surface.

The push namespace is `afore/afore-ems/v1/input/` followed by `forecast_solar`, `forecast_ratio`, `weather`, `load_profile` or `heartbeat`. The ESP also accepts `afore/afore-ems/v1/forecast/solar` as an ingress alias. Publish only once on the canonical topic. Unix seconds are used for timestamps. Each item expires according to its own lifetime. Solcast expiration is evaluated against the last upstream poll, even if HA republishes every five minutes. 30-minute period starts are UTC Unix seconds, and estimates are kW.

New Discovery items: operating mode select, AUX1 switch, nominal battery voltage, useful battery capacity and LIVE lock reason. AUX ON/OFF commands obey the signed-firmware interlock and report commanded state with physical_feedback=null. The new comparison sensor classifies strategy agreement; SEM current is a limit in A and ESP simulated W is a setpoint, so their numerical equality is not used to infer agreement.
