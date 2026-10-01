# OpenHub

OpenHub is a **source-available Linux control center for Logitech and ASTRO gaming peripherals**.

The project is capability-driven: discover what a device actually exposes, then enable only controls backed by verified protocol features.

## v0.2.5 — On-board button assignment reader

v0.2.5 begins the G502 assignments milestone by decoding the button table stored in the **currently active 0x8100 on-board profile**.

This release is intentionally **read-only for button assignments**. Existing validated DPI-stage and report-rate controls remain writable, but v0.2.5 does not rewrite any button or macro record yet.

For each profile button slot OpenHub now reads the four-byte binding record and identifies:

- normal/base layer;
- G-Shift alternate layer when the device reports it;
- mouse-button outputs;
- keyboard HID outputs with modifier bits;
- common consumer/media-key outputs;
- built-in Logitech functions such as DPI Shift, DPI cycling, profile cycling and battery status;
- macro execute/stop references;
- unknown or unsupported records without guessing their meaning.

The raw four bytes are always shown next to the interpreted assignment.

## Why button writes remain disabled

The profile descriptor reports the number of button slots, but those slot numbers are not yet presented as physical G502 labels such as “thumb back” or “DPI shift button”.

Before enabling remapping we want to validate the real slot order against the G502 hardware and confirm that the base/G-Shift records OpenHub decodes match the user's known assignments.

That keeps the project rule intact:

    read -> understand -> validate -> write

rather than turning a partially understood button table into a persistent profile-memory write.

## Current G502 profile support

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

v0.2.5 reads the active profile records beginning at the documented button-table offsets:

- base assignments: byte 32 onward;
- G-Shift assignments: byte 96 onward when the descriptor reports an alternate layer;
- four bytes per assignment;
- up to the descriptor-reported button count, capped at 16.

Recognized binding behaviors include SEND, FUNCTION and macro references. Unknown records remain visible as raw bytes rather than being assigned invented names.

## Still intentionally excluded

v0.2.5 does not write:

- button assignments;
- macros;
- G-Shift assignment data;
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

## Testing v0.2.5 on the G502

1. Open the G502 in **Inspect**.
2. Press **Open HID++ controls**.
3. Find **On-board button assignments — read-only**.
4. Compare the listed base assignments with what the mouse actually does.
5. If G-Shift is enabled, compare that layer too.
6. Use **Copy control report** and share the button-assignment section, especially any rows shown as Unknown.

No button-memory write is performed in this version.

## Initial hardware targets

- Logitech G502 LIGHTSPEED family
- Logitech G915 X family
- ASTRO A50 X

## Roadmap

### v0.2.5
Read and decode active-profile button assignments without modifying them.

### Next
After validating the G502 slot order and encoding, add a narrowly scoped persistent remapper for safe assignment types first. Macros remain a separate milestone.

### Later
Mouse lighting/profile polish, G915 X lighting, automatic application profiles, receiver-child transport, ASTRO controls, and packaging.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/HIDPP.md](docs/HIDPP.md).

## License

OpenHub is licensed under the **PolyForm Noncommercial License 1.0.0**.

Personal and other permitted noncommercial use is allowed under that license. Commercial use is **not granted** by the repository license. See [COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md).

Because the repository restricts commercial use, OpenHub is **source-available**, not OSI-defined open-source software.

## Contributions

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code.
