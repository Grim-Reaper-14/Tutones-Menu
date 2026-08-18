# Tutones Vehicle Paint Overlay v3

Repo-shaped integration checkpoint for the next GitHub write.

The paint layer is intentionally independent from ImGui and from the GTA native invoker. UI code only queues operations; the final Tutones adapter performs native calls on the existing GTA script-thread runtime path.

## Safety and runtime behavior

- Primary/secondary indexed palettes: Chrome, Classic, Matte, Metals, Utility, Worn, Chameleon.
- Pearlescent uses classic indexed colors.
- Wheels accept Alloy, Classic, and Chameleon indices.
- Companion values are preserved on writes (primary/secondary and pearl/wheel pairs).
- Queued operations are vehicle-stable: switching cars before execution drops the stale operation.
- Passive paint reads are throttled to 250 ms instead of polling multiple natives every script tick.
- Entering or switching vehicles refreshes immediately.
- Successful writes refresh the published snapshot immediately.
- Backend/read failures invalidate or preserve state conservatively instead of fabricating success.

The only intentionally missing piece is the thin adapter to Tutones' exact current `NativeRegistry`, `GameRuntime` queue, and `GameState` APIs. Those must be read from current `main` before the final merge rather than guessed.
