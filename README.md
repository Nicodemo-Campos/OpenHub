# OpenHub

OpenHub is a **source-available Linux control center for Logitech and ASTRO gaming peripherals**.

The long-term goal is not to maintain one giant hardcoded compatibility list. OpenHub should identify a device, discover what it can safely understand, and only expose controls that are positively supported.

## v0.1.1 — Connection model & permissions

v0.1.1 keeps discovery read-only, but improves what OpenHub learned from the first real-hardware test.

It now:

- separates **current connection** from **wireless capability**;
- distinguishes physical devices, USB receivers and the A50 X base-station interface;
- recognizes the tested IDs `046D:C08D` (G502 direct USB), `046D:C539` (LIGHTSPEED receiver), `046D:C356` (G915 X direct USB) and `046D:0B0B` (A50 X USB/base station) as identity metadata;
- links receiver/direct interfaces when both sides of the same known family are visible;
- checks effective hidraw access with the current user's actual ACLs;
- clearly reports when HID access is blocked by permissions;
- ships a narrow `udev` rule using `TAG+="uaccess"` instead of world-writable HID permissions;
- still **does not open hidraw endpoints and sends no configuration commands**.

The first milestone's Device Inspector, VID/PID reporting, sysfs discovery, capability status and copyable diagnostics remain available.

## Why connection and capability are separate

A wireless-capable device may currently be attached by cable. For example:

    Logitech G915 X
    Connected now: USB (wired)
    Wireless capability: LIGHTSPEED + Bluetooth

Likewise, a G502 may expose both its direct USB identity and its LIGHTSPEED receiver at the same time while charging.

OpenHub therefore treats these as different facts instead of reducing both to one ambiguous "Transport" field.

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

Discovery itself does not require elevated privileges.

## HID permissions

If OpenHub reports:

    HID access: permission needed

install the included session-scoped udev rule:

    sudo ./tools/install-udev-rules.sh

Then reconnect the Logitech/ASTRO devices and press **Rescan devices**.

The rule targets Logitech vendor ID `046d` and uses `TAG+="uaccess"`; OpenHub does not recommend `chmod 666 /dev/hidraw*` or a global world-writable hidraw rule.

See [docs/PERMISSIONS.md](docs/PERMISSIONS.md) for details.

## Initial hardware targets

Development is initially focused on:

- Logitech G502 wireless/LIGHTSPEED family
- Logitech G915 X family
- ASTRO A50 X

The architecture is deliberately capability-driven so future Logitech hardware can be supported without turning the application into a model-by-model switch statement.

## Roadmap

### v0.1.1
Safe discovery, explicit connection-vs-capability modeling, related-interface detection and HID permission diagnostics.

### v0.2
First real protocol backend: safe HID++ probing and G502 DPI/polling-rate discovery before any configuration writes are enabled.

### Later
Keyboard lighting, profiles, button mapping/macros, battery monitoring, automatic profile switching, ASTRO controls and packaging.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the current design.

## License

OpenHub is licensed under the **PolyForm Noncommercial License 1.0.0**.

Personal and other permitted noncommercial use is allowed under that license. Commercial use is **not granted** by the repository license. See [COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md) for the project's commercial-licensing policy.

Because the repository restricts commercial use, OpenHub is **source-available**, not OSI-defined open-source software.

## Contributions

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code. During the early architecture phase, substantive third-party code is not accepted until explicit contribution/relicensing terms are published.
