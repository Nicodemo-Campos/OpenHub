# OpenHub architecture

OpenHub is built around **capabilities**, not just model names.

## v0.1.1 data flow

    Linux sysfs + effective file access
       |
       +-- /sys/bus/usb/devices
       +-- /sys/class/hidraw
       +-- access(2) for current-user ACL checks
       |
       v
    DeviceScanner
       |
       +-- current connection
       +-- device role
       +-- known wireless capabilities
       +-- receiver/direct-interface relationships
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

The scanner does not open `/dev/hidraw*` in v0.1.1. It reads kernel metadata and checks whether the current process would have filesystem/ACL access.

## Connection state is not device capability

A LIGHTSPEED-capable device connected through its data cable is currently using USB. Both facts matter:

    currentConnection = "USB (wired)"
    wirelessCapabilities = ["LIGHTSPEED"]

OpenHub keeps them separate so a wired charging session does not hide or mislabel the device's wireless features.

## Known identity metadata

v0.1.1 contains a deliberately tiny set of tested VID/PID identity hints for the initial development hardware. These hints annotate device role and connection capability; they do not activate write commands.

Protocol support must eventually come from backend probing, not from assuming that a product ID implies every feature.

## Design rules

1. **No blind writes.** A control must not appear until its capability is positively identified.
2. **Separate discovery from control.** Enumeration must remain useful even when a device has no writable backend.
3. **Separate current connection from wireless capability.**
4. **Backends own protocol knowledge.** HID++, lighting and headset-specific logic should live outside the UI.
5. **Unknown is a valid state.** OpenHub must be able to say “device detected, capability unknown” without guessing.
6. **Use narrow permissions.** Prefer session ACLs/`uaccess`; never require world-writable hidraw endpoints.
7. **Diagnostics should be shareable.** Reports should contain protocol-relevant metadata while avoiding unnecessary personal identifiers.

## Planned backend boundary

Future versions are expected to add a backend interface roughly along these lines:

    DeviceBackend
      - probe(device)
      - capabilities()
      - readState()
      - apply(setting)

Candidate implementations include a Logitech HID++ backend, a keyboard-lighting backend and an ASTRO headset backend.

The UI should consume backend-reported capabilities rather than checking product names directly.

## Why model-name/VID hints still exist

The small `DeviceKnowledge` and identity layer is a bootstrap mechanism for the initial hardware. It labels what OpenHub already knows while the real protocol-probing layer is still being built.

A future device should be able to move from "Logitech detected" to useful support because a backend recognizes its features, even if its exact model was never hardcoded.
