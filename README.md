# OpenHub

OpenHub is a **source-available Linux control center for Logitech and ASTRO gaming peripherals**.

The project is capability-driven: discover what a device actually exposes, then enable only controls backed by verified protocol features.

## v0.3.1.1 — G915 X direct-frame hotfix

The first v0.3.1 hardware test produced a useful failure: the keyboard accepted the host-mode/software-control/Static commands, but the visible result was a fully dark keyboard.

v0.3.1.1 disables that firmware-zone experiment and instead uses the G915 X runtime per-key buffer. This path is now **hardware-validated on the tested wired G915 X**:

- re-read the three 0x8081 bitmap banks;
- require the exact 126-address universe validated in v0.3.0;
- take software lighting control with the G915 X handshake;
- paint all reported LED ranges one solid color through 0x8081;
- commit exactly one frame;
- release back to firmware after about five seconds.

This hotfix does not save a keyboard lighting profile or write a persistent 0x8071 effect record.

### Hardware result

The tested wired G915 X successfully rendered the transient solid 0x8081 frame and returned from the validation test. This confirms the direct per-key buffer path as the correct foundation for the next lighting milestone.

### Reproducing the hotfix test

1. Connect the G915 X by USB.
2. Open **Inspect → Open HID++ controls**.
3. Find **v0.3.1.1 transient 0x8081 solid-frame test**.
4. Keep the default magenta color.
5. Press **Test all reported LEDs — 5 seconds**.
6. Confirm whether the addressable keyboard LEDs turn magenta.
7. Confirm firmware/on-board lighting resumes after release.
8. Copy the control report if anything differs.

## v0.3.1 — First transient G915 X RGB test

v0.3.1 builds directly on the hardware-validated v0.3.0 G915 X discovery pass.

The tested wired keyboard reports:

- HID++ device index `0x01`;
- RGB Effects `0x8071` version 4;
- Per-Key Lighting v2 `0x8081` version 0;
- Profile Management `0x8101`;
- cluster 0 at location Primary with Static advertised.

Only that exact signature can unlock the new test.

### Five-second Primary Static test

The UI can now run one explicit reversible RGB test:

1. re-validate the endpoint and feature versions;
2. re-enumerate cluster 0 and require location Primary;
3. re-enumerate effects and locate Static by **effect ID** `0x0001`;
4. switch Profile Management `0x8101` to host mode;
5. claim RGB Effects `0x8071` software control;
6. send a **volatile** Primary Static effect with `persist=0`;
7. leave it visible for roughly five seconds;
8. release RGB software control;
9. return Profile Management to firmware mode.

The default test color is bright magenta so the physical change is easy to see.

The test can also be released manually, and closing the HID++ controls dialog performs a best-effort release if the transient claim is still active.

### What v0.3.1 still does not do

This is **not** the per-key editor yet.

v0.3.1 sends no:

- `0x8081` individual-key SET;
- `0x8081` range/batch SET;
- `0x8081` frame commit;
- persistent G915 X RGB profile write;
- brightness write;
- keyboard remap;
- macro or firmware write.

The point of v0.3.1 is to validate the software-control handoff and clean firmware return before we let OpenHub paint the 126 per-key addresses discovered in v0.3.0.

## G915 X discovery already validated in v0.3.0

OpenHub reads:

- `0x8071` RGB clusters, locations and device-reported effects;
- `0x8081` three bitmap banks and the addressable LED universe;
- `0x1004` Unified Battery telemetry;
- runtime feature indexes rather than model-hardcoded indexes.

Per-key RGB does not expose a true live current-color read-back, so capability/address metadata is reported without inventing current key colors.

## Current G502 support

The G502 backend remains hardware-validated through v0.2.7.1:

- active DPI and persistent five-stage DPI profiles;
- report-rate read/persistence;
- battery;
- safe persistent button remapping;
- hardware-validated Primary lighting;
- non-Primary reported lighting zones remain read-only until physically mapped.

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

If OpenHub reports hidraw permission problems:

    sudo ./tools/install-udev-rules.sh

Reconnect the device and press **Rescan devices**.

The included rule uses `TAG+="uaccess"`; OpenHub does not recommend world-writable hidraw permissions.

## Testing v0.3.1 on the G915 X

1. Connect the G915 X by USB.
2. Open **Inspect → Open HID++ controls**.
3. Confirm the v0.3.0 discovery data still appears.
4. Find **v0.3.1 transient Primary Static test**.
5. Leave the obvious default magenta color or choose another non-black RGB value.
6. Press **Test Primary static — 5 seconds** and confirm the warning.
7. Watch whether the expected Primary keyboard lighting changes.
8. Confirm the normal firmware lighting returns after about five seconds.
9. If it does not, press **Release to firmware**; reconnect the keyboard if needed.
10. Press **Copy control report** and keep the transient test/release trace.

## Initial hardware targets

- Logitech G502 LIGHTSPEED family
- Logitech G915 X family
- ASTRO A50 X

## Roadmap

### v0.3.1

Validate the first transient G915 X 0x8071 software-control handoff and release.

### Next

If the transient Primary test works and firmware returns cleanly, build the first tiny 0x8081 per-key paint experiment against known address IDs before attempting a visual keyboard editor.

### Later

- G915 X per-key editor, firmware effects, brightness and profiles;
- keyboard assignments;
- G502 polish/macros;
- automatic application profiles;
- receiver-child transport;
- ASTRO A50 X controls;
- packaging;
- eventual QML/UI overhaul.

## Continuity / handoff

See [docs/CONTINUITY.md](docs/CONTINUITY.md) for hardware observations, protocol decisions, safety boundaries, validated milestones and the exact next action.

Also see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/HIDPP.md](docs/HIDPP.md).

## License

OpenHub is licensed under the **PolyForm Noncommercial License 1.0.0**.

Personal and other permitted noncommercial use is allowed under that license. Commercial use is **not granted** by the repository license. See [COMMERCIAL_LICENSE.md](COMMERCIAL_LICENSE.md).

Because the repository restricts commercial use, OpenHub is **source-available**, not OSI-defined open-source software.

## Contributions

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code.
