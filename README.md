# OpenHub

OpenHub is a **source-available Linux control center for Logitech and ASTRO gaming peripherals**.

The project is capability-driven: discover what a device actually exposes, then enable only controls backed by verified protocol features.

## v0.3.0 — G915 X lighting discovery

v0.3.0 starts the keyboard milestone with a deliberately **read-only** G915 X lighting inspector.

OpenHub now understands two lighting feature families exposed by the tested G915 X:

- **0x8071 RGB Effects** — firmware RGB clusters and their device-reported effects;
- **0x8081 Per-Key Lighting v2** — the addressable per-key LED universe.

No G915 X lighting SET, software-control claim, or frame commit is issued in v0.3.0.

### RGB Effects — 0x8071

OpenHub resolves the feature index at runtime and reads:

- cluster count;
- cluster index;
- location;
- persistency flags;
- device-reported effect count;
- effect ID;
- effect capability bits;
- effect period metadata.

The UI shows the real effect list reported by each cluster instead of assuming a fixed model table.

### Per-Key Lighting v2 — 0x8081

OpenHub reads the three key/address bitmap banks used by Per-Key Lighting v2 and decodes the addressable zone IDs.

The report includes:

- number of addressable zones;
- compact address-ID ranges;
- raw bitmap banks for protocol debugging.

Important: 0x8081 does **not** provide a true read-back of the current per-key RGB buffer. v0.3.0 therefore reports capability/address metadata only and does not pretend to know the current color of every key.

### Why writes are still disabled

Per-key RGB is a software-control/takeover path. Before writing anything on the user's G915 X we want to validate:

1. the actual 0x8071 cluster layout;
2. the actual 0x8081 address universe;
3. which LEDs correspond to special addresses such as logo/media/G-keys;
4. the software-control handshake needed before per-key frames;
5. a clean release/return-to-firmware path.

The first write milestone will be an explicit reversible lighting test after v0.3.0 hardware discovery is confirmed.

## Current G915 X support

- HID++ feature discovery;
- wired device-index probing;
- Unified Battery telemetry when 0x1004 is present;
- 0x8071 RGB cluster/effect discovery;
- 0x8081 per-key address bitmap discovery;
- copyable raw protocol trace.

Still read-only for:

- firmware RGB effects;
- per-key RGB;
- brightness;
- profile management;
- key remapping/customization.

## Current G502 support

The G502 backend remains hardware-validated through v0.2.7.1.

### DPI

- live DPI read/write through 0x2201;
- five persistent on-board DPI stages;
- default/current/DPI-shift stage handling.

### Report rate

- live report-rate read;
- persistent active-profile report-rate writes.

### Button assignments

- base/G-Shift decoding;
- safe persistent mouse-button and built-in-function remapping;
- protected macros/keyboard/consumer/unknown records.

### Lighting

- 0x8070 Color LED Effects discovery;
- hardware-validated persistent Off/Static/Cycle/Breathing settings for **Primary**;
- additional reported zones such as Logo remain visible but read-only until their physical mapping is validated.

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

## Testing v0.3.0 on the G915 X

1. Connect the G915 X by USB for the first validation pass.
2. Open the keyboard in **Inspect**.
3. Press **Open HID++ controls**.
4. Confirm battery telemetry still reads normally.
5. Find **G915 X lighting discovery — read-only**.
6. Note the 0x8071 clusters, locations and effect lists.
7. Note the 0x8081 addressable-zone count/ranges.
8. Press **Copy control report** and keep the RGB cluster + per-key bitmap sections.

No keyboard lighting mutation command is sent in this release.

## Initial hardware targets

- Logitech G502 LIGHTSPEED family
- Logitech G915 X family
- ASTRO A50 X

## Roadmap

### v0.3.0

Read-only G915 X RGB cluster and per-key address discovery.

### Next

Hardware-validate the v0.3.0 G915 X report, then add one explicit reversible software-control lighting test before building a full per-key editor.

### Later

- G915 X firmware effects and per-key editor;
- brightness and profile management;
- keyboard assignments;
- G502 polish/macros;
- automatic application profiles;
- receiver-child transport;
- ASTRO A50 X controls;
- packaging;
- eventual QML/UI overhaul.

## Continuity / handoff

See [docs/CONTINUITY.md](docs/CONTINUITY.md) for the current hardware observations, protocol decisions, safety boundaries, validated milestones, and exact next-step guidance for future sessions.

Also see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/HIDPP.md](docs/HIDPP.md).

## License

OpenHub is licensed under the **PolyForm Noncommercial License 1.0.0**.

Personal and other permitted noncommercial use is allowed under that license. Commercial use is **not granted** by the repository license. See [COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md).

Because the repository restricts commercial use, OpenHub is **source-available**, not OSI-defined open-source software.

## Contributions

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code.
