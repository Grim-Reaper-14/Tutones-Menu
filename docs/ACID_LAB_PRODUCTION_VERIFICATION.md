# Acid Lab Instant Production In-Game Verification

Run these checks in GTA Online after the Windows build succeeds. Use an Acid Lab that has completed its setup mission.

## Normal fill

1. Sell or consume enough Acid so stock is below 160 units.
2. Open **Business > Acid Lab** and select **Instant Fill Production**.
3. Confirm the menu reports `Success`, `Persistent stock: VERIFIED`, and `Observed Acid stock: 160 / 160`.
4. When the live cache reports `UPDATED`, confirm the Acid Lab product display and sell screen show full stock without changing sessions.
5. Start a normal Acid sale and confirm the available quantity/value corresponds to full stock.

## Deferred live refresh

1. Trigger the fill while outside the Acid Lab or while its interior script is not active.
2. Confirm persistent stock still reports `VERIFIED` and the live cache reports `DEFERRED`.
3. Re-enter the Acid Lab or reopen its business screen.
4. Confirm the stock refreshes to 160 units from the persistent stat.

## Guard paths

- Use the action before joining GTA Online. Expect an online-session failure.
- Test on both GTA Online character slots. Only the active character's factory-6 stock may change.
- Test before completing Acid Lab setup. Expect a setup-required failure and no stock write.
- Press the action repeatedly. Only one request may be pending at a time.
- Run the action when stock is already full. Expect verified success without exceeding 160 units.

## Acceptance criteria

- The action never writes the unrelated `Global_2708938/2708939` freemode transaction/Nightclub-safe state.
- Success requires `MPX_PRODTOTALFORFACTORY6` to read back exactly 160 on the same active character.
- The live cache is updated only when its seven-entry factory array, Acid factory type 32, and existing 0-160 stock value validate.
- A missing or stale live cache is treated as deferred refresh, not as failure of the verified persistent write.
