# OpenHub

OpenHub is a **source-available Linux control center for Logitech and ASTRO gaming peripherals**.

The long-term goal is capability-driven support: identify a device, discover what it actually exposes, and only show controls backed by a positively identified protocol feature.

## v0.2.2 — First validated hardware controls

v0.2.2 is the first OpenHub release that can intentionally change device state.

After the existing HID++ discovery/read sequence, OpenHub can expose controls only when the device itself reports the corresponding capability:

- **Adjustable DPI (0x2201)** — change the active sensor DPI;
- **Adjustable Report Rate (0x8060)** — change the active polling/report interval.

The initial real-hardware target is the Logitech G502 LIGHTSPEED.

Before every SET, OpenHub re-reads the device's supported range/list, validates the requested value, checks that the HID++ endpoint/protocol identity still matches the probe, sends the SET, and then performs a GET verification.

For the validated G502 this means the UI is built from the mouse's own reported constraints rather than from a hardcoded compatibility table.

## What v0.2.2 does not write

This release does **not** implement:

- on-board profile memory writes (0x8100/0x8101);
- RGB or per-key lighting writes;
- button remapping/macros;
- firmware/DFU operations;
- A50 X control writes;
- LIGHTSPEED receiver-child configuration.

The new controls target the active HID++ state. OpenHub deliberately does not call profile-memory write functions, so v0.2.2 is not a profile editor.

## Safety model

Startup is still passive and sysfs-only. Opening hidraw remains an explicit user action.

For a DPI change, the flow is:

    discover 0x2201
       -> read supported DPI list/range
       -> validate requested DPI
       -> SET_SENSOR_DPI
       -> GET_SENSOR_DPI
       -> accept only if the device reports the requested value

For report rate:

    discover 0x8060
       -> read supported-rate mask
       -> validate requested interval
       -> SET_REPORT_RATE
       -> GET_REPORT_RATE
       -> accept only if the device reports the requested value

If the endpoint identity changes, capability data cannot be re-read, a requested value is not supported, the SET fails, or verification does not match, OpenHub reports an error instead of assuming success.

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

The HID++ request/response path needs read/write access to the relevant hidraw endpoint.

If OpenHub reports permission problems:

    sudo ./tools/install-udev-rules.sh

Then reconnect the Logitech/ASTRO devices and press **Rescan devices**.

The included rule uses `TAG+="uaccess"`. OpenHub does not recommend `chmod 666 /dev/hidraw*` or a world-writable hidraw rule.

See [docs/PERMISSIONS.md](docs/PERMISSIONS.md).

## Using v0.2.2

1. Open the directly attached G502 in **Inspect**.
2. Press **Open HID++ controls**.
3. Confirm the live DPI/report-rate values look correct.
4. Choose a device-supported DPI or report rate and press the matching **Apply** button.
5. OpenHub sends the SET and immediately verifies it with a GET.
6. Use **Copy control report** if a write or verification fails.

The G915 X still receives read-only battery/capability handling in this release because v0.2.2 intentionally limits first-write testing to the already validated mouse features.

## Initial hardware targets

Development is initially focused on:

- Logitech G502 wireless/LIGHTSPEED family
- Logitech G915 X family
- ASTRO A50 X

The architecture is deliberately capability-driven.

## Roadmap

### v0.2.2
Validated active DPI and report-rate controls with read-back verification.

### v0.2.x
Polish the mouse control surface, handle wireless receiver-child transport, and decide how active-state changes interact with profiles.

### v0.3+
Keyboard lighting and additional configuration backends after their read/validation paths are established.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/HIDPP.md](docs/HIDPP.md).

## License

OpenHub is licensed under the **PolyForm Noncommercial License 1.0.0**.

Personal and other permitted noncommercial use is allowed under that license. Commercial use is **not granted** by the repository license. See [COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md).

Because the repository restricts commercial use, OpenHub is **source-available**, not OSI-defined open-source software.

## Contributions

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code. During the early architecture phase, substantive third-party code is not accepted until explicit contribution/relicensing terms are published.
