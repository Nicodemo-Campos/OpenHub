# OpenHub

OpenHub is a **source-available Linux control center for Logitech and ASTRO gaming peripherals**.

The long-term goal is not to maintain one giant hardcoded compatibility list. OpenHub should identify a device, discover what it can safely understand, and only expose controls that are positively supported.

## v0.1.0 — Discovery

The first milestone is intentionally read-only.

OpenHub v0.1:

- scans Linux USB HID and hidraw devices;
- identifies Logitech/ASTRO-family hardware when the kernel exposes enough information;
- recognizes the G502, G915 and ASTRO A50 X families when their reported names are available;
- shows VID/PID, transport, sysfs path, hidraw endpoints and filesystem permissions;
- classifies capabilities as Implemented, Detected, Planned or Research;
- includes a Device Inspector and a copyable diagnostic report;
- can optionally show non-Logitech HID devices for troubleshooting;
- **never opens hidraw endpoints and sends no configuration commands**.

This is the foundation for later DPI, polling-rate, lighting, profile, macro, battery and headset-control backends.

## Why discovery first?

Peripheral control software can write directly to hardware. OpenHub therefore starts from a conservative rule:

> If a capability has not been positively identified, do not expose a control for it.

A device may be:

- **Known family** — OpenHub recognizes the model/family and knows which backends are planned.
- **Logitech detected** — the vendor/device is recognizable, but model-specific control has not been implemented.
- **HID detected** — Linux exposes the device, but OpenHub makes no protocol assumptions.

## Build

Requirements:

- Linux
- C++20 compiler
- CMake 3.21+
- Qt 6 Widgets

On Debian/Ubuntu, the common build dependencies are:

    sudo apt install build-essential cmake ninja-build qt6-base-dev

Build and run:

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ./build/openhub

OpenHub v0.1 does not require elevated privileges for discovery. Future configuration features may need dedicated udev rules; the project will not recommend broad world-writable hidraw permissions.

## Initial hardware targets

Development is initially focused on:

- Logitech G502 wireless/LIGHTSPEED family
- Logitech G915 X family
- ASTRO A50 X

The architecture is deliberately capability-driven so future Logitech hardware can be supported without turning the application into a model-by-model switch statement.

## Roadmap

### v0.1
Safe device discovery, identity, capability classification and diagnostics.

### v0.2
First real control backend, starting with the G502 family: DPI, polling-rate discovery and safe capability probing.

### Later
Keyboard lighting, profiles, button mapping/macros, battery monitoring, automatic profile switching, ASTRO controls and packaging.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the current design.

## License

OpenHub is licensed under the **PolyForm Noncommercial License 1.0.0**.

Personal and other permitted noncommercial use is allowed under that license. Commercial use is **not granted** by the repository license. See [COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md) for the project's commercial-licensing policy.

Because the repository restricts commercial use, OpenHub is **source-available**, not OSI-defined open-source software.

## Contributions

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code. During the early architecture phase, substantive third-party code is not accepted until explicit contribution/relicensing terms are published.
