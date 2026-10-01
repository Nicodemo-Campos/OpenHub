# HID++ probing in OpenHub

OpenHub v0.2 introduces a deliberately small HID++ transport/probe layer.

The goal is **discovery before control**.

## Safety boundary

The v0.2 probe sends only:

- Root: GetProtocolVersion
- Root: GetFeature
- Feature Set: GetCount
- Feature Set: GetFeatureID

These operations discover protocol metadata. No setter or configuration function is implemented in the v0.2 probe.

The application also requires explicit user action before opening a hidraw endpoint. Startup scanning remains sysfs-only.

## Endpoint discovery

A Logitech USB device can expose several hidraw nodes. OpenHub does not assume that the first node is the vendor protocol interface.

For each node it reads the Linux HID report descriptor and parses Report ID items. A node becomes an HID++ candidate only when it advertises report ID:

- `0x10` — 7-byte short HID++ report
- `0x11` — 20-byte long HID++ report

OpenHub then sends Root.GetProtocolVersion and waits for a matching response carrying its software ID.

This is intentionally preferable to hardcoding paths such as `/dev/hidraw5`, because hidraw numbering changes across boots and USB topology changes.

## Device index

Direct Logitech HID++ devices are seen in the wild with more than one direct-device index convention. v0.2 therefore probes a tiny ordered set of direct indexes using the non-mutating protocol-version request and keeps the first one that produces a valid HID++ feature-protocol reply.

Receiver-child addressing is a separate problem and is not enabled in v0.2.

## Feature discovery

After the protocol endpoint is confirmed:

1. Root.GetFeature(`0x0001`) resolves the Feature Set index.
2. FeatureSet.GetCount returns the number of non-root features.
3. FeatureSet.GetFeatureID enumerates the device's live feature IDs.
4. Root.GetFeature is used to confirm each feature's runtime index and version.

Feature indexes are treated as runtime data. OpenHub does not assume, for example, that RGB is always at one particular feature index.

## Capability mapping

OpenHub currently groups discovered feature IDs into broad UI capabilities. Examples:

- `0x2201` / `0x2202` → adjustable DPI
- `0x8060` / `0x8061` → report rate
- `0x8070`, `0x8071`, `0x8080`, `0x8081` → lighting
- `0x8100`, `0x8101` → profile management
- `0x1B00`–`0x1B04` → reprogrammable controls
- `0x1000`, `0x1001`, `0x1004` → battery telemetry

The existence of a feature is not yet permission to write to it. A later backend still needs a validated read path, value/range checks, and device-specific safety constraints before a control can become writable.

## Protocol references used during implementation

OpenHub's v0.2 transport behavior was cross-checked against publicly available implementations and documentation, including:

- libratbag HID++ generic and HID++ 2.0 code:
  https://github.com/libratbag/libratbag
- Solaar HID++ feature definitions and discovery:
  https://github.com/pwr-Solaar/Solaar
- G915 X protocol notes used to validate direct/wireless endpoint behavior:
  https://github.com/TheMorpheus407/g915x-heatmap

OpenHub contains its own small implementation rather than embedding one of those projects as a runtime dependency.
