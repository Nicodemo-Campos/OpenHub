# OpenHub

OpenHub is a **source-available Linux control center for Logitech and ASTRO gaming peripherals**.

The project is capability-driven: discover what a device actually exposes, then enable only controls backed by verified protocol features.

## v0.2.7.1 — G502 lighting validation hotfix

v0.2.7.1 keeps the v0.2.7 lighting discovery/editor but tightens the write boundary after real G502 hardware validation.

OpenHub now enumerates HID++ **0x8070 Color LED Effects** at runtime:

- zone count;
- zone location;
- device-reported effect list for each zone;
- effect IDs/capabilities;
- current live effect when the device exposes readable zone settings.

The UI only offers effects that the zone actually reports.

### Writable lighting subset

For the first persistent lighting release, OpenHub can write these profile effects when the **Primary** zone reports them:

- Off (`0x0000`);
- Static (`0x0001`);
- Color cycle (`0x0003`);
- Breathing (`0x000A`).

Static and Breathing expose RGB controls. Color cycle and Breathing expose period/intensity controls.

Unknown and more complex effects remain read-only.

## Persistent profile lighting

The G502-class profile format already validated by OpenHub stores normal lighting records at:

- zone 0: byte 208, length 11;
- zone 1: byte 219, length 11.

v0.2.7.1 keeps both reported records visible for diagnostics, but persistent writes are now restricted to **zone 0 when its reported location is Primary (`0x0001`)**. The second reported Logo zone stays read-only because changing its second profile record was verified in flash but did not produce an observed physical LED change on the tested G502 LIGHTSPEED.

For every lighting change OpenHub:

1. re-enumerates 0x8070;
2. confirms the requested effect is in that zone's hardware-reported list;
3. validates on-board profile mode and profile format 0x03;
4. re-resolves the CRC-valid active profile sector;
5. clones the entire sector;
6. replaces only the first 11-byte Primary lighting record;
7. recomputes CRC;
8. writes and reads the complete sector back;
9. reloads the same active profile;
10. restores the current DPI stage when possible;
11. reads the whole sector again;
12. when 0x8070 exposes readable live settings, verifies the actual live effect too.

Any post-write verification failure triggers an attempt to restore and verify the original complete sector.

## Current G502 support

### DPI
- live DPI read/write through 0x2201;
- five persistent DPI stages;
- default/current/DPI-shift stage handling.

### Report rate
- live report-rate read;
- persistent active-profile report-rate writes.

### Button assignments
- base/G-Shift decoding;
- safe persistent mouse-button and built-in-function remapping;
- protected macros/keyboard/consumer/unknown records.

### Lighting
- 0x8070 zone/effect discovery;
- current effect read when supported;
- hardware-validated persistent Off/Static/Cycle/Breathing settings for Primary;
- additional reported zones, including Logo on the tested G502, remain visible but read-only until their physical mapping is validated.

## Still intentionally excluded

v0.2.7.1 does not write:

- non-Primary reported lighting zones whose physical mapping is unvalidated;
- unknown/complex lighting effects;
- alternate lighting records;
- custom animations;
- macros;
- keyboard/consumer button mappings;
- profile directory entries;
- profile names;
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

## Testing v0.2.7.1 on the G502

1. Open the G502 in **Inspect**.
2. Press **Open HID++ controls**.
3. Find **Color LED Effects — 0x8070**.
4. Note how many zones and effects the mouse reports.
5. Confirm **Primary** is the only write-enabled zone.
6. Start with a reversible Primary change such as Static with a clearly different RGB color.
7. Press **Save lighting to active profile** and confirm the physical lighting changes.
8. Confirm reported non-Primary zones such as Logo are visible but read-only.
9. Close/reopen OpenHub and verify the stored Primary effect remains.
10. Use **Copy control report** if the reported zones differ from expected behavior.

## Initial hardware targets

- Logitech G502 LIGHTSPEED family
- Logitech G915 X family
- ASTRO A50 X

## Roadmap

### v0.2.7.1
Keep G502 Color LED discovery intact while restricting persistent writes to the hardware-validated Primary path.

### Next
Finish any G502 lighting quirks found on real hardware, then either polish profiles/assignments or begin the G915 X lighting milestone using the reusable lighting model.

### Later
Macros, richer profiles, G915 X per-key lighting, automatic application profiles, receiver-child transport, ASTRO controls, packaging, and the larger QML/UI overhaul.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/HIDPP.md](docs/HIDPP.md).

## License

OpenHub is licensed under the **PolyForm Noncommercial License 1.0.0**.

Personal and other permitted noncommercial use is allowed under that license. Commercial use is **not granted** by the repository license. See [COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md).

Because the repository restricts commercial use, OpenHub is **source-available**, not OSI-defined open-source software.

## Contributions

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code.
