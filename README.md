# OpenHub

OpenHub is a **source-available Linux control center for Logitech and ASTRO gaming peripherals**.

The long-term goal is capability-driven support: identify a device, discover what it actually exposes, and only show controls backed by a positively identified protocol feature.

## v0.2.1 — Read-only live state

v0.2.1 builds on the HID++ capability probe and reads selected live values without changing device configuration.

The explicit **Read HID++ state (GET only)** action now:

- discovers the correct HID++ endpoint and device index at runtime;
- enumerates the device's live feature set;
- reads **Adjustable DPI (0x2201)** sensor count, supported range/list, current DPI, and default DPI;
- reads **Adjustable Report Rate (0x8060)** supported rates and current report interval/rate;
- reads **Battery Voltage (0x1001)** and shows voltage, charging state, and an explicitly approximate percentage;
- reads **Unified Battery (0x1004)** percentage/status where devices provide it;
- also understands **Battery Status (0x1000)** for future compatible devices;
- shows those values in a new Live State table;
- includes the live-state traffic in the copyable diagnostic report.

On the initial hardware this targets the G502's DPI/report rate/battery and the G915 X's Unified Battery.

**There are still no configuration setters in this release.** OpenHub v0.2.1 does not change DPI, polling rate, lighting, profiles, buttons, or other device settings.

## Safety model

Startup remains passive and sysfs-only. hidraw is opened only after the user explicitly requests a HID++ state read.

The live-state path implements only known discovery/read function IDs. It does not contain the corresponding SET functions.

That gives the project a deliberate progression:

    v0.2.0  discover capabilities
    v0.2.1  read current state
    later   validate and write selected settings

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

The HID++ request/response path needs read/write access to the relevant hidraw endpoint even for GET operations.

If OpenHub reports permission problems:

    sudo ./tools/install-udev-rules.sh

Then reconnect the Logitech/ASTRO devices and press **Rescan devices**.

The included rule uses `TAG+="uaccess"`. OpenHub does not recommend `chmod 666 /dev/hidraw*` or a world-writable hidraw rule.

See [docs/PERMISSIONS.md](docs/PERMISSIONS.md).

## Using v0.2.1

1. Open a directly attached Logitech device in **Inspect**.
2. Press **Read HID++ state (GET only)**.
3. OpenHub probes the endpoint/features and then reads the live values it understands.
4. Press **Copy state report** to share the complete result and TX/RX trace.

The action remains disabled for the LIGHTSPEED receiver object and A50 X until their dedicated transports are implemented.

## Initial hardware targets

Development is initially focused on:

- Logitech G502 wireless/LIGHTSPEED family
- Logitech G915 X family
- ASTRO A50 X

The architecture is deliberately capability-driven.

## Roadmap

### v0.2.1
Read-only DPI, report-rate, and battery state over capabilities confirmed at runtime.

### v0.2.2
First validated write controls for the G502, beginning with DPI/report rate only after the v0.2.1 reads are confirmed on hardware.

### Later
Keyboard lighting, profiles, button mapping/macros, automatic profile switching, receiver-child transport, ASTRO controls, and packaging.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/HIDPP.md](docs/HIDPP.md).

## License

OpenHub is licensed under the **PolyForm Noncommercial License 1.0.0**.

Personal and other permitted noncommercial use is allowed under that license. Commercial use is **not granted** by the repository license. See [COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md).

Because the repository restricts commercial use, OpenHub is **source-available**, not OSI-defined open-source software.

## Contributions

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code. During the early architecture phase, substantive third-party code is not accepted until explicit contribution/relicensing terms are published.
