# OpenHub architecture

OpenHub is built around **capabilities**, not just model names.

## v0.2 data flow

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
       |                             |
       +-------------+---------------+
                     |
                     v
              Qt UI / Inspector

## Core design rules

1. **No blind writes.** A control must not appear until its capability is positively identified.
2. **Separate discovery from control.** Startup enumeration remains useful even when no device protocol is opened.
3. **Explicit protocol probing.** v0.2 opens hidraw only after the user presses the HID++ probe action.
4. **GET before SET.** A backend must validate read/discovery behavior before configuration is enabled.
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
A non-mutating HID++ transport/probe implementation. It:

- parses HID report descriptors;
- finds candidate report 0x10/0x11 endpoints;
- identifies the working device index with Root.GetProtocolVersion;
- enumerates Feature Set;
- maps feature IDs to readable names.

It intentionally implements no setters.

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
