# OpenHub continuity / handoff notes

_Last updated: 2026-10-01_

This file exists so a future ChatGPT session or contributor can continue OpenHub without reconstructing the project history from chat.

## Project identity

- Repository: `Nicodemo-Campos/OpenHub`
- Default branch: `main`
- Platform: Linux
- Language: C++20
- UI: Qt 6 Widgets
- Build: CMake + Ninja
- License: PolyForm Noncommercial 1.0.0
- Correct project description: **source-available**, not OSI open source.
- Commercial use is not granted by the repository license; see `COMMERCIAL_LICENSE.md`.

OpenHub is intended to become a Linux control center for Logitech/ASTRO gaming peripherals, inspired by the useful parts of G HUB without trying to clone its entire product surface.

## Design rules

The project is capability-driven, not a giant hardcoded compatibility list.

1. Discover the real HID endpoints.
2. Identify HID++ support and feature IDs.
3. Read before writing.
4. Only expose a control after the relevant capability is positively identified.
5. Never send arbitrary writes to unknown hardware.
6. Persistent profile writes use clone -> narrow patch -> CRC -> full write -> full read-back -> reload -> verification.
7. If a post-write verification fails, attempt to restore the exact original sector and verify the rollback.
8. Protocol-advertised capability is not automatically treated as hardware-validated behavior.

Unknown Logitech devices may be inspected generically. Completely unknown devices should be identified without speculative control commands.

## HID access

OpenHub ships a udev rule based on:

```
SUBSYSTEM=="hidraw", KERNEL=="hidraw*", ATTRS{idVendor}=="046d", TAG+="uaccess"
```

Install with:

```bash
sudo ./tools/install-udev-rules.sh
```

Reconnect the device afterward.

## Hardware available for real-device validation

### Logitech G502 LIGHTSPEED

- Direct USB identity seen during development: `046D:C08D`
- Receiver identity: `046D:C539`
- Working direct HID++ endpoint during USB tests: `/dev/hidraw7`
- Direct device index: `0x00`
- HID++ protocol: 4.2
- Profile format: `0x03`
- On-board profile sector size: **255 bytes**
- Exact C08D device is known to need the one-based/current-profile index quirk; OpenHub resolves active profile candidates dynamically instead of assuming one convention.

Hardware-validated G502 functionality through **v0.2.7.1**:

- safe HID++ feature discovery;
- battery telemetry;
- active DPI read/write through `0x2201`;
- device-reported DPI range validation;
- report-rate discovery through `0x8060`;
- persistent report-rate edit through the active `0x8100` profile;
- five persistent DPI stages;
- current/default/DPI-shift stage handling;
- persistent button assignment reader;
- safe persistent remapping for a narrow typed subset;
- `0x8070` Color LED Effects discovery;
- persistent **Primary** G502 lighting for Off / Static / Color cycle / Breathing.

Important G502 lighting finding:

- the device reports both `Primary` and `Logo` 0x8070 zones;
- the Primary profile record produced the expected physical LED change;
- the second Logo profile record could be written and CRC/read-back verified but did **not** produce an observed physical LED change;
- the device does not expose readable live 0x8070 effect settings in this configuration;
- therefore v0.2.7.1 keeps Primary write-enabled and all other reported zones read-only until their physical mapping is validated.

Do not undo this safety gate without new hardware evidence.

### Logitech G915 X

Observed development device:

- USB identity: `046D:C356`
- Reported name: `G915 X LS`
- Wired HID++ endpoint during tests: `/dev/hidraw10`
- Wired device index: `0x01`
- HID++ protocol: 4.2
- Previously observed feature count: 35

Known/observed relevant features:

- `0x1004` Unified Battery, version 5 — battery read already works through the generic live-state backend.
- `0x8071` RGB Effects, version 4.
- `0x8081` Per-Key Lighting v2, version 2.
- `0x8040` Brightness Control.
- `0x8101` Profile Management.
- `0x1B05` Full Key Customization.
- `0x1B10` Control List.
- `0x8051` Logitech Modifiers.
- `0x4523` Keyboard Disable Controls.
- `0x4540` Keyboard Layout 2.

The G915 X is the next major milestone after v0.2.7.1.

Public implementations/source research relevant to the G915 X:

- HID++ `0x8071` RGB Effects exposes device info, cluster info, and effect info. Feature indexes must be resolved at runtime.
- HID++ `0x8081` Per-Key Lighting v2 has no true live read-back of the current per-key RGB buffer.
- Known 0x8081 write functions include individual zones, range fill, same-color multi-zone batches, and frame commit.
- Per-key writes are a software-control/takeover path and must not be enabled until the required `0x8071` software-control behavior is validated on our actual G915 X.
- Do not assume another G915/G915 X model's feature index or device index; discover them at runtime.

