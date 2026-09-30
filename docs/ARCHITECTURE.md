# OpenHub architecture

OpenHub is built around **capabilities**, not just model names.

## v0.1 data flow

    Linux sysfs
       |
       +-- /sys/bus/usb/devices
       +-- /sys/class/hidraw
       |
       v
    DeviceScanner
       |
       v
    DeviceInfo
       |
       v
    DeviceKnowledge
       |
       +-- Known family
       +-- Logitech detected
       +-- Generic HID
       |
       v
    Qt UI / Device Inspector

The scanner does not open `/dev/hidraw*` in v0.1. It reads kernel metadata and filesystem permission information only.

## Design rules

1. **No blind writes.** A control must not appear until its capability is positively identified.
2. **Separate discovery from control.** Enumeration must remain useful even when a device has no writable backend.
3. **Backends own protocol knowledge.** HID++, lighting and headset-specific logic should live outside the UI.
4. **Unknown is a valid state.** OpenHub must be able to say “device detected, capability unknown” without guessing.
5. **Diagnostics should be shareable.** Reports should contain protocol-relevant metadata while avoiding unnecessary personal identifiers.

## Planned backend boundary

Future versions are expected to add a backend interface roughly along these lines:

    DeviceBackend
      - probe(device)
      - capabilities()
      - readState()
      - apply(setting)

Candidate implementations include a Logitech HID++ backend, a keyboard-lighting backend and an ASTRO headset backend.

The UI should consume backend-reported capabilities rather than checking product names directly.

## Why model-name hints still exist in v0.1

v0.1 has a small `DeviceKnowledge` layer for the initial hardware families. It is not a control backend. It only labels the development status of known targets while the real protocol probing layer is still being built.
