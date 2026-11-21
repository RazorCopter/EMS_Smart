# LIVE qualification

Mission 5 exposes an operating-mode selector. SHADOW is the only accepted firmware operating mode and LIVE returns LIVE_NOT_QUALIFIED. The host-tested `LiveExecutor` consumes a completed Safety decision and uses a separate result for matched setpoint readback. It starts in SHADOW, requires a volatile lease and exclusive control, and releases control on invalid or expired decisions. It renews unchanged setpoints every 30 seconds while armed. The driver's independent watchdog and release behavior must be qualified before connecting a physical actuator. The firmware still links only the SHADOW path.

The separate `waveshare-1ch-v51-live-trial` image boots in SHADOW and supports
both Trainer and continuous LIVE. The standard image remains the SHADOW rollback. Confirm
exclusive ownership of the RS485 command path and keep the operator at the inverter.

1. Start the capture tool before selecting LIVE. The firmware reads and
   remembers the current native mode, selects External EMS and verifies battery
   power control before sending setpoints.
2. Trainer limits requests to 200 W in either direction within Safety bounds.
   It reads back each command and compares it with measured battery power.
   The initial response envelope is 60 W after 3 seconds of settling, with
   a 10-second deadline for an unresolved response.
3. An unresolved mismatch stops the session and attempts verified restoration
   of the remembered native mode. Trainer completion after five minutes also
   restores native mode. The console reports a failed restoration separately.
4. Continuous LIVE omits the Trainer deadline, cap and power comparison. It
   retains the baseline Safety bounds, command readback and native-mode recovery.

Before considering a separate LIVE build:

- The setpoint frame codec is verified against both signed examples in the manufacturer's document. Protocol framing alone does not establish charge direction or reliable setpoint readback.
- The console stores command polarity independently of the SHADOW policy and shows a preview of device watts. Off preserves the EMS sign and on negates it. A future qualified session must take a fresh copy of this setting before arming and hold it fixed until disarmed. Telemetry polarity always comes from the read profile.
- The host-tested actuator requires exclusive bus ownership and the verified battery-power control type. Connect it to an exclusive physical bus implementation only after qualifying FC03 setpoint readback and restoration of the previously observed native mode.
- Confirm authorized inverter model, firmware, vendor write semantics, address/units, applicable limits and post-write readback. Reconcile SEM and stop concurrent controllers before field testing.
- Verify the documented two-minute timeout and 0 W setpoint on the installed firmware. Qualify the return to native control separately for loss of writer, network, clock or power. Validate fail-closed behavior through stale data and brownout.
- Qualify explicit arming, isolation from manual AUX, one owner, bounded command executor, watchdog, and rollback. Produce tests and independent bus captures under faults and every applicable limit.
- Obtain explicit authorization for qualified field control. Keep the read-only SHADOW build available as rollback until separately accepted.