Recommended first G915 milestone:

1. read-only `0x8071` RGB cluster/effect enumeration;
2. read-only `0x8081` address/key bitmap discovery;
3. expose the results in the HID++ controls/report;
4. hardware-validate the discovered clusters and address universe;
5. only then add an explicit, reversible software-control lighting test.

## ASTRO A50 X

Observed identity:

- `046D:0B0B`
- USB/base-station interface
- previously seen at `/dev/hidraw4`

A50 X controls are still research-only. Do not force its protocol through the direct HID++ G502/G915 path without evidence.

## Version history / validated milestones

- **v0.1.0** — Linux device discovery + inspector.
- **v0.1.1** — connection model + hidraw permission handling.
- **v0.2.0** — non-mutating HID++ feature probe.
- **v0.2.1** — G502 live DPI/report-rate/battery reads.
- **v0.2.2** — first DPI/report-rate setters; direct report-rate SET exposed the on-board-profile firmware restriction.
- **v0.2.2.1** — detect/block invalid direct report-rate writes in on-board mode.
- **v0.2.3 / .1 / .2** — persistent active-profile report-rate backend, active-profile resolution, support for 255-byte profile sectors.
- **v0.2.4** — persistent five-stage DPI editor + current stage switching.
- **v0.2.5** — read-only on-board button assignment decoder.
- **v0.2.6** — safe persistent button remapper with rollback.
- **v0.2.7** — first G502 Color LED Effects writer.
- **v0.2.7.1** — hardware-validation hotfix: Primary write-enabled, unvalidated reported zones read-only.

The user explicitly hardware-approved v0.2.4, v0.2.6, and v0.2.7.1. The G502 Primary RGB path also worked physically during v0.2.7 testing.

## G502 profile-memory facts currently used by OpenHub

For the validated profile format `0x03`:

- byte 0: report-rate interval in ms;
- byte 1: default DPI index;
- byte 2: DPI-shift index;
- bytes 3..12: five little-endian 16-bit DPI values;
- base button records begin at byte 32, four bytes each;
- G-Shift button records begin at byte 96, four bytes each when present;
- normal lighting record 0 begins at byte 208 and is 11 bytes;
- second normal lighting record begins at byte 219, but its physical mapping is intentionally not considered validated on the tested G502;
- final two sector bytes are CRC-16 CCITT, seed `0xFFFF`, stored big-endian.

Sector size does not need to be a multiple of 16. The tested G502 reports 255 bytes.

## Current write boundaries

Allowed only after runtime validation:

- G502 active DPI;
- G502 report rate;
- G502 on-board DPI stages;
- narrow G502 button remap subset;
- G502 Primary lighting subset.

Still protected / not writable:

- arbitrary raw HID++ records;
- macros;
- macro-backed/unknown button records;
- broader keyboard/consumer remapping;
- unvalidated G502 lighting zones;
- profile directory mutation;
- firmware / DFU;
- ASTRO controls;
- G915 X lighting until its software-control path is validated.

## Code landmarks

- `src/device/DeviceScanner.*` — Linux device/sysfs discovery.
- `src/device/DeviceKnowledge.*` — human-facing support/capability summaries.
- `src/hidpp/HidppProbe.*` — current HID++ transport, feature probe, live reads, validated writes.
- `src/app/MainWindow.cpp` — Qt Widgets UI and current control panels.
- `docs/HIDPP.md` — protocol notes and write boundaries.
- `docs/ARCHITECTURE.md` — current architecture notes.
- `CHANGELOG.md` — release-by-release summary.

The HID++ class is becoming large. A later refactor should separate transport/session, feature readers, device backends and UI-facing capability models, but avoid doing a large refactor in the middle of first G915 X hardware validation.

## Build / test loop

```bash
git pull
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/openhub
```

For device testing:

1. Rescan devices.
2. Inspect the target.
3. Open HID++ controls.
4. Use the narrowest possible reversible test.
5. Copy the control report after any unexpected behavior.
6. Treat the hardware observation as authoritative over assumptions made from protocol documentation.

## GitHub workflow

Development has been committed directly to `main` with the user's approval.

Before claiming a version/commit is ready, check the GitHub Actions run for the **current HEAD** and ensure the build completed successfully.

## Immediate next action

Start **v0.3.0** around the G915 X.

The safest first implementation is a read-only keyboard-lighting inspector:

- enumerate `0x8071` RGB Effects clusters and their reported effects;
- query `0x8081` key/address bitmap banks;
- show battery plus lighting capabilities in the UI/report;
- send no lighting mutation commands yet.

After the user's G915 X report confirms the actual cluster layout/key universe, add the first reversible software-control test in a subsequent point release.
