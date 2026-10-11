# OVERDRIVE — Unreal edition

Native Unreal Engine **5.6** C++ port of Cloudtop Coast. The existing browser game in the repository root remains unchanged; this is a separate native project, not a browser wrapper.

## Validation status

The standalone C++ race/controller suite passes **1,710 assertions**, including complete three-lap runs in all classes with a heading-aware driver. The movement/camera suite passes **2,516 checks**, including naturally earned drift tiers, slipstream, recovery, no automatic corner following, lap/reward anti-farming, interruption-safe drift rewards, and camera comparisons at 30/60/144 fps. Strict Clang warnings and address/undefined-behavior sanitizers pass for the new movement/camera code. Actual Unreal test drives exercise the same simulation and camera through normal game ticks; they do not establish subjective enjoyment or substitute for physical-controller playtesting.

Verified before publication on macOS with **UE 5.6.1** and **Xcode 15.4**: UnrealHeaderTool and both native modules build successfully; the asset-generation commandlet creates the Cloudtop Coast map and native assets; and the game loads the level and runs its native GameMode. All five `Overdrive.Graphics` automation tests pass, covering winding/UVs, primitive normals, material slots, kart LOD/tangent quality, and wheel motion.

Packaged executables, Windows/Linux Unreal builds, a complete physical-controller race, and device-specific rumble remain unverified. Standalone core tests do not substitute for an Unreal build or hardware playtesting.

## Mac launch environment

**Double-click `LaunchUnreal.command` to open this project**, or use `Build.sh play` to build and play. Both remove `DYLD_INSERT_LIBRARIES` only from the launched process environment; globally injected desktop libraries can otherwise break Unreal's cross-architecture helper processes. Other applications and system settings are unchanged.

Set `UE_ROOT` if Unreal is installed elsewhere. This workaround does not guarantee compatibility between every macOS/Xcode release and UE 5.6. Read the supported toolchain requirements below.

## Requirements

- Unreal Engine **5.6**, including its C++ development tools. Other versions require retargeting and validation.
- macOS: UE 5.6's installed SDK configuration accepts **Xcode 15.2–16.9**. Use a version compatible with both your macOS release and UE. `Build.sh` selects a version-eligible toolchain from `/Applications`, `~/Applications`, `~/Downloads`, or `DEVELOPER_DIR` for this invocation only; `xcode-select` is not changed. Xcode installation is never automatic. The optional `InstallXcode.sh` requires a downloaded archive path and explicit `--yes` confirmation.
- Windows: Visual Studio 2022 with Game development with C++ and the UE 5.6-supported MSVC/Windows SDK.
- Linux: UE 5.6 and its supported native toolchain.
- A controller recognized by Unreal and the operating system. Xbox-style and PlayStation-style button labels are provided. Rumble availability depends on the device, connection, and platform driver.

[UE 5.6 Mac requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine?application_version=5.6)

## Build and play on Mac or Linux

From this directory, point `UE_ROOT` at your installed engine:

```bash
export UE_ROOT="/Users/Shared/Epic Games/UE_5.6"
bash Build.sh check
bash Build.sh play
```

`play` runs UnrealBuildTool, generates the missing native assets, then launches an Unreal game window. It does not install software or change system configuration. The first asset/shader/distance-field build can take several minutes.

Other actions:

- `bash Build.sh editor`: build, generate assets, and open the level in Unreal Editor.
- `bash Build.sh setup`: build and generate missing assets without launching the game.
- `bash Build.sh package`: build/cook/package a Development game into `Dist/`.
- `bash Build.sh test`: run standalone native tests; requires CMake 3.20+ and a C++20 compiler, not Unreal.

## Open directly / Windows

1. Generate IDE project files for `Overdrive.uproject`, then build the **OverdriveEditor** target in **Development Editor** for your platform.
2. Open `Overdrive.uproject`. The editor module creates missing materials, static meshes and `Content/Overdrive/Maps/CloudtopCoast.umap` on first startup. Existing assets are preserved.
3. Open `/Game/Overdrive/Maps/CloudtopCoast` if it is not already open. Select **Play > Standalone Game** for controller testing. In ordinary PIE, Escape may be intercepted by the editor's Stop command.
4. Wait for shader and distance-field compilation before assessing graphics or performance.

