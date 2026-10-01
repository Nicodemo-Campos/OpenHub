# HID++ probing and live-state reads in OpenHub

OpenHub follows **discovery before control**.

## v0.2.1 safety boundary

The current HID++ path sends only read/discovery operations:

- Root: GetProtocolVersion
- Root: GetFeature
- Feature Set: GetCount
- Feature Set: GetFeatureID
- Adjustable DPI 0x2201: GetSensorCount, GetSensorDpiList, GetSensorDpi
- Adjustable Report Rate 0x8060: GetReportRateList, GetReportRate
- Battery Voltage 0x1001: GetBatteryVoltage
- Unified Battery 0x1004: GetStatus
- Battery Status 0x1000: GetBatteryLevelStatus

No setter or configuration function is implemented in this path.

The application requires explicit user action before opening a hidraw endpoint. Startup scanning remains sysfs-only.

## Endpoint discovery

A Logitech USB device can expose several hidraw nodes. OpenHub does not assume that the first node is the vendor protocol interface.

For each node it reads the Linux HID report descriptor and parses Report ID items. A node becomes an HID++ candidate only when it advertises:

- `0x10` — 7-byte short HID++ report
- `0x11` — 20-byte long HID++ report

OpenHub then sends Root.GetProtocolVersion and waits for a matching response carrying its software ID.

This avoids hardcoding paths such as `/dev/hidraw7`, whose numbering can change across boots or USB topology changes.

## Feature discovery

After the protocol endpoint is confirmed:

1. Root.GetFeature(`0x0001`) resolves Feature Set.
2. FeatureSet.GetCount returns the number of non-root features.
3. FeatureSet.GetFeatureID enumerates the live feature IDs.
4. Root.GetFeature confirms each runtime index and version.

Feature indexes are runtime data. OpenHub never assumes, for example, that Adjustable DPI will always be feature index `0x0C`.

## Live-state readers

### Adjustable DPI — 0x2201

OpenHub reads:

- sensor count;
- each sensor's supported DPI values/range and step encoding;
- current DPI;
- default DPI.

The SET_SENSOR_DPI function is intentionally absent from v0.2.1.

### Adjustable Report Rate — 0x8060

OpenHub reads the supported interval bitmask and current interval, then presents the corresponding rate in Hz.

The SET_REPORT_RATE function is intentionally absent.

### Battery Voltage — 0x1001

OpenHub reads the battery voltage and status flags. The displayed percentage is marked approximate because it is estimated from voltage using the same public voltage curve used by mature Logitech tooling.

### Unified Battery — 0x1004

OpenHub reads the reported discharge percentage, coarse level code, and charge status. This is the path used by the tested G915 X.

## Feature naming

v0.2.1 also expands the feature registry for IDs observed on the test hardware, including Control List, Full Key Customization, Keyboard Layout 2, DFU-related IDs, Device Reset, and Enable Hidden Features.

Undocumented internal/hidden IDs remain labelled unknown rather than being guessed.

## Feature versions

The HID++ protocol version and individual feature versions are distinct concepts. v0.2.0 incorrectly displayed the protocol major version as the Root feature version. v0.2.1 now reads the version byte returned by FeatureSet.GetFeatureID, including for Root, and still cross-checks non-root features through Root.GetFeature.

## Protocol references used during implementation

The request layouts and parsers were cross-checked against public implementations/documentation including:

- libratbag HID++ 2.0 code:
  https://github.com/libratbag/libratbag
- Solaar HID++ feature and battery handling:
  https://github.com/pwr-Solaar/Solaar
- G915 X protocol notes:
  https://github.com/TheMorpheus407/g915x-heatmap

OpenHub contains its own implementation rather than embedding those projects as runtime dependencies.
