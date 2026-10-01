# OpenHub

OpenHub is a **source-available Linux control center for Logitech and ASTRO gaming peripherals**.

The project is capability-driven: discover what a device actually exposes, then enable only controls backed by verified protocol features.

## v0.2.6 — Safe persistent button remapping

v0.2.6 moves the G502 assignment work from read-only decoding into the first **persistent remapper**.

The writer is deliberately narrow. OpenHub does not accept arbitrary four-byte records from the UI. A remap must be one of the explicitly encoded assignment families below, and the active profile must pass the same descriptor, directory, sector and CRC validation used by the DPI/report-rate writers.

### Writable assignment subset

v0.2.6 can save:

- no action;
- a single mouse-button output;
- built-in Logitech profile functions from the validated subset:
  - tilt left/right;
  - next/previous/cycle/default/shift DPI;
  - next/previous/cycle profile;
  - G-Shift;
  - battery status;
  - scroll up/down.

Mouse outputs include the standard left/right/middle/back/forward records plus the documented higher mouse-button bits.

### Protected records

v0.2.6 intentionally refuses to overwrite:

- Base Button 1 and Base Button 2, to avoid disabling the two primary clicks in the first remapping release;
- macro execute/stop records;
- keyboard HID assignments;
- consumer/media HID assignments;
- unknown or invalid records;
- built-in function records with extra data or semantics outside the validated subset;
- button profiles whose 0x8100 profile format is not the G502-class format validated for this writer.

Read-only decoding remains broader than the write subset.

## Persistent remap flow

For one assignment change OpenHub:

1. re-opens and re-validates the HID++ endpoint;
2. confirms on-board profile mode;
3. re-reads the 0x8100 descriptor;
4. restricts the first remapper to validated profile format 0x03;
5. re-resolves the CRC-valid active profile sector;
6. validates the requested button slot and layer;
7. validates both the existing record and the requested typed action;
8. clones the complete profile sector;
9. changes exactly one four-byte button record;
10. recomputes the sector CRC;
11. writes the complete sector;
12. reads the whole sector back and requires exact equality;
13. reloads the same active profile;
14. restores the previous active DPI stage when possible;
15. reads the sector again after reload and requires the requested record to remain present.

If write/read-back, profile reload, DPI-stage restoration, or final verification fails, OpenHub attempts to restore the complete original sector and verifies that rollback.

## Current G502 support

### DPI

- live DPI read/write through 0x2201;
- five persistent on-board DPI stages;
- default/current/DPI-shift stage detection;
- activate a stored stage;
- persist stage values with CRC and read-back verification.

### Report rate

- read supported/current rate through 0x8060;
- host-mode direct write;
- on-board-mode active-profile persistence through 0x8100.

### Button assignments

- decode base and G-Shift tables;
- preserve raw four-byte records in diagnostics;
- persist the narrow v0.2.6 assignment subset;
- macro/keyboard/consumer/unknown records remain protected.

## Still intentionally excluded

v0.2.6 does not write:

- macros;
- keyboard/consumer button mappings;
- profile-select/mode-switch/host-button records with extra semantics;
- profile directory entries;
- RGB / per-key lighting;
- profile names;
- power settings;
- firmware / DFU;
- ASTRO A50 X controls;
- LIGHTSPEED receiver-child devices.

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

## Testing v0.2.6 on the G502

1. Open the G502 in **Inspect**.
2. Press **Open HID++ controls**.
3. Find **On-board button assignments**.
4. Start with a non-primary base slot, for example one currently mapped to DPI/Battery/Back/Forward.
5. Choose a simple reversible action such as **Mouse Back**, **Mouse Forward**, **Next DPI**, or **Battery status**.
6. Press **Save assignment** and confirm the profile-memory dialog.
7. Physically test that button.
8. Reopen OpenHub and verify the assignment is still stored.
9. Use **Copy control report** if anything differs from the expected behavior.

Base Button 1 and Base Button 2 are intentionally protected in this version.

## Initial hardware targets

- Logitech G502 LIGHTSPEED family
- Logitech G915 X family
- ASTRO A50 X

## Roadmap

### v0.2.6
Safe persistent remapping for a narrow set of G502 on-board button actions.

### Next
Validate remaps on real hardware, then decide whether to expand the safe writer to keyboard/consumer assignments or move to G502 lighting/profile polish before the G915 X milestone.

### Later
Macros, richer profiles, G915 X lighting, automatic application profiles, receiver-child transport, ASTRO controls, packaging, and the larger QML/UI overhaul.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/HIDPP.md](docs/HIDPP.md).

## License

OpenHub is licensed under the **PolyForm Noncommercial License 1.0.0**.

Personal and other permitted noncommercial use is allowed under that license. Commercial use is **not granted** by the repository license. See [COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md).

Because the repository restricts commercial use, OpenHub is **source-available**, not OSI-defined open-source software.

## Contributions

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code.
