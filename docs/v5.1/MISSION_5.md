# Mission 5 · Console, manual AUX and HA inputs

Firmware v5.1-console-3, signed OTA SHA-256 d1e9c09a1f06e0f70c6f00c0367f95dc04e2e23cf5564220f9ad508f68ba260d. Console at http://10.0.0.110/. The configured production MQTT host, credentials, device ID, prefix and base topic are retained. The previous SHADOW image remains available for OTA rollback. Modbus remains FC04 only.

The HA/Solarman defaults are 10,240 Wh, 51.2 V nominal, 50 A charge/discharge, 20–100% SOC, 100% target and 3,300 W export limit. The independent 1,000 W caps remain active. Daily TARGET_SOC expires at 15:30 Europe/Rome and re-arms the next local day. Relative duration is still selectable and serves as the fallback without a valid wall clock.

The console replaces the flow list with a self-contained SVG diagram, poll interval two seconds, display deadband 20 W, arrows and proportional width for non-idle measurements. Load is derived. Strategies and battery forms use one help dictionary, keyboard/touch help, numeric bounds and paired sliders. Before posting, the UI displays the diff of the modified fields.

## Verification

- An authenticated browser checked desktop/mobile responsiveness, tooltip, slider, save diff, overview SVG and System. Three browser AUX ON/OFF cycles showed GPIO47 high then low. Signed OTA booted valid. The host core assertions cover configured defaults and the daily cutoff/next-day transition.
- REST and UI rejected LIVE; MQTT denied it with LIVE_NOT_QUALIFIED. Three ON/OFF pairs were sent to the actual broker using the same envelope as the HA switch. An additional ON used an 8 s auto-off, confirmed via MQTT state. A software reboot requested while ON returned AUX to OFF with GPIO47 LOW and resumed FC04/MQTT. GPIO evidence is not contact-feedback evidence.
- On the production broker, 41 unique Discovery payloads shared the existing device identifier. New: operating mode select, AUX1 switch, nominal voltage and battery capacity sensors, LIVE lock reason.
- Five synthetic HA inputs were received; solar included 48 half-hour slots for tomorrow. All were verified valid, then expired and had null payloads; heartbeat expiration showed HA offline while SELF_USE continued. HA forecasts are never used by the core in this release. [Valid input evidence](evidence/M5_FORECAST_VALID.json) · [Expired evidence](evidence/M5_FORECAST_STALE.json).
- YAML parsed, Jinja templates parsed, the Obsidian diff applies to the supplied card without modifying the Inverter live controls, and the patched JavaScript passed syntax validation. Nothing was installed on Home Assistant.

## Deliverables

[HA package and instructions](ha/README.md) · [AUX abstraction](05_AUX_ABSTRACTION.md) · [LIVE checklist](LIVE_UNLOCK_CHECKLIST.md) · [Broker ACKs](evidence/M5_ACKS.json)

[Desktop overview](evidence/M5_desktop_overview.png) · [Desktop tooltip](evidence/M5_desktop_tooltip.png) · [Desktop system](evidence/M5_desktop_system.png)

[Mobile overview](evidence/M5_mobile_overview.png) · [Mobile tooltip](evidence/M5_mobile_tooltip.png) · [Mobile system](evidence/M5_mobile_system.png)
