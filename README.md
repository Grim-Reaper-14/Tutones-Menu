# Tutones Vehicle Paint Overlay v4

Repo-shaped integration checkpoint for the next GitHub write.

The paint layer stays independent from ImGui and from the GTA native invoker. UI code only queues operations; the final Tutones adapter performs native calls on the existing GTA script-thread runtime path.

## Runtime behavior

- Primary/secondary indexed palettes: Chrome, Classic, Matte, Metals, Utility, Worn, Chameleon.
- Pearlescent uses classic indexed colors.
- Wheels accept Alloy, Classic, and Chameleon indices.
- Custom RGB primary and secondary paint are supported without replacing the stored indexed fallback.
- Selecting an indexed primary/secondary paint removes the corresponding custom RGB override only after the indexed value was written successfully.
- Custom override reset preserves the underlying indexed paint.
- Companion values are preserved on indexed writes (primary/secondary and pearl/wheel pairs).
- Queued operations are vehicle-stable: switching cars before execution drops the stale operation.
- Passive paint reads are throttled to 250 ms instead of polling multiple natives every script tick.
- Entering or switching vehicles refreshes immediately.
- Successful writes refresh the published snapshot immediately.
- Backend/read failures invalidate or preserve state conservatively instead of fabricating success.

The code is intended to be compiled and tested standalone under C++20 with warnings-as-errors and sanitizers before being wired into the current Tutones runtime.

The only intentionally missing piece is the thin adapter to Tutones' exact current `NativeRegistry`, `GameRuntime` queue, and `GameState` APIs. Those should be read from current `main` before the final merge rather than guessed.
