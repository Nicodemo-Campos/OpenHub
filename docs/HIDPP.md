# HID++ probing, reads, and validated controls in OpenHub

OpenHub follows **discovery before control**.

## v0.2.2 write boundary

The project now implements exactly two configuration operations:

- Adjustable DPI `0x2201`: `SetSensorDpi`;
- Adjustable Report Rate `0x8060`: `SetReportRate`.

Everything else remains read-only or unsupported.

The setter layouts were cross-checked against mature public HID++ implementations. For `0x2201`, the request carries sensor index + big-endian DPI. For `0x8060`, the request carries the report interval in milliseconds.

## Pre-write validation

A control is not made writable merely because OpenHub recognizes a model name.

The sequence is capability-driven:

1. perform the normal HID++ endpoint/protocol probe;
2. discover the feature ID and runtime feature index;
3. read the current state and supported values;
4. when Apply is pressed, reopen the endpoint;
5. run `Root.GetProtocolVersion` again to ensure endpoint identity still matches;
6. re-read the device-supported DPI list/range or report-rate mask;
7. reject the requested value if it is not supported;
8. send the SET;
9. read the value back and require an exact match.

A write is never reported as successful solely because the SET packet received a response.

## Adjustable DPI — 0x2201

Read operations:

- GetSensorCount;
- GetSensorDpiList;
- GetSensorDpi.

v0.2.2 adds:

- SetSensorDpi.

The DPI UI is generated from the device response. Range+step devices use a bounded spin control; discrete-list devices use only the advertised values.

The current G502 hardware test reports 100–25600 DPI in steps of 50, but those numbers are not used as the source of truth by the setter.

## Adjustable Report Rate — 0x8060

Read operations:

- GetReportRateList;
- GetReportRate.

v0.2.2 adds:

- SetReportRate.

The UI only offers intervals present in the device's bitmask. A G502 may report intervals corresponding to 1000, 500, 250, and 125 Hz, but OpenHub validates the live mask again immediately before writing.

## What is intentionally excluded

v0.2.2 does not write:

- On-board Profiles / Profile Management (0x8100/0x8101);
- Color LED / RGB / Per-Key Lighting;
- reprogrammable controls;
- hidden/internal features;
- DFU/firmware features;
- receiver-child devices;
- ASTRO A50 X.

No profile-memory write function is called by the new DPI/report-rate controls.

## Endpoint discovery

A Logitech USB device can expose several hidraw nodes. OpenHub parses HID report descriptors and considers a node an HID++ candidate only when it advertises report ID `0x10` and/or `0x11`.

The working device index is found with a non-mutating Root.GetProtocolVersion request. Runtime feature indexes are discovered through Feature Set and never hardcoded.

## Battery and other reads

Existing v0.2.1 read support remains:

- Battery Voltage 0x1001;
- Unified Battery 0x1004;
- Battery Status 0x1000;
- live DPI/report-rate state.

## Protocol references used during implementation

Request layouts and behavior were cross-checked against public implementations/documentation including:

- libratbag HID++ 2.0 code:
  https://github.com/libratbag/libratbag
- Solaar HID++ feature handling:
  https://github.com/pwr-Solaar/Solaar
- G915 X protocol notes:
  https://github.com/TheMorpheus407/g915x-heatmap

OpenHub contains its own implementation rather than embedding those projects as runtime dependencies.
