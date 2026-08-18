# Tutones Vehicle Paint Overlay v2

Second repo-shaped vehicle paint pass for Tutones-Menu.

V2 expands the V1 indexed primary/secondary controller with the paint groups shown by the current Vehicle / Paint UI:

- Primary/secondary indexed palettes: Chrome, Classic, Matte, Metals, Utility, Worn, Chameleon.
- Pearlescent uses classic indexed colours.
- Wheels accept Alloy, Classic, and Chameleon indices.
- Companion values are preserved on writes (primary/secondary and pearl/wheel pairs).
- Paint state can be read as one controller snapshot for later UI/service use.
- Invalid target/palette/index combinations are rejected before a backend write.

The native adapter is still intentionally separate from this layer.
