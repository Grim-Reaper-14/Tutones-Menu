# Vehicle paint integration notes

## V1 scope

The first version only models indexed primary/secondary paint.

The final Tutones adapter should implement `IVehiclePaintBackend` using the existing native runtime, while UI code should call the controller rather than invoke natives directly.
