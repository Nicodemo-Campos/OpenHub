# Changelog

## 0.2.2

- Added the first intentional HID++ configuration writes.
- Added active DPI SET support for Adjustable DPI (0x2201).
- Added active report-rate SET support for Adjustable Report Rate (0x8060).
- Re-reads device-supported ranges/masks before every write.
- Rejects unsupported DPI/report-rate values before sending a SET.
- Performs a protocol identity check immediately before configuration.
- Verifies every successful SET with a corresponding GET.
- Added capability-driven DPI/report-rate controls to the HID++ dialog.
- Added configuration actions and SET/verification traces to the copyable control report.
- Kept profile-memory, lighting, remap, DFU, receiver-child, and A50 X writes disabled.

## 0.2.1

- Added read-only HID++ live-state queries after capability discovery.
- Added Adjustable DPI (0x2201) sensor/range/current/default reads.
- Added Adjustable Report Rate (0x8060) supported/current reads.
- Added Battery Voltage (0x1001) voltage/status reads with an explicitly approximate voltage-derived percentage.
- Added Unified Battery (0x1004) percentage/status reads.
- Added Battery Status (0x1000) read support for compatible future devices.
- Added a Live State table and copyable combined state/probe report.
- Expanded feature names for IDs observed on the G502/G915 X.
- Fixed feature-version reporting: Root now uses the version returned by Feature Set instead of the HID++ protocol major version.
- Kept all configuration/SET commands disabled.

## 0.2.0

- Added explicit, non-mutating HID++ capability probing.
- Added HID report-descriptor parsing to identify report 0x10/0x11 vendor endpoints.
- Added runtime HID++ device-index detection for directly attached Logitech devices.
- Added Root.GetProtocolVersion probing.
- Added runtime Feature Set enumeration and feature-version resolution.
- Added readable names for common Logitech HID++ feature IDs.
- Added high-level capability grouping for DPI, report rate, battery, buttons/remapping, lighting, and profiles.
- Added a copyable HID++ probe report with raw TX/RX trace.
- Kept startup discovery passive.
- Kept all configuration/SET commands disabled.
- Kept receiver-child and A50 X protocol probing disabled pending dedicated transports.

## 0.1.1

- Separated current connection state from wireless capability.
- Distinguished physical devices, LIGHTSPEED receivers, and the A50 X base-station interface.
- Added related-interface linking.
- Added effective hidraw ACL checks.
- Added a session-scoped Logitech udev rule using `TAG+="uaccess"`.

## 0.1.0

- Initial Linux USB/HID discovery.
- Device Inspector and copyable diagnostics.
- Initial G502, G915 X, and A50 X family recognition.
- No device protocol traffic.
