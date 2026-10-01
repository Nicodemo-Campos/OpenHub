# OpenHub

OpenHub is a **source-available Linux control center for Logitech and ASTRO gaming peripherals**.

The long-term goal is capability-driven support: identify a device, discover what it actually exposes, and only show controls backed by a positively identified protocol feature.

## v0.2.0 — HID++ capability probing

v0.2 is the first version that actively talks to supported Logitech device interfaces.

Startup discovery remains passive. The new **Probe HID++ (GET only)** action is explicit and:

- inspects HID report descriptors first;
- only considers hidraw endpoints that advertise HID++ report ID `0x10` and/or `0x11`;
- tries the direct-device HID++ indexes used by current Logitech devices instead of assuming one fixed endpoint/index;
- sends a non-mutating `Root.GetProtocolVersion` request to identify the real HID++ endpoint;
- resolves `Feature Set (0x0001)` at runtime;
- enumerates live feature IDs, feature indexes, flags, and feature versions;
- translates many known feature IDs into readable names;
- derives high-level capability groups such as battery, DPI, report rate, buttons/remapping, lighting, and profiles from the feature set;
- generates a copyable low-level probe report including TX/RX trace data.

**v0.2 does not send configuration commands.** There are no DPI SETs, lighting SETs, profile writes, button remaps, onboard-memory changes, or headset writes in this release.

Receiver-child probing and A50 X protocol probing remain intentionally disabled until their transport layers are handled separately.

## Why this matters

OpenHub no longer needs to infer a G502 feature merely because the device name contains "G502".

A successful probe can instead say:

    HID++ protocol: 4.2
    0x2201 Adjustable DPI
    0x8060 Adjustable Report Rate
    0x8100 On-board Profiles

That is the foundation for supporting future hardware by capability rather than by a giant hardcoded model table.

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

The v0.2 probe needs read/write access to the relevant hidraw endpoint because HID++ GET requests are request/response packets sent through hidraw.

If OpenHub reports permission problems:

    sudo ./tools/install-udev-rules.sh

Then reconnect the Logitech/ASTRO devices and press **Rescan devices**.

The included rule uses `TAG+="uaccess"`. OpenHub does not recommend `chmod 666 /dev/hidraw*` or a world-writable hidraw rule.

See [docs/PERMISSIONS.md](docs/PERMISSIONS.md).

## Using the v0.2 probe

1. Open a directly attached Logitech device in **Inspect**.
2. Press **Probe HID++ (GET only)**.
3. OpenHub finds the HID++ vendor endpoint and enumerates the live feature set.
4. Press **Copy probe report** if you want to share the result for debugging/development.

The button is not offered for the LIGHTSPEED receiver object or the A50 X in v0.2.

## Initial hardware targets

Development is initially focused on:

- Logitech G502 wireless/LIGHTSPEED family
- Logitech G915 X family
- ASTRO A50 X

The architecture is deliberately capability-driven.

## Roadmap

### v0.2
Non-mutating HID++ endpoint detection and live feature enumeration.

### v0.2.x
Read current values for capabilities that the probe positively identifies, beginning with mouse DPI/report rate and battery where supported.

### v0.3+
Configuration controls, only after the corresponding read paths and safety checks are validated on real hardware.

Keyboard lighting, profiles, button mapping/macros, automatic profile switching, ASTRO controls, and packaging follow as their backends mature.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/HIDPP.md](docs/HIDPP.md).

## License

OpenHub is licensed under the **PolyForm Noncommercial License 1.0.0**.

Personal and other permitted noncommercial use is allowed under that license. Commercial use is **not granted** by the repository license. See [COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md).

Because the repository restricts commercial use, OpenHub is **source-available**, not OSI-defined open-source software.

## Contributions

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code. During the early architecture phase, substantive third-party code is not accepted until explicit contribution/relicensing terms are published.
