# AUX1 manual output · v5.1 Mission 5

AUX1 is a manually commanded output, independent of the EMS calculation and RS485 polling. The verified board mapping is GPIO47 active high. The application initializes the GPIO latch LOW before enabling the output, both during variant initialization and at setup. There is no saved ON state or replay queue. Manual ON is refused before the signed OTA image is valid and the runtime watchdog and reader interlock are ready.

Authenticated REST and Web UI handlers and validated, fresh, non-retained MQTT commands can request ON or OFF. The UI requests confirmation. The optional `auto_off_s` uses a monotonic deadline, accepts 0–86400 and zero means manual hold until an OFF request or safety intervention. The command envelope TTL applies to MQTT message acceptance. A latched bus fault or unavailable watchdog forces GPIO47 LOW. AUX cannot issue Modbus requests or change settings.

`aux1` reports the commanded state; `physical_feedback` is always null. GPIO47 sampled via the system endpoint is electrical pin evidence after initialization. It does not verify relay contacts. Audit lines use the origin (WEB_UI, REST_API, MQTT, TIMER, SAFETY_ENGINE), action, outcome and commanded state. No inverter command is executed when AUX changes state.

Power-on and software restart return the relay to OFF. Scope measurement across boot ROM, watchdog, power ramps and brownouts remains necessary to assess brief pulses. Never infer a contact reading from the GPIO level.
