# Vehicle paint integration notes

## V2 scope

The controller now covers the indexed paint groups visible in Tutones' Vehicle / Paint UI.

The final adapter needs only four focused native operations:

- get indexed vehicle colours
- set indexed vehicle colours
- get extra colours
- set extra colours

Primary/secondary writes preserve the other indexed colour. Pearlescent/wheel writes preserve the other extra colour.

This layer remains independent from ImGui and from the exact Tutones native invoker API.
