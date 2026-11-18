# Future LIVE qualification

Mission 5 exposes an operating-mode selector. SHADOW is the only accepted operating mode and LIVE returns LIVE_NOT_QUALIFIED. There is no LIVE executor in the build. `CommandExecutor::resolve` consumes a completed Safety decision; the only implementation is `ShadowCommandExecutor`. Its decision contract fixes executed=false. A future applied-command result must have a separately qualified type and composition, with no reuse of AUX or direct caller-to-bus access.

Before considering a separate LIVE build:

- Confirm authorized inverter model, firmware, vendor write semantics, address/units, applicable limits and post-write readback. Reconcile SEM and stop concurrent controllers before field testing.
- Review External EMS heartbeat requirement, confirm renewal below 2 minutes and verify inverter-native fallback when writer, network, clock or power fails. Validate fail-closed behavior through stale data and brownout.
- Qualify explicit arming, isolation from manual AUX, one owner, bounded command executor, watchdog, and rollback. Produce tests and independent bus captures under faults and every applicable limit.
- Obtain explicit authorization for qualified field control. Keep the read-only SHADOW build available as rollback until separately accepted.
