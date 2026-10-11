# OVERDRIVE

An original open-source kart racer on a floating coastal circuit. This repository contains both the browser prototype and the native Unreal Engine game, including its editable generated kart, track, and material assets.

## Two editions

| | Browser prototype | Unreal edition |
| --- | --- | --- |
| Engine | Custom WebGL2 renderer | Unreal Engine 5.6 |
| Language | TypeScript | C++ |
| Input | Keyboard and touch | Keyboard and gamepad |
| Graphics | Shadow maps, HDR, bloom, analytic ray-traced water reflections | Lumen lighting/reflections, detailed Comet GT karts, native materials and LODs |
| Handling | Original accessible arcade prototype | Momentum steering, grip/braking tradeoffs, drift hops and three turbo tiers, clean exits, slipstream and timed launches |

The Unreal edition includes the newer movement and camera overhaul. The browser edition is preserved separately; it does not include every native gameplay feature. This is an original game, not an official Mario Kart project, and contains no Nintendo characters or assets.

## Play the browser edition

Requires Node.js **22.18+** and npm. Node 24 LTS is recommended.

```sh
git clone https://github.com/offyotto/overdrive.git
cd overdrive
npm ci
npm run dev
```

Open the localhost URL printed by Vite, normally `http://127.0.0.1:5173`.

- `WASD` or arrow keys: drive / brake
- `Shift`: drift
- `Space`: turbo
- `E`: use item
- `Esc`: pause
- Touch controls appear on small screens.

For a production build: `npm run build`, then `npm run preview`. To expose a dev server to your LAN explicitly, use `npm run dev -- --host 0.0.0.0`; do not expose it to the public internet.

## Play the Unreal edition

Install **Unreal Engine 5.6** separately, with the supported C++ toolchain for your operating system. Engine binaries are not included or relicensed by this project.

On macOS or Linux:

```sh
cd unreal/Overdrive
UE_ROOT="/path/to/UE_5.6" bash Build.sh check
UE_ROOT="/path/to/UE_5.6" bash Build.sh play
```

On Windows, generate project files for `unreal/Overdrive/Overdrive.uproject`, build the `OverdriveEditor` target in **Development Editor**, then open `/Game/Overdrive/Maps/CloudtopCoast` and select **Play → Standalone Game**.

The generated `Content/Overdrive` assets are committed and can be edited in Unreal. Source generators can create missing assets; `bash Build.sh karts` refreshes only the V2 karts after backing them up. Allow time for first-run shaders and distance fields.

Native controls: left stick / `A D` steer; `RT/LT` / `W S` accelerate and brake; `RB` / `Shift` drift; `A` / `Space` turbo; `X` / `E` item; `Y` / `C` look back; `B` / `R` recover; Menu / `Esc` pause. PlayStation labels are available in settings. Recovery loses momentum and energy.

See [the Unreal setup and gameplay guide](unreal/Overdrive/README.md) for platform requirements, settings, build commands, and research informing the movement design.

## Development and tests

```sh
npm run build
npm test
bash unreal/Overdrive/Build.sh test
```

The standalone native tests require CMake 3.20+ and a C++20 compiler, but not Unreal. They exercise actual gameplay and camera code. Unreal graphics automation tests live in `Source/OverdriveEditor/OverdriveGraphicsTests.cpp`.

Verified before publication: browser production build and 14 tests; native Unreal 5.6.1 module build on macOS; 1,710 race/controller assertions; 2,516 movement/camera checks; and five Unreal graphics tests. Native test drives cover the new handling and camera. Windows/Linux builds, packaged executables, multiplayer, and physical controller/rumble behavior are not verified. This repository ships source and game assets, not a packaged executable.

## Repository layout

- `src/`, `tests/`: browser engine, gameplay, interface, and tests
- `unreal/Overdrive/Source/`: native game, portable simulation/camera core, and editor generators
- `unreal/Overdrive/Content/Overdrive/`: editable game meshes, materials, and Cloudtop Coast level
- `unreal/Overdrive/Config/`, `Tests/`: native configuration and standalone tests

Build caches, player saves, credentials, local diagnostics, and unrelated projects are intentionally excluded. Keep changes scoped and include regression tests where practical.

## License

The original game code, documentation, and authored/procedurally generated game assets in this repository are licensed under the [MIT License](LICENSE).

Unreal Engine and engine-provided resources referenced by the project remain subject to Epic's separate license; no Unreal Engine source, binaries, or proprietary engine assets are redistributed here. Build dependencies retain their own licenses. The browser requests Outfit and Barlow Condensed from Google Fonts; those fonts are separately licensed under the SIL Open Font License and are not bundled. System-font fallbacks work without that network request. See [NOTICE](NOTICE).
