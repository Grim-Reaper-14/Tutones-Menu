# Enter Last Vehicle In-Game Verification

Run these checks after the Windows build succeeds. Start in a freemode session with the menu open on **Vehicle > General**.

## Normal entry

1. Enter a vehicle as the driver, exit it, move several meters away, and select **Enter Last Vehicle**.
2. Confirm the button changes to **Entering Last Vehicle...** and cannot be pressed again while the request is pending.
3. Confirm the local player is placed in that vehicle's driver seat.
4. Confirm the menu reports `SUCCESS` and says the driver seat was verified.
5. Repeat after exiting through a normal door animation and after bailing out while the vehicle is moving slowly.

## Target variations

- Repeat with a personal vehicle in GTA Online.
- Repeat with a stolen ambient vehicle.
- Repeat when the last vehicle is nearby but outside the camera view.
- Repeat after walking far enough away that the vehicle may stream out. Expect either verified entry while the entity exists or an explicit missing/disappeared-vehicle failure.

## Guard and failure paths

- Select the action before ever entering a vehicle in the session. Expect a `FAILED` result saying GTA has no last occupied vehicle.
- Use the action while already inside a vehicle. Expect a `FAILED` result instructing you to exit first.
- Let another ped take the last vehicle's driver seat. Expect an occupied-driver-seat failure; the other ped must not be displaced.
- Trigger the action and immediately respawn or change the local player model. Expect a local-player-changed failure.
- Destroy or despawn the last vehicle immediately after triggering the action. Expect an explicit disappeared-vehicle failure.
- Attempt rapid repeated clicks. Only one request should remain pending.
- Interrupt script-thread processing or unload during a pending request. The menu must not hang or continue retrying after shutdown.

## Acceptance criteria

- Native dispatch alone never produces `SUCCESS`; success requires both the current-vehicle handle and driver-seat ped to match the queued target and local player.
- Every accepted request reaches `SUCCESS` or `FAILED` within two seconds.
- Failures remain visible beneath the button and are also logged under `vehicle.enter_last`.
- No request evicts another driver, follows a changed player ped, or retries after its target disappears.