For headless generation before cooking, use the built editor executable with the project path and `-run=OverdriveSetup -unattended -nop4 -NullRHI`. The commandlet exits nonzero on failure. Inspect `Saved/Logs/` if generation fails. Do not package before the map and assets have been generated.

Generated `Content/Overdrive/` files are real editable Unreal assets and should be retained/versioned when available. `Binaries/`, `Intermediate/`, `Saved/` and `DerivedDataCache/` are build outputs. The asset generator does not overwrite existing content.

## Controls

| Action | Xbox / generic | PlayStation | Keyboard |
| --- | --- | --- | --- |
| Steer | Left stick | Left stick | A/D or arrows |
| Accelerate / brake | RT / LT | R2 / L2 | W/S or arrows |
| Drift | RB | R1 | Shift |
| Turbo | A | Cross | Space |
| Use item | X | Square | E |
| Look back | Y | Triangle | C |
| Recover kart (loses momentum and energy) | B | Circle | R |
| Pause | Menu | Options | Escape |
| Menu navigation / adjust | D-pad or left stick | D-pad or left stick | Arrows |
| Confirm / back | A / B | Cross / Circle | Enter / Escape |

Center the stick and release both triggers before starting or resuming. On-screen guidance appears while the neutral safety gate is active. Disconnecting the active controller pauses the race; reconnection never resumes automatically. Enter allows keyboard fallback while disconnected controller axes remain suppressed. Unrelated controllers cannot take over an active controller's input.

Settings include dead zone, steering sensitivity, analog throttle/braking, vibration, button labels, auto-accelerate, sound volume and calmer camera. New handling records use `OverdriveNative_v2`; the previous `OverdriveNative_v1` file remains untouched. Garage/controller preferences carry over, but old on-rails lap times do not compete with the new driving rules. Browser localStorage is not imported.

## Movement and mastery

- Steering changes heading; momentum and limited grip determine your line. Brake before tight turns, rather than holding full throttle and letting the track steer for you. Higher engine classes demand earlier braking.
- Tap drift while steering to hop into a slide, then modulate steering. Controlled slip charges blue, orange, and purple turbo tiers. Release to cash out; straighten the kart before release for a clean-exit bonus. Hitting a wall forfeits charge.
- Stay behind a moving rival briefly to earn a slipstream boost. Begin accelerating just after the final countdown number for a launch boost. Recovery costs momentum, energy, and a little progress.
- Physics runs at 60 Hz. Rendered kart poses interpolate between steps, and the chase camera follows that exact pose using damped springs. Normal driving never adds separate camera-position jumps; look-back and FOV transitions are gradual. Calmer camera keeps FOV fixed.

Research informing the design: [Ryan, Rigby and Przybylski (2006)](https://link.springer.com/article/10.1007/s11031-006-9051-8) report associations between perceived competence/autonomy and enjoyment; [Maddy's Celeste/TowerFall physics article](https://maddymakesgames.com/articles/celeste_and_towerfall_physics/index.html) describes simple, precise, predictable collision rules; [Ryan Juckett's Damped Springs](https://www.ryanjuckett.com/damped-springs/) explains continuous camera motion and frame-rate-independent spring integration. These support design principles, not a claim that one handling model is universally preferred. Drift/grip values remain playtest-tuning choices.

## Rendering and gameplay

- UE static meshes and native materials, Lumen lighting/reflections, TSR, bloom and dynamic shadows.
- Mac/Linux defaults use software Lumen. Windows enables hardware-ray-traced Lumen for supported DX12/SM6 GPUs; actual capability and fallback must be checked in Unreal. This does not promise hardware RT on every machine.
- Three engine classes, three laps, seven AI opponents, charged drift boosts, turbo strips, coins and three item types.
- Original procedural karts, floating islands, lighthouse, foliage, balloons and ocean. Editable generated meshes/materials and the Cloudtop Coast level are included in `Content/Overdrive`; the editor module can recreate missing assets.
- Fixed-step spline-based arcade handling, not Chaos Vehicles simulation; no multiplayer or split-screen.

Before release, build the Unreal modules, run the asset generator, play a complete native race, test controller disconnect/reconnect and keyboard fallback on real hardware, and package/run the result on each target platform.
