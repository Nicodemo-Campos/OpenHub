# OpenHub

OpenHub is a **source-available Linux control center for Logitech and ASTRO gaming peripherals**.

The project is capability-driven: discover what a device actually exposes, then enable only controls backed by verified protocol features.

## v0.2.4 — On-board DPI stages

v0.2.4 expands the G502 on-board profile backend from report rate into the mouse's **five persistent DPI stages**.

For a validated active `0x8100` profile, OpenHub can now:

- read all five DPI slots from profile memory;
- identify the profile's default DPI stage;
- identify the DPI-shift stage stored in the profile;
- read the currently active DPI stage through `GetCurrentDpiIndex`;
- activate an enabled stage through `SetCurrentDpiIndex`;
- enable/disable individual stages;
- persist new stage values and the default-stage index in the active profile.

A disabled DPI slot is represented by `0 DPI` in the profile, matching the established HID++ profile layout.

## Persistent DPI-stage write flow

Before saving, OpenHub:

1. confirms `0x8100` On-board Profiles and `0x2201` Adjustable DPI;
2. re-reads the hardware-supported DPI range/list;
3. validates every enabled slot against that range/step;
4. requires at least one enabled stage;
5. requires the selected default stage to remain enabled;
6. re-resolves the active profile and CRC-valid sector;
7. clones the exact profile sector;
8. changes only the documented default-stage byte plus the five little-endian DPI values;
9. recomputes the profile CRC;
10. writes and reads the complete sector back;
11. reloads the same active profile;
12. restores/chooses a valid current stage and verifies both the active stage index and live DPI.

If final live verification fails, OpenHub attempts to restore the original profile sector.

## Current G502 controls

### Active DPI — 0x2201

Direct runtime DPI control remains available with device-reported range validation and read-back verification.

### On-board DPI stages — 0x8100 + 0x2201

The active profile exposes five stages. The UI shows which stage is:

- current;
- default;
- DPI-shift;
- disabled.

**Save DPI stages to active profile** performs the persistent profile-memory write.

**Activate stage** changes the current on-board DPI index and verifies the resulting live DPI without rewriting the profile sector.

### Report rate — 0x8060 + 0x8100

- Host mode: direct validated `0x8060` SET.
- On-board mode: persist the selected rate in the active profile sector.

## Still intentionally excluded

v0.2.4 does not yet write:

- button bindings;
- macros;
- DPI-shift assignment itself;
- profile directory entries;
- RGB / per-key lighting;
- profile names;
- power settings;
- firmware / DFU;
- ASTRO A50 X controls;
- LIGHTSPEED receiver-child devices.

The profile writer continues to preserve every byte that is not part of the narrow field being edited.

## Build

Requirements:

- Linux
- C++20 compiler
- CMake 3.21+
- Qt 6 Widgets

On Debian/Ubuntu:

    sudo apt install build-essential cmake ninja-build qt6-base-dev

Build and run:

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ./build/openhub

## HID permissions

The HID++ path needs read/write access to the relevant hidraw endpoint.

If OpenHub reports permission problems:

    sudo ./tools/install-udev-rules.sh

Reconnect the device and press **Rescan devices**.

The included rule uses `TAG+="uaccess"`; OpenHub does not recommend world-writable hidraw permissions.

See [docs/PERMISSIONS.md](docs/PERMISSIONS.md).

## Testing v0.2.4 on the G502

1. Open the G502 in **Inspect**.
2. Press **Open HID++ controls**.
3. Confirm the on-board profile shows valid directory/profile CRCs.
4. Inspect the five DPI stages and their current/default/shift markers.
5. First try **Activate stage** on another already-enabled stage.
6. Then change one stage to another supported DPI value and press **Save DPI stages to active profile**.
7. Close/reopen OpenHub and verify the stage remains stored.

Use **Copy control report** after testing; v0.2.4 includes the on-board DPI-stage table and write trace.

## Initial hardware targets

- Logitech G502 LIGHTSPEED family
- Logitech G915 X family
- ASTRO A50 X

## Roadmap

### v0.2.4
Persistent on-board DPI-stage reading/editing plus active-stage switching.

### Next
Decode the G502 button-binding table safely and expose assignments without touching macros first.

### Later
Macros, mouse lighting/profile polish, G915 X lighting, automatic application profiles, receiver-child transport, ASTRO controls, and packaging.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/HIDPP.md](docs/HIDPP.md).

## License

OpenHub is licensed under the **PolyForm Noncommercial License 1.0.0**.

Personal and other permitted noncommercial use is allowed under that license. Commercial use is **not granted** by the repository license. See [COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md).

Because the repository restricts commercial use, OpenHub is **source-available**, not OSI-defined open-source software.

## Contributions

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code.
