# Vehicle Stealth In-Game Verification

Run these checks in GTA Online after the Windows build succeeds. Use the local player as the driver and begin with the vehicle stationary in a normal freemode session.

## Supported vehicles

Repeat the activation and deactivation checks for:

- Akula
- Annihilator Stealth (`annihilator2`)
- F-160 Raiju

## Activation

1. Enter the driver seat and select **Enable Vehicle Stealth**.
2. Confirm the menu reports `PENDING` while it acquires network control.
3. Confirm the Akula/Annihilator wings fold or the Raiju missile bays close.
4. Confirm the final menu result says stealth was enabled and confirmed by GTA.
5. Confirm the vehicle weapons and homing behavior match GTA's normal stealth mode.
6. Have another player attempt a homing lock to confirm the radar/lock-on behavior, not only the visual animation.

## Deactivation

1. Select **Disable Vehicle Stealth**.
2. Confirm the wings or missile bays deploy.
3. Confirm the final menu result says stealth was disabled and confirmed by GTA.
4. Confirm normal weapon and homing behavior returns.

## Failure paths

- Switch vehicles immediately after queueing the request; expect a stale-request failure.
- Leave the driver seat during the transition; expect a driver-seat failure.
- Test an unsupported vehicle; expect the supported-model message.
- Test before joining GTA Online; expect a script-global/session failure.
- Repeat in a populated public session; the operation must either acquire network control and succeed or time out with the network-control reason.
- Rapidly press both stealth buttons; only one request should be accepted until the current transition finishes.

The operation must never report success from native dispatch alone. Success requires network control, physical hardware read-back, and the Enhanced `vehicle_stealth_mode` player-state bit to match the requested state.
