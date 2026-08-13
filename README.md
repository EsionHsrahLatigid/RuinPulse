# RuinPulse

RuinPulse is a YUP-based audio plugin and standalone synth that renders a phase-distorted pulse oscillator through deterministic ruin bursts, cascaded nonlinear drive, and bounded stereo output. It builds from this project directory and uses the adjacent `../yup` checkout when present.

## Identity

- Version: `0.1.0`
- App ID: `jp.ehl.ruinpulse`
- Plugin ID: `jp.ehl.ruinpulse`
- AU subtype: `RnPu`
- Plugin vendor: `ehl_`
- AU manufacturer: `EHL1`
- Type: stereo-output synth accepting MIDI input
- macOS formats: Standalone, VST3, AUv2
- Windows formats: Standalone, VST3
- State magic/version: `RNP1` / `1`

## Sound

MIDI note number sets the pulse oscillator frequency. `Pitch offset` transposes that frequency, `Pulse` changes pulse width, `Skew` phase-distorts the pulse timing, `Ruin` injects seeded transition avalanches, `Decay` controls the retriggered envelope, `Drive` cascades nonlinear saturation, and `Output` trims the final bounded signal.

## Standalone Controls

The editor includes a momentary `Trigger` control for the Standalone app. Press and hold the button, press and hold Space while the editor has keyboard focus, or hold both; the editor publishes the combined gate state to the processor. External MIDI has priority while held, and a held standalone gate resumes after the MIDI note releases.

The editor also shows an output activity meter. Trigger edges and meter values move through processor-owned atomics, so realtime rendering stays lock-free and allocation-free.

## Build

Clone with `--recurse-submodules`, or initialize the shared [yup-ehl-design-module](https://github.com/EsionHsrahLatigid/yup-ehl-design-module) before configuring:

```sh
git submodule update --init
```

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug
ctest --preset engine-debug
```

```sh
cmake --preset plugin-release
cmake --build --preset plugin-release
ctest --preset plugin-release
```

Release bundles are staged under `artifacts/plugin-release/<platform-arch>/{standalone,vst3,au}` by the common `ehl_stage_products` target. `build/` remains CMake's internal workspace; Windows uses `windows-x64` without AU.

For local macOS non-CI `plugin-release` builds, staged VST3 and AU bundles are also physically copied to `~/Library/Audio/Plug-Ins/VST3` and `~/Library/Audio/Plug-Ins/Components`. The Standalone app stays under `artifacts/plugin-release/<platform-arch>/standalone`. Configure with `-DEHL_COPY_PLUGIN_AFTER_BUILD=OFF` to disable the local plugin copy.

- `ruinpulse_release_bundles`
- `ruinpulse_standalone_plugin`
- `ruinpulse_vst3_plugin`
- `ruinpulse_au_plugin` on Apple platforms

## CI

`.github/workflows/ci.yml` is the required CI entrypoint for pushes to `main`, pull requests, and manual runs. It uses SHA-pinned GitHub actions, a lightweight Linux classifier, macOS arm64 and Windows x64 heavy jobs, 14-day artifacts, and strict `SHA256SUMS.txt` manifests.

`.github/workflows/release.yml` does not compile. A `v*` tag resolves to an exact commit, verifies the CMake version, requires one successful canonical `CI` push run on `main` for that same SHA, downloads exactly the two expected artifacts, verifies checksums and ZIP integrity, then promotes versioned release ZIPs.

## Layout

- `include/ruinpulse/` contains the realtime-safe DSP engine API and local DSP primitives.
- `source/` contains the engine implementation and YUP plugin/editor/state wrapper.
- `tests/` contains deterministic engine regression tests and plugin bridge tests for trigger, MIDI ownership, meter, and state.
- `cmake/` contains the project-local macOS icon conversion workaround used by the standalone target.
