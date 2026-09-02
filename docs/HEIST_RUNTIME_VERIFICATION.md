# Heist Runtime Verification

Target: GTA V Enhanced 1.73 / b1158.13. Use a disposable test session and preserve the Tutones log for every failure.

## Stop conditions

- Stop immediately on a crash, infinite pending state, unsupported-layout message, unexpected stat change, stale waypoint, or failed teleport verification.
- Do not add more writable heist globals or locals until every critical Auto Shop and lifecycle check below passes.

## Baseline and lifecycle

- [ ] Load the Release x64 DLL and confirm native and script runtimes reach ready state.
- [ ] In Story Mode, run every Heist Hub action; each must fail cleanly without changing stats.
- [ ] During GTA Online loading, run every action; each must fail or defer safely.
- [ ] Spam each button 20 times; each feature must keep at most one operation pending.
- [ ] Change sessions while waypoint and teleport requests are pending; no stale handler, global, thread, or entity may be used.
- [ ] Unload and reload Tutones after one successful waypoint, then verify waypoint resolution again.

## Auto Shop contracts

With the planning board closed:

- [ ] Record the original active-character `TUNER_CURRENT` and `TUNER_GEN_BS` values.
- [ ] Ready Union Depository and verify contract `0`, prep mask `12543`.
- [ ] Ready Superdollar and verify contract `1`, prep mask `4351`.
- [ ] Test contracts `2` through `7`; each must use prep mask `12543`.
- [ ] Confirm names map to indices `0` through `7` in Rockstar's expected order.
- [ ] Re-enter the Auto Shop and verify the selected contract and completed setups display correctly.
- [ ] Force either stat write/readback to fail and verify both original stats are restored.
- [ ] Switch characters and verify the `MPX_` names resolve only to the active character.

With `tuner_planning` running:

- [ ] Confirm the runtime reports the board as running only when the thread is alive and its stack contains locals 406 and 408.
- [ ] Ready another contract and verify both reload locals receive `2`.
- [ ] Allow several game ticks and verify the board visibly consumes the reload request.
- [ ] Close the board while reload is queued; the request must fail without accessing a stale thread.
- [ ] Close the board and select Reload; verify the explicit `tuner_planning not running` failure.
- [ ] Launch the finale only through Rockstar's planning-board interaction.
- [ ] Complete or abort a finale and verify normal progression is intact.
- [ ] Confirm no finale-launch shortcut or local 3627 write exists in the UI, logs, or runtime.

## Exotic Exports

- [ ] Refresh while inactive; state must be Inactive and coordinates unavailable.
- [ ] Waypoint and teleport must reject inactive, invalid, or non-finite event data.
- [ ] Wait for a legitimate spawn and compare the reported position with the visible vehicle.
- [ ] Set a waypoint and verify it lands on the current vehicle, not a previous spawn.
- [ ] Deliver or despawn the vehicle during collision warmup; teleport must stop with a changed/despawned message.
- [ ] Repeat waypoint verification at three different spawn locations.
- [ ] Teleport on foot and verify safe placement beside the export.
- [ ] Teleport in a locally controlled vehicle and verify the vehicle arrives and remains usable.
- [ ] Attempt teleport without network control of the current vehicle; it must fail without moving it.
- [ ] Test during death, respawn, interior entry, and session transition; unsafe or stale targets must be rejected.
- [ ] Verify collision is requested over separate scheduler ticks before movement.
- [ ] Verify post-teleport coordinates are within 6 horizontal units and 15 vertical units of the destination.

## Exit criteria

- [ ] Windows Release x64 build passes.
- [ ] `TutonesScriptLocalBounds` passes.
- [ ] No crash or stuck pending operation occurs during the complete checklist.
- [ ] Stat rollback, planning-local bounds, event revalidation, network-control refusal, and arrival verification are observed at least once.
