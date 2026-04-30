# Herbalist Architecture V2 (implementation scaffold)

This folder starts the implementation baseline for the new concept:
- data-driven source of truth,
- event-driven module coupling,
- deterministic simulation boundary.

## Included now
- `HerbalistIntentEvents.h` — canonical intent event payloads (UI/Application input side).
- `HerbalistFactEvents.h` — canonical fact event payloads (Simulation output side).

## Next implementation steps
1. Add `UHerbalistMessageRouterSubsystem` to map intents -> simulation commands.
2. Add `UHerbalistAssetCatalog` (PrimaryDataAsset) and move runtime paths into settings.
3. Restrict world/inventory state writes to simulation boundary only.
4. Add replay checksums and RNG stream snapshots.
