# OpenHub

OpenHub is a **source-available Linux control center for Logitech and ASTRO gaming peripherals**.

The project is capability-driven: discover what a device actually exposes, then enable only the controls backed by verified protocol features.

## v0.2.3 — Active on-board profile report rate

v0.2.3 adds the first **profile-memory** write path, initially for the G502 LIGHTSPEED-class HID++ 0x8100 layout.

The important change is how report rate is handled when **On-board Profiles** are enabled.

Instead of sending the direct 0x8060 SET that the G502 firmware rejects with `INVALID_ARGUMENT`, OpenHub now follows the profile path:

1. read the 0x8100 profile-memory descriptor;
2. read and CRC-check the user profile directory;
3. resolve the currently active profile and its sector;
4. read and CRC-check that exact sector;
5. clone the sector byte-for-byte;
6. change only byte 0, the profile report-rate interval;
7. recompute the HID++ CRC-CCITT;
8. write the sector back in 16-byte HID++ long-report chunks;
9. read the entire sector back and require an exact match;
10. verify the live 0x8060 rate;
11. if necessary, re-select the same profile so firmware reloads it.

If the final live verification fails, OpenHub attempts to restore the original profile sector.

## Current writable controls

### DPI — 0x2201

DPI is still an active-state control:

- re-read supported DPI range/list;
- reject unsupported values;
- send `SetSensorDpi`;
- verify with `GetSensorDpi`.

### Report rate — 0x8060 + 0x8100

- **Host mode:** direct validated 0x8060 SET + GET verification.
- **On-board mode:** persist the rate in the active 0x8100 profile sector and verify it.

The UI labels the persistent action **Save active profile rate** and asks for confirmation before writing profile memory.

## What v0.2.3 still does not write

OpenHub does not yet modify:

- profile directory entries;
- DPI tables stored inside profiles;
- button bindings or macros;
- RGB / per-key lighting;
- profile names;
- power settings;
- firmware / DFU;
- ASTRO A50 X controls;
- LIGHTSPEED receiver-child devices.

The profile writer intentionally preserves every unknown byte in the active profile sector.

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

## Testing v0.2.3 on the G502

1. Open the G502 in **Inspect**.
2. Press **Open HID++ controls**.
3. Confirm **On-board profiles** shows an active profile, sector, and valid CRC.
4. Choose a different supported report rate.
5. Press **Save active profile rate**.
6. Confirm the profile-memory warning.
7. After success, use **Copy control report** and verify the configuration action and memory trace.

A safe first test is 1000 Hz -> 500 Hz -> 1000 Hz.

## Initial hardware targets

- Logitech G502 LIGHTSPEED family
- Logitech G915 X family
- ASTRO A50 X

## Roadmap

### v0.2.3
Active on-board profile report-rate persistence with CRC and read-back verification.

### Next
Read the full G502 profile model in a user-friendly way, then add profile DPI slots and button bindings without rewriting unknown fields.

### Later
G915 X lighting, automatic application profiles, receiver-child transport, ASTRO controls, and packaging.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/HIDPP.md](docs/HIDPP.md).

## License

OpenHub is licensed under the **PolyForm Noncommercial License 1.0.0**.

Personal and other permitted noncommercial use is allowed under that license. Commercial use is **not granted** by the repository license. See [COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md).

Because the repository restricts commercial use, OpenHub is **source-available**, not OSI-defined open-source software.

## Contributions

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code.
