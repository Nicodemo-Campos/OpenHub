# OpenHub architecture

OpenHub is built around **capabilities**, not just model names.

## v0.2.3 data flow

    Linux sysfs
       |
       +-- USB/HID identity
       +-- hidraw endpoints
       +-- HID report descriptors
       +-- effective ACL access
       |
       v
    DeviceScanner
       |
       v
    DeviceInfo
       |
       +-----------------------------+
       |                             |
       v                             v
    DeviceKnowledge              Explicit user action
                                     |
                                     v
                                HidppProbe
                                     |
                                     +-- report 0x10/0x11 endpoint filtering
                                     +-- Root.GetProtocolVersion
                                     +-- Root.GetFeature(0x0001)
                                     +-- Feature Set enumeration
                                     |
                                     v
                                Live feature list
                                     |
                                     v
                                Capability groups
                                     |
                                     +-- read live values
                                     +-- validate requested DPI/rate
                                     +-- explicit SET
                                     +-- 0x8100 profile-sector clone/patch
                                     +-- CRC + full read-back verification
                                     +-- verification GET
       |                             |
       +-------------+---------------+
                     |
                     v
              Qt UI / Inspector

## Core design rules

1. **No blind writes.** A control must not appear until its capability is positively identified.
2. **Separate discovery from control.** Startup enumeration remains useful even when no device protocol is opened.
3. **Explicit protocol probing.** v0.2 opens hidraw only after the user presses the HID++ probe action.
4. **Read before write, verify after write.** A backend must validate capabilities and, for persistent memory, validate CRC/layout before configuration. Every write needs a device read-back or live-state verification.
5. **Feature indexes are runtime data.** HID++ feature indexes are resolved from the device and never assumed to be stable.
6. **Separate current connection from wireless capability.**
7. **Backends own protocol knowledge.** UI code consumes backend results rather than constructing raw protocol frames.
8. **Unknown is a valid state.** Unsupported/unknown feature IDs remain visible in diagnostics.
9. **Use narrow permissions.** Prefer session ACLs/`uaccess`; never require world-writable hidraw endpoints.
10. **Diagnostics should be shareable.** Probe reports include protocol traces but avoid unnecessary personal identifiers.

## Current modules

### DeviceScanner
Passive Linux discovery, topology, identity, and permission information.

### DeviceKnowledge
Bootstrap labels for known development hardware. It is not a protocol backend.

### HidppProbe
The current HID++ session layer:

- parses HID report descriptors;
- finds candidate report 0x10/0x11 endpoints;
- identifies the working device index with Root.GetProtocolVersion;
- enumerates Feature Set;
- maps feature IDs to readable names;
- reads validated live state;
- implements active DPI and host-mode report-rate setters;
- reads 0x8100 on-board profile metadata, directory sectors, and active profile sectors;
- persists active-profile report rate by cloning the exact sector, changing only the rate byte and CRC, and verifying the full read-back;
- re-checks supported values before each write and verifies the final live state.

This class is still small enough for the current milestone; a later refactor can split transport/probe/control interfaces as more writable backends are added.

## Planned backend boundary

The next layer should build on the probe result rather than re-discovering the device:

    DeviceBackend
      - probe(device)
      - capabilities()
      - readState()
      - validate(setting)
      - apply(setting)

The UI should consume backend-reported capabilities and validation ranges.

## Receiver and headset transports

Receiver-child HID++ addressing and the A50 X control protocol are kept outside the v0.2 direct-device probe. They need their own transport logic rather than being forced through the direct-device path.
