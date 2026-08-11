# RuinPulse Design

RuinPulse is a focused phase-distorted pulse-avalanche synth. MIDI notes tune a monophonic pulse oscillator; each note retriggers a deterministic seed, an attack/decay envelope, and ruin bursts that fracture pulse transitions without using random host state.

## Product Checklist

- [x] Project name is `RuinPulse`.
- [x] Version is `0.1.0`.
- [x] App ID is `jp.ehl.ruinpulse`.
- [x] Plugin ID is `jp.ehl.ruinpulse`.
- [x] AU subtype is `RnPu`.
- [x] Plugin is a synth with MIDI input and stereo audio output.
- [x] State format uses stable parameter ID/value pairs.
- [x] State magic/version are unique to RuinPulse: `RNP1` / `1`.

## Host Parameter Checklist

- [x] Stable host parameter: `Pitch offset` / `pitch_offset`.
- [x] Stable host parameter: `Pulse` / `pulse`.
- [x] Stable host parameter: `Ruin` / `ruin`.
- [x] Stable host parameter: `Skew` / `skew`.
- [x] Stable host parameter: `Decay` / `decay`.
- [x] Stable host parameter: `Drive` / `drive`.
- [x] Stable host parameter: `Output` / `output`.
- [x] `Trigger` is UI runtime state, not a host parameter and not serialized.

## DSP Checklist

- [x] Note number determines oscillator frequency.
- [x] `Pitch offset` transposes note frequency.
- [x] `Pulse` controls pulse width.
- [x] `Skew` phase-distorts pulse timing.
- [x] `Decay` controls a retriggerable attack/decay envelope.
- [x] `Ruin` injects deterministic seeded avalanche bursts at transitions.
- [x] `Drive` applies cascaded nonlinear saturation.
- [x] Final output is finite and bounded to `[-0.98, 0.98]`.
- [x] Pre-trigger output is silent.
- [x] Release reaches a bounded silent tail.
- [x] Non-finite and out-of-range parameter inputs are sanitized.
- [x] Denormal-prone internal states are flushed to zero.
- [x] `processSample()` and `process()` allocate no memory.
- [x] Audio callback path avoids locks, file I/O, logging, and UI access.

## UI Checklist

- [x] Visual language uses a high-contrast hazard strobe and ruptured timing grid.
- [x] Visuals are drawn with YUP primitives; no external dependencies or assets.
- [x] Existing YUP parameter grid is reused.
- [x] Standalone editor has a momentary `Trigger` button.
- [x] Space acts as a held gate while the editor has focus.
- [x] Mouse gate and Space gate are combined into one desired standalone gate.
- [x] External MIDI has priority over standalone trigger while a MIDI note is active.
- [x] A held standalone gate resumes after MIDI note-off.
- [x] Focus loss and editor close publish a release fail-safe.
- [x] Output meter reads an atomic processor peak.

## Test Checklist

- [x] Engine tests prove silence before trigger.
- [x] Engine tests prove default RMS after attack is above the floor.
- [x] Engine tests prove deterministic output with max diff <= `1e-6`.
- [x] Engine tests prove finite bounded output under extreme inputs.
- [x] Engine tests prove release tail reaches <= `1e-5`.
- [x] Engine tests prove retriggerable decay.
- [x] Engine tests prove `Pulse`, `Skew`, and `Ruin` measurably alter transition density or spectrum proxies.
- [x] Plugin tests prove standalone trigger rendering and output metering.
- [x] Plugin tests prove rapid UI edge handling.
- [x] Plugin tests prove MIDI priority and standalone handoff.
- [x] Plugin tests prove state round-trip by stable IDs and `RNP1` rejection for invalid magic.

## CI and Release Checklist

- [x] `ci.yml` uses SHA-pinned actions.
- [x] `ci.yml` has a lightweight change classifier and heavy macOS/Windows jobs.
- [x] Heavy jobs configure, build, test, package, and checksum artifacts.
- [x] Artifacts expire after 14 days.
- [x] `release.yml` performs exact-SHA artifact promotion.
- [x] Release verifies the tag version against `project(RuinPulse VERSION ...)`.
- [x] Release verifies exactly one successful canonical CI run for the tagged SHA.
- [x] Release verifies SHA-256 manifests and ZIP integrity before publication.
- [x] Release publishes exactly two versioned ZIP assets.
